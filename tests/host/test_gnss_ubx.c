/*
 * UBX-расширения приёмника u-blox M10 (gnss_ubx.c и их интеграция):
 *   - разбор кадров из потока (контрольная сумма, мусор, длинные кадры, повторная синхронизация);
 *   - UBX-NAV-PVT -> решение (позиция, высота, скорость, курс, время, фикс, hAcc/vAcc/sAcc);
 *   - приоритет PVT над NMEA и возврат к NMEA при пропаже PVT;
 *   - UBX-NAV-ODO, SEC-SIG, MON-RF, MON-VER и дистанция трека (UBX-NAV-RESETODO);
 *   - стартовая настройка приёмника (отдельные VALSET, опрос MON-VER);
 *   - фильтр точек по hAcc и теги GPX (Garmin TrackPointExtension, pg:hacc);
 *   - снимок путевого компьютера раз в секунду и экран Trip computer.
 * Части, зависящие от приложения, проверяются только при PURRGO_GNSS_UBX = 1.
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/app_ui.h"
#include "purrgo/config.h"
#include "purrgo/gnss_adapter.h"
#include "purrgo/gnss_receiver.h"
#include "purrgo/gnss_ubx.h"
#include "purrgo/hardware_config.h"
#include "purrgo/track_logger.h"
#include "purrgo/trip_computer.h"
#include "ui_trip.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

/* ------------------------------------------------------------------------- */
/* Построение кадров                                                         */
/* ------------------------------------------------------------------------- */

static void w16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void w32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

typedef struct {
    uint8_t valid, fix_type, flags, num_sv;
    uint16_t flags3, pdop;
    int32_t lat, lon, hmsl_mm, gspeed, head_mot;
    uint32_t hacc, vacc, sacc, head_acc;
    uint16_t year; uint8_t month, day, hour, min, sec;
} pvt_t;

static pvt_t good_pvt(void)
{
    pvt_t v;
    memset(&v, 0, sizeof v);
    v.valid = 0x03; v.fix_type = 3; v.flags = 0x01; v.num_sv = 11; v.pdop = 150;
    v.lat = 507654321; v.lon = 98765432; v.hmsl_mm = 123456;
    v.gspeed = 2500; v.head_mot = 12345678;
    v.hacc = 3500; v.vacc = 5200; v.sacc = 300; v.head_acc = 200000;
    v.year = 2026; v.month = 10; v.day = 19; v.hour = 12; v.min = 34; v.sec = 56;
    return v;
}

static void pvt_payload(const pvt_t *v, uint8_t p[92])
{
    memset(p, 0, 92);
    w16(&p[4], v->year); p[6] = v->month; p[7] = v->day; p[8] = v->hour; p[9] = v->min; p[10] = v->sec;
    p[11] = v->valid; p[20] = v->fix_type; p[21] = v->flags; p[23] = v->num_sv;
    w32(&p[24], (uint32_t)v->lon); w32(&p[28], (uint32_t)v->lat); w32(&p[36], (uint32_t)v->hmsl_mm);
    w32(&p[40], v->hacc); w32(&p[44], v->vacc); w32(&p[60], (uint32_t)v->gspeed);
    w32(&p[64], (uint32_t)v->head_mot); w32(&p[68], v->sacc); w32(&p[72], v->head_acc);
    w16(&p[76], v->pdop); w16(&p[78], v->flags3);
}

static size_t make_frame(uint8_t *out, size_t size, uint8_t cls, uint8_t id, const uint8_t *pl, uint16_t len)
{
    return purrgo_ubx_build_frame(out, size, cls, id, pl, len);
}

static size_t pvt_frame(const pvt_t *v, uint8_t *out, size_t size)
{
    uint8_t p[92];
    pvt_payload(v, p);
    return make_frame(out, size, 0x01, 0x07, p, 92);
}

static size_t odo_frame(uint32_t dist, uint8_t *out, size_t size)
{
    uint8_t p[20];
    memset(p, 0, sizeof p);
    w32(&p[8], dist); w32(&p[12], dist + 1000U); w32(&p[16], 3U);
    return make_frame(out, size, 0x01, 0x09, p, 20);
}

static size_t sec_frame(uint8_t jam, uint8_t spf, uint8_t *out, size_t size)
{
    uint8_t p[4] = { 2, (uint8_t)(0x01 | (jam << 1) | 0x08 | (spf << 4)), 0, 0 };
    return make_frame(out, size, 0x27, 0x09, p, 4);
}

static size_t rf_frame(uint8_t ant, uint16_t agc, uint8_t cw, uint8_t *out, size_t size)
{
    uint8_t p[28];
    memset(p, 0, sizeof p);
    p[1] = 1;
    p[4 + 2] = ant; p[4 + 3] = 1;
    w16(&p[4 + 12], 90); w16(&p[4 + 14], agc); p[4 + 16] = cw;
    return make_frame(out, size, 0x0A, 0x38, p, 28);
}

static void nmea(char *out, size_t size, const char *body)
{
    unsigned cs = 0;
    for (const char *p = body; *p; p++) { cs ^= (unsigned char)*p; }
    snprintf(out, size, "$%s*%02X\r\n", body, cs);
}

/* ------------------------------------------------------------------------- */
/* Разбор кадров                                                             */
/* ------------------------------------------------------------------------- */

static purrgo_ubx_feed_t feed_all(purrgo_ubx_parser_t *p, const uint8_t *d, size_t n)
{
    purrgo_ubx_feed_t last = PURRGO_UBX_FEED_NOT_UBX;
    for (size_t i = 0; i < n; i++) { last = purrgo_ubx_parser_feed(p, d[i]); }
    return last;
}

static void test_parser(void)
{
    purrgo_ubx_parser_t p;
    uint8_t f[128], g[128];
    pvt_t v = good_pvt();
    size_t n = pvt_frame(&v, f, sizeof f);

    purrgo_ubx_parser_init(&p);
    CHECK(n == 100);
    for (size_t i = 0; i + 1 < n; i++) {
        CHECK(purrgo_ubx_parser_feed(&p, f[i]) == PURRGO_UBX_FEED_CONSUMED);
    }
    CHECK(purrgo_ubx_parser_active(&p));
    CHECK(purrgo_ubx_parser_feed(&p, f[n - 1]) == PURRGO_UBX_FEED_FRAME);
    CHECK(!purrgo_ubx_parser_active(&p));
    CHECK(p.msg_class == 0x01 && p.msg_id == 0x07 && p.length == 92);
    CHECK(p.payload[20] == 3 && p.payload[23] == 11);

    /* Битая контрольная сумма: кадр отброшен, следующий принимается. */
    memcpy(g, f, n);
    g[n - 1] ^= 0x5A;
    CHECK(feed_all(&p, g, n) == PURRGO_UBX_FEED_CONSUMED);
    CHECK(!purrgo_ubx_parser_active(&p));
    CHECK(feed_all(&p, f, n) == PURRGO_UBX_FEED_FRAME);
    memcpy(g, f, n);
    g[n - 2] ^= 0x5A;                                   /* битая CK_A: кадр сброшен сразу */
    CHECK(feed_all(&p, g, n) != PURRGO_UBX_FEED_FRAME);
    CHECK(!purrgo_ubx_parser_active(&p));

    /* Текст NMEA и посторонние байты - не UBX (отдаются NMEA-разборщику). */
    CHECK(purrgo_ubx_parser_feed(&p, '$') == PURRGO_UBX_FEED_NOT_UBX);
    CHECK(purrgo_ubx_parser_feed(&p, 'G') == PURRGO_UBX_FEED_NOT_UBX);
    CHECK(purrgo_ubx_parser_feed(&p, '\n') == PURRGO_UBX_FEED_NOT_UBX);
    CHECK(purrgo_ubx_parser_feed(&p, 0xB5) == PURRGO_UBX_FEED_CONSUMED);   /* возможное начало кадра */
    CHECK(purrgo_ubx_parser_feed(&p, '$') == PURRGO_UBX_FEED_NOT_UBX);     /* не 0x62: разбор сброшен, байт - NMEA */
    CHECK(!purrgo_ubx_parser_active(&p));

    /* B5 B5 62 ...: первый B5 был мусором. */
    CHECK(purrgo_ubx_parser_feed(&p, 0xB5) == PURRGO_UBX_FEED_CONSUMED);
    CHECK(feed_all(&p, f, n) == PURRGO_UBX_FEED_FRAME);

    /* Слишком длинный кадр (длина 0xFFFF): разбор не ждёт его конца. */
    const uint8_t huge[] = { 0xB5, 0x62, 0x0A, 0x04, 0xFF, 0xFF };
    CHECK(feed_all(&p, huge, sizeof huge) == PURRGO_UBX_FEED_NOT_UBX);
    CHECK(!purrgo_ubx_parser_active(&p));

    /* Кадр без payload (опрос MON-VER) и два кадра подряд. */
    uint8_t poll[8];
    CHECK(make_frame(poll, sizeof poll, 0x0A, 0x04, NULL, 0) == 8);
    CHECK(feed_all(&p, poll, 8) == PURRGO_UBX_FEED_FRAME && p.length == 0);
    CHECK(feed_all(&p, poll, 8) == PURRGO_UBX_FEED_FRAME);
    CHECK(feed_all(&p, f, n) == PURRGO_UBX_FEED_FRAME);
}

/* ------------------------------------------------------------------------- */
/* UBX-NAV-PVT -> решение                                                    */
/* ------------------------------------------------------------------------- */

static bool apply(const pvt_t *v, purrgo_gnss_solution_t *s)
{
    uint8_t p[92];
    pvt_payload(v, p);
    return purrgo_gnss_ubx_apply_pvt(p, 92, s);
}

static void test_pvt_mapping(void)
{
    purrgo_gnss_solution_t s;
    pvt_t v = good_pvt();

    memset(&s, 0, sizeof s);
    CHECK(apply(&v, &s));
    CHECK(s.valid && s.fix_type == 3 && s.fix_quality == 1 && s.satellites_tracked == 11);
    CHECK(s.lat_1e7 == 507654321 && s.lon_1e7 == 98765432);
    CHECK(s.alt_m == 123);                              /* hMSL 123,456 м -> 123 */
    CHECK(s.speed_knots == 485);                        /* 2,5 м/с = 4,86 уз -> 485 */
    CHECK(s.course_valid && s.course_deg_100 == 12345); /* 123,45678 градуса */
    CHECK(s.year == 26 && s.month == 10 && s.day == 19 && s.hours == 12 && s.minutes == 34 && s.seconds == 56);
    CHECK(s.pdop_100 == 150);
    CHECK(s.acc_valid && s.hacc_mm == 3500 && s.vacc_mm == 5200 && s.sacc_mm_s == 300);

    /* Высота: округление к ближайшему, отрицательная симметрично. */
    v.hmsl_mm = -430500; CHECK(apply(&v, &s) && s.alt_m == -431);
    v.hmsl_mm = -430499; CHECK(apply(&v, &s) && s.alt_m == -430);
    v.hmsl_mm = 499;     CHECK(apply(&v, &s) && s.alt_m == 0);
    v.hmsl_mm = 500;     CHECK(apply(&v, &s) && s.alt_m == 1);

    /* Курс: только при движении и приличной оценке. */
    v = good_pvt(); v.gspeed = 400;
    CHECK(apply(&v, &s) && !s.course_valid);
    v = good_pvt(); v.head_acc = 5000000;
    CHECK(apply(&v, &s) && !s.course_valid);
    v = good_pvt(); v.head_mot = 36000000;               /* 360 градусов -> 0 */
    CHECK(apply(&v, &s) && s.course_valid && s.course_deg_100 == 0);

    /* Дифференциальное решение и 2D. */
    v = good_pvt(); v.flags = 0x03;
    CHECK(apply(&v, &s) && s.fix_quality == 2);
    v = good_pvt(); v.fix_type = 2;
    CHECK(apply(&v, &s) && s.valid && s.fix_type == 2);
    v = good_pvt(); v.fix_type = 4;                      /* GNSS + dead reckoning */
    CHECK(apply(&v, &s) && s.valid && s.fix_type == 3);

    /* Нет фикса: позиция невалидна, скорость, курс и точность сброшены. */
    int32_t keep_lat = s.lat_1e7;
    v = good_pvt(); v.fix_type = 0; v.flags = 0;
    CHECK(apply(&v, &s));
    CHECK(!s.valid && s.fix_type == 1 && s.fix_quality == 0 && s.speed_knots == 0);
    CHECK(!s.course_valid && !s.acc_valid);
    CHECK(s.lat_1e7 == keep_lat);                        /* координаты не затираются мусором */
    v = good_pvt(); v.flags = 0;                         /* gnssFixOK = 0 */
    CHECK(apply(&v, &s) && !s.valid && !s.acc_valid);
    v = good_pvt(); v.fix_type = 5;                      /* только время */
    CHECK(apply(&v, &s) && !s.valid);
    v = good_pvt(); v.flags3 = 0x01;                     /* invalidLlh */
    CHECK(apply(&v, &s) && !s.valid);

    /* Время не применяется без validDate и validTime. */
    memset(&s, 0, sizeof s);
    v = good_pvt(); v.valid = 0x01;
    CHECK(apply(&v, &s) && s.valid && s.year == 0 && s.hours == 0);

    /* Точность недоступна (0xFFFFFFFF) или неверная длина. */
    v = good_pvt(); v.hacc = 0xFFFFFFFFu;
    CHECK(apply(&v, &s) && !s.acc_valid);
    uint8_t p[92];
    pvt_payload(&v, p);
    CHECK(!purrgo_gnss_ubx_apply_pvt(p, 91, &s));
    CHECK(!purrgo_gnss_ubx_apply_pvt(p, 93, &s));
    CHECK(!purrgo_gnss_ubx_apply_pvt(NULL, 92, &s));
}

/* ------------------------------------------------------------------------- */
/* Приоритет PVT над NMEA                                                    */
/* ------------------------------------------------------------------------- */

static void test_pvt_over_nmea(void)
{
    purrgo_gnss_solution_t s;
    char line[160];
    pvt_t v = good_pvt();
    uint8_t p[92];

    memset(&s, 0, sizeof s);
    purrgo_gnss_adapter_reset();
    purrgo_gnss_ubx_init();

    /* До первого PVT владеет NMEA. */
    nmea(line, sizeof line, "GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W");
    CHECK(purrgo_gnss_process_nmea(line, &s));
    CHECK(s.valid && s.lat_1e7 / 100000 == 4811);       /* 48,1173 градуса */

    v.fix_type = 2;                                      /* 2D: отличим от 3D из GSA */
    pvt_payload(&v, p);
    CHECK(purrgo_gnss_ubx_process_frame(0x01, 0x07, p, 92, &s, g_now));
    CHECK(s.lat_1e7 == 507654321 && s.fix_type == 2);

    /* Пока PVT владеет решением: RMC только подтверждает, что приёмник жив. */
    CHECK(purrgo_gnss_process_nmea(line, &s));
    CHECK(s.lat_1e7 == 507654321 && s.lon_1e7 == 98765432 && s.speed_knots == 485);

    /* GGA: спутники, высота, качество - из PVT; HDOP - из GGA. */
    nmea(line, sizeof line, "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
    purrgo_gnss_process_nmea(line, &s);
    CHECK(s.satellites_tracked == 11 && s.alt_m == 123 && s.hdop_100 == 90);

    /* GSA: тип фикса и PDOP остаются из PVT, HDOP/VDOP берутся из GSA. */
    nmea(line, sizeof line, "GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1");
    purrgo_gnss_process_nmea(line, &s);
    CHECK(s.fix_type == 2 && s.pdop_100 == 150);
    CHECK(s.hdop_100 == 130 && s.vdop_100 == 210);

    /* PVT пропал: после PURRGO_GNSS_PVT_HOLD_EPOCHS эпох RMC решением снова владеет NMEA. */
    nmea(line, sizeof line, "GPRMC,123520,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W");
    purrgo_gnss_process_nmea(line, &s);                  /* вторая эпоха RMC после PVT */
    CHECK(s.lat_1e7 == 507654321);
    purrgo_gnss_process_nmea(line, &s);                  /* третья */
    CHECK(s.lat_1e7 == 507654321);
    CHECK(s.acc_valid && s.hacc_mm == 3500);             /* пока владеет PVT, его точность действует */
    purrgo_gnss_process_nmea(line, &s);                  /* четвёртая: удержание закончилось */
    CHECK(s.lat_1e7 / 100000 == 4811);
    CHECK(!s.acc_valid);                                 /* hAcc от старого PVT к позиции из RMC не относится */

    /* PVT снова пошёл - владелец PVT. */
    CHECK(purrgo_gnss_ubx_process_frame(0x01, 0x07, p, 92, &s, g_now));
    CHECK(s.lat_1e7 == 507654321 && s.acc_valid);

    /* PVT без фикса переводит решение в невалидное, NMEA этого не перебивает. */
    v = good_pvt(); v.fix_type = 0; v.flags = 0;
    pvt_payload(&v, p);
    purrgo_gnss_ubx_process_frame(0x01, 0x07, p, 92, &s, g_now);
    nmea(line, sizeof line, "GPRMC,123521,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W");
    purrgo_gnss_process_nmea(line, &s);
    CHECK(!s.valid);

    purrgo_gnss_adapter_reset();
}

/* ------------------------------------------------------------------------- */
/* SEC-SIG, MON-RF, ODO, MON-VER                                             */
/* ------------------------------------------------------------------------- */

static void process(const uint8_t *f, size_t n, purrgo_gnss_solution_t *s, uint32_t now)
{
    purrgo_ubx_parser_t p;
    purrgo_ubx_parser_init(&p);
    CHECK(feed_all(&p, f, n) == PURRGO_UBX_FEED_FRAME);
    (void)purrgo_gnss_ubx_process_frame(p.msg_class, p.msg_id, p.payload, p.length, s, now);
}

static void test_status_messages(void)
{
    purrgo_gnss_solution_t s;
    uint8_t f[128];
    size_t n;

    memset(&s, 0, sizeof s);
    purrgo_gnss_ubx_init();
    const purrgo_gnss_ubx_status_t *st = purrgo_gnss_ubx_status();
    CHECK(!st->odo_valid && !st->sec_valid && !st->rf_valid && !st->ver_valid);

    /* SEC-SIG: глушение и спуфинг. */
    n = sec_frame(PURRGO_UBX_JAM_WARNING, PURRGO_UBX_SPOOF_AFFIRMED, f, sizeof f);
    process(f, n, &s, 1000);
    CHECK(st->sec_valid && st->jam_det_enabled && st->spoof_det_enabled);
    CHECK(st->jam_state == PURRGO_UBX_JAM_WARNING && st->spoof_state == PURRGO_UBX_SPOOF_AFFIRMED);
    n = sec_frame(PURRGO_UBX_JAM_NONE, PURRGO_UBX_SPOOF_NONE, f, sizeof f);
    process(f, n, &s, 2000);
    CHECK(st->jam_state == PURRGO_UBX_JAM_NONE && st->spoof_state == PURRGO_UBX_SPOOF_NONE && st->sec_ms == 2000);
    {   /* версия, отличная от 2, игнорируется */
        uint8_t old[4] = { 1, 0x3F, 0, 0 };
        n = make_frame(f, sizeof f, 0x27, 0x09, old, 4);
        process(f, n, &s, 3000);
        CHECK(st->sec_ms == 2000);
    }

    /* MON-RF. */
    n = rf_frame(PURRGO_UBX_ANT_SHORT, 4096, 128, f, sizeof f);
    process(f, n, &s, 1500);
    CHECK(st->rf_valid && st->ant_status == PURRGO_UBX_ANT_SHORT && st->ant_power == 1);
    CHECK(st->agc_cnt == 4096 && st->cw_suppression == 128 && st->noise_per_ms == 90);

    /* Свежесть: сообщение старше PURRGO_UBX_STALE_MS считается устаревшим. */
    CHECK(purrgo_gnss_ubx_fresh(1000, 1000 + PURRGO_UBX_STALE_MS));
    CHECK(!purrgo_gnss_ubx_fresh(1000, 1000 + PURRGO_UBX_STALE_MS + 1));
    CHECK(purrgo_gnss_ubx_fresh(0xFFFFFF00u, 0x100u));   /* переход через 0 счётчика мс */

    /* MON-VER: строки без нуля в конце не выходят за поле. */
    {
        uint8_t ver[70];
        memset(ver, 'A', sizeof ver);
        n = make_frame(f, sizeof f, 0x0A, 0x04, ver, sizeof ver);
        process(f, n, &s, 5000);
        CHECK(st->ver_valid && strlen(st->sw_version) == 30 && strlen(st->hw_version) == 10);

        memset(ver, 0, sizeof ver);
        memcpy(&ver[0], "EXT CORE 5.30 (abcdef)", 22);
        memcpy(&ver[30], "000A0000", 8);
        memcpy(&ver[40], "PROTVER=34.30", 13);
        memcpy(&ver[70 - 30 + 0], "MOD=G10A-F30", 12);
        n = make_frame(f, sizeof f, 0x0A, 0x04, ver, sizeof ver);
        process(f, n, &s, 20000);
        CHECK(strcmp(st->sw_version, "EXT CORE 5.30 (abcdef)") == 0 && strcmp(st->hw_version, "000A0000") == 0);
    }

    /* Короткие и чужие кадры не ломают состояние. */
    {
        uint8_t shortp[3] = { 0 };
        CHECK(!purrgo_gnss_ubx_process_frame(0x01, 0x09, shortp, 3, &s, 6000));
        CHECK(!purrgo_gnss_ubx_process_frame(0x0A, 0x38, shortp, 3, &s, 6000));
        CHECK(!purrgo_gnss_ubx_process_frame(0x05, 0x00, shortp, 2, &s, 6000));   /* NAK: только лог */
        CHECK(!purrgo_gnss_ubx_process_frame(0x99, 0x99, shortp, 3, &s, 6000));
        CHECK(!purrgo_gnss_ubx_process_frame(0x01, 0x07, NULL, 0, &s, 6000));
    }
}

static bool odo_feed(uint32_t dist, uint32_t now)
{
    purrgo_gnss_solution_t s;
    uint8_t f[64];
    size_t n = odo_frame(dist, f, sizeof f);
    memset(&s, 0, sizeof s);
    process(f, n, &s, now);
    return n == 28;
}

static void test_trip_distance(void)
{
    uint32_t d = 0;
    size_t len;
    static const uint8_t reset_frame[8] = { 0xB5, 0x62, 0x01, 0x10, 0x00, 0x00, 0x11, 0x34 };

    /* Обычный ход: сброс отправлен, модуль обнулил счётчик. */
    purrgo_gnss_ubx_init(); fake_gnss_tx_reset();
    CHECK(!purrgo_gnss_ubx_trip_distance(1000, &d));     /* данных ещё нет */
    CHECK(odo_feed(500, 1000));
    CHECK(purrgo_gnss_ubx_trip_distance(1000, &d) && d == 500);
    purrgo_gnss_ubx_reset_trip(2000);
    CHECK(fake_gnss_tx_frames() == 1);
    CHECK(memcmp(fake_gnss_tx_data(&len), reset_frame, 8) == 0 && len == 8);
    CHECK(purrgo_gnss_ubx_trip_distance(2000, &d) && d == 0);   /* сразу 0, не ждём кадр модуля */
    CHECK(odo_feed(510, 2500));                          /* кадр до сброса: внутри окна, игнорируется */
    CHECK(purrgo_gnss_ubx_trip_distance(2500, &d) && d == 0);
    CHECK(odo_feed(3, 3600));                            /* модуль обнулился */
    CHECK(purrgo_gnss_ubx_trip_distance(3600, &d) && d == 3);
    CHECK(odo_feed(20, 4600));
    CHECK(purrgo_gnss_ubx_trip_distance(4600, &d) && d == 20);
    CHECK(purrgo_gnss_ubx_status()->odo_total_m == 1020 && purrgo_gnss_ubx_status()->odo_std_m == 3);

    /* Устаревшие данные: ODO перестал приходить. */
    CHECK(!purrgo_gnss_ubx_trip_distance(4600 + PURRGO_UBX_STALE_MS + 1, &d));
    CHECK(purrgo_gnss_ubx_trip_distance(4600 + PURRGO_UBX_STALE_MS, &d));

    /* Команду отправить нельзя (GNSS выключен): считаем от текущего значения одометра. */
    purrgo_gnss_ubx_init(); fake_gnss_tx_reset(); fake_gnss_tx_set_fail(true);
    CHECK(odo_feed(800, 1000));
    purrgo_gnss_ubx_reset_trip(2000);
    CHECK(fake_gnss_tx_frames() == 0);
    CHECK(purrgo_gnss_ubx_trip_distance(2000, &d) && d == 0);
    CHECK(odo_feed(810, 2100));                          /* окна ожидания нет: модуль не сбрасывался */
    CHECK(purrgo_gnss_ubx_trip_distance(2100, &d) && d == 10);
    CHECK(odo_feed(900, 3100));
    CHECK(purrgo_gnss_ubx_trip_distance(3100, &d) && d == 100);
    fake_gnss_tx_set_fail(false);

    /* Сброс до первого ODO и успешная отправка: отсчёт от нуля модуля. */
    purrgo_gnss_ubx_init(); fake_gnss_tx_reset();
    purrgo_gnss_ubx_reset_trip(1000);
    CHECK(fake_gnss_tx_frames() == 1);
    CHECK(odo_feed(7, 3000));
    CHECK(purrgo_gnss_ubx_trip_distance(3000, &d) && d == 7);

    /* Сброс до первого ODO и отказ отправки: база - первое значение. */
    purrgo_gnss_ubx_init(); fake_gnss_tx_reset(); fake_gnss_tx_set_fail(true);
    purrgo_gnss_ubx_reset_trip(1000);
    CHECK(odo_feed(900, 1500));
    CHECK(purrgo_gnss_ubx_trip_distance(1500, &d) && d == 0);
    CHECK(odo_feed(905, 2500));
    CHECK(purrgo_gnss_ubx_trip_distance(2500, &d) && d == 5);
    fake_gnss_tx_set_fail(false);

    /* Модуль перезапущен (холодный старт): счётчик упал - база сброшена. */
    purrgo_gnss_ubx_init(); fake_gnss_tx_reset();
    CHECK(odo_feed(5000, 1000));
    purrgo_gnss_ubx_reset_trip(2000);
    CHECK(odo_feed(5000, 4000));                         /* окно закончилось, модуль не сбросился */
    CHECK(purrgo_gnss_ubx_trip_distance(4000, &d) && d == 0);
    CHECK(odo_feed(12, 5000));
    CHECK(purrgo_gnss_ubx_trip_distance(5000, &d) && d == 12);

    /* Опрос версии. */
    fake_gnss_tx_reset();
    static const uint8_t poll_frame[8] = { 0xB5, 0x62, 0x0A, 0x04, 0x00, 0x00, 0x0E, 0x34 };
    CHECK(purrgo_gnss_ubx_poll_version());
    CHECK(memcmp(fake_gnss_tx_data(&len), poll_frame, 8) == 0 && len == 8);
    fake_gnss_tx_set_fail(true);
    CHECK(!purrgo_gnss_ubx_poll_version());
    fake_gnss_tx_set_fail(false);
}

/* ------------------------------------------------------------------------- */
/* Стартовая настройка приёмника                                             */
/* ------------------------------------------------------------------------- */

static uint8_t g_cfg[8][128];
static size_t g_cfg_len[8];
static int g_cfg_count;

static void cfg_tx(const uint8_t *data, size_t len)
{
    if (g_cfg_count < 8 && len <= sizeof g_cfg[0]) {
        memcpy(g_cfg[g_cfg_count], data, len);
        g_cfg_len[g_cfg_count++] = len;
    }
}

/* Значение ключа в кадре VALSET; false, если ключа нет. Все ключи расширений - 1 байт. */
static bool valset_value(int frame_idx, uint32_t key, uint32_t *value)
{
    const uint8_t *f = g_cfg[frame_idx];
    const size_t len = g_cfg_len[frame_idx];
    size_t pos = 10;

    while (pos + 5 <= len - 2) {
        uint32_t k = (uint32_t)f[pos] | ((uint32_t)f[pos + 1] << 8) | ((uint32_t)f[pos + 2] << 16) |
                     ((uint32_t)f[pos + 3] << 24);
        size_t vs = (((k >> 28) & 7U) == 3U) ? 2U : (((k >> 28) & 7U) == 4U ? 4U : 1U);
        if (k == key) { *value = f[pos + 4]; return true; }
        pos += 4 + vs;
    }
    return false;
}

static void test_configure_frames(void)
{
#if PURRGO_GNSS_UBX
    uint32_t v = 99;
    /* Кадры: 0 NMEA, 1 PVT, 2 ODO, 3 MON-RF, [SEC-SIG], затем одометр, модель, опрос версии. */
    const int sig = PURRGO_GNSS_SEC_SIG;
    const int odo_cfg = 4 + sig, dyn = 5 + sig, poll = 6 + sig;

    g_cfg_count = 0;
    purrgo_gnss_receiver_configure_ubx(cfg_tx);
    CHECK(g_cfg_count == 7 + sig);

    for (int i = 0; i < poll; i++) {                     /* все VALSET: слой - PURRGO_GNSS_UBX_LAYERS */
        CHECK(g_cfg[i][2] == 0x06 && g_cfg[i][3] == 0x8A && g_cfg[i][7] == (uint8_t)PURRGO_GNSS_UBX_LAYERS);
    }
    /* 0: базовая настройка не изменилась. */
    CHECK(valset_value(0, 0x209100BBU, &v) && v == 1);   /* GGA */
    CHECK(valset_value(0, 0x209100B1U, &v) && v == 0);   /* VTG выключен */
    CHECK(!valset_value(0, 0x20910007U, &v));            /* PVT не в базовом кадре */
    /* 1..3: каждое бинарное сообщение отдельным кадром: отказ по одному ключу не отменяет остальные. */
    CHECK(valset_value(1, 0x20910007U, &v) && v == 1);   /* NAV-PVT */
    CHECK(!valset_value(1, 0x2091007FU, &v));
    CHECK(valset_value(2, 0x2091007FU, &v) && v == 1);   /* NAV-ODO */
    CHECK(valset_value(3, 0x2091035AU, &v) && v == 1);   /* MON-RF */
#if PURRGO_GNSS_SEC_SIG
    CHECK(valset_value(4, 0x20910635U, &v) && v == 1);   /* SEC-SIG (только SPG 5.30+) */
#else
    for (int i = 0; i < g_cfg_count; i++) {              /* в SPG 5.10 ключа нет: не отправляется вовсе */
        CHECK(!valset_value(i, 0x20910635U, &v));
    }
#endif
    /* Одометр включён, профиль из конфигурации. */
    CHECK(valset_value(odo_cfg, 0x10220001U, &v) && v == 1);
    CHECK(valset_value(odo_cfg, 0x20220005U, &v) && v == (uint32_t)PURRGO_GNSS_ODO_PROFILE);
    /* Модель динамики - отдельным кадром (отказ не отменяет остальное). */
    CHECK(valset_value(dyn, 0x20110021U, &v) && v == (uint32_t)PURRGO_GNSS_DYNMODEL);
    CHECK(!valset_value(dyn, 0x10220001U, &v));
    /* Опрос версии. */
    static const uint8_t poll_frame[8] = { 0xB5, 0x62, 0x0A, 0x04, 0x00, 0x00, 0x0E, 0x34 };
    CHECK(g_cfg_len[poll] == 8 && memcmp(g_cfg[poll], poll_frame, 8) == 0);

    /*
     * Ответы ACK/NAK на VALSET приходят по порядку отправки: NAK приписывается группе
     * по номеру. Здесь отклонена вторая группа (NAV-ODO) и последняя (DYNMODEL).
     */
    const int groups = 6 + sig;
    purrgo_gnss_solution_t sol;
    memset(&sol, 0, sizeof sol);
    CHECK(purrgo_gnss_ubx_cfg_replies() == 0 && purrgo_gnss_ubx_cfg_nak_mask() == 0);
    for (int i = 0; i < groups; i++) {
        const bool nak = (i == 2) || (i == groups - 1);
        uint8_t ack[2] = { 0x06, 0x8A };
        CHECK(!purrgo_gnss_ubx_process_frame(0x05, nak ? 0x00 : 0x01, ack, 2, &sol, 1000));
    }
    CHECK(purrgo_gnss_ubx_cfg_replies() == (uint8_t)groups);
    CHECK(purrgo_gnss_ubx_cfg_nak_mask() == ((1UL << 2) | (1UL << (groups - 1))));
    {   /* лишние ответы (не от этой серии) ничего не портят */
        uint8_t nak[2] = { 0x06, 0x8A };
        (void)purrgo_gnss_ubx_process_frame(0x05, 0x00, nak, 2, &sol, 1000);
        CHECK(purrgo_gnss_ubx_cfg_replies() == (uint8_t)groups);
        CHECK(purrgo_gnss_ubx_cfg_nak_mask() == ((1UL << 2) | (1UL << (groups - 1))));
    }
    /* Повторная настройка начинает отсчёт заново. */
    g_cfg_count = 0;
    purrgo_gnss_receiver_configure_ubx(cfg_tx);
    CHECK(purrgo_gnss_ubx_cfg_replies() == 0 && purrgo_gnss_ubx_cfg_nak_mask() == 0);
#else
    g_cfg_count = 0;
    purrgo_gnss_receiver_configure_ubx(cfg_tx);
    CHECK(g_cfg_count == 1);                             /* только NMEA-настройка */
#endif
}

/* ------------------------------------------------------------------------- */
/* Логгер: колбэк нового трека, фильтр hAcc, теги GPX                        */
/* ------------------------------------------------------------------------- */

#define LAT0 537135000
#define LON0 284199000

static purrgo_gnss_solution_t mk(uint32_t t)
{
    purrgo_gnss_solution_t f;
    memset(&f, 0, sizeof f);
    f.valid = true; f.year = 26; f.month = 10; f.day = 19;
    f.hours = (uint8_t)(t / 3600U); f.minutes = (uint8_t)((t / 60U) % 60U); f.seconds = (uint8_t)(t % 60U);
    f.lat_1e7 = LAT0; f.lon_1e7 = LON0; f.alt_m = 100;
    f.speed_knots = 500;
    return f;
}

static char *finish_and_read(void)
{
    char name[64];
    snprintf(name, sizeof name, "%s", purrgo_logger_get_active_filename());
    purrgo_logger_stop();
    uint32_t sz = 0;
    const uint8_t *d = fake_fs_data(name, &sz);
    if (d == NULL) { return NULL; }
    char *t = malloc((size_t)sz + 1u);
    memcpy(t, d, sz);
    t[sz] = 0;
    return t;
}

static void logger_fresh(void)
{
    purrgo_logger_reset();
    fake_fs_reset();
    memset(&app_config, 0, sizeof app_config);
    app_config.tz_offset_minutes = 0;
}

static int g_new_track_calls;
static void on_track(void) { g_new_track_calls++; }

static void test_logger_hacc_and_gpx(void)
{
    purrgo_gnss_solution_t f;

    /* Фильтр: hAcc выше порога - точка не пишется, ниже - пишется. */
    logger_fresh();
    f = mk(0);
    CHECK(purrgo_logger_start(&f));
    f.acc_valid = true; f.hacc_mm = PURRGO_GNSS_TRACK_MAX_HACC_M * 1000U + 1U;
    CHECK(!purrgo_logger_add_point(&f));
    f.hacc_mm = PURRGO_GNSS_TRACK_MAX_HACC_M * 1000U;
    CHECK(purrgo_logger_add_point(&f));
    purrgo_logger_stop();

    /* При известной hAcc HDOP не учитывается: hAcc точнее. */
    logger_fresh();
    f = mk(0);
    f.acc_valid = true; f.hacc_mm = 4000; f.hdop_100 = 900;
    CHECK(purrgo_logger_start(&f) && purrgo_logger_add_point(&f));
    purrgo_logger_stop();

    /* Без hAcc (NMEA) работает прежний фильтр по HDOP. */
    logger_fresh();
    f = mk(0);
    f.acc_valid = false; f.hacc_mm = 1000; f.hdop_100 = 600;
    CHECK(purrgo_logger_start(&f));
    CHECK(!purrgo_logger_add_point(&f));
    f.hdop_100 = 500;
    CHECK(purrgo_logger_add_point(&f));
    purrgo_logger_stop();

    /* GPX: pg:hacc и Garmin TrackPointExtension (скорость м/с, курс), порядок тегов. */
    logger_fresh();
    f = mk(0);
    f.speed_knots = 485;                                  /* 4,85 уз = 2,49 м/с */
    f.course_valid = true; f.course_deg_100 = 4500;
    f.acc_valid = true; f.hacc_mm = 3520;
    CHECK(purrgo_logger_start(&f) && purrgo_logger_add_point(&f));
    char *t = finish_and_read();
    CHECK(t != NULL);
    if (t) {
        const char *sp = strstr(t, "<gpxtpx:speed>2.49</gpxtpx:speed>");
        const char *cr = strstr(t, "<gpxtpx:course>45.00</gpxtpx:course>");
        const char *te = strstr(t, "</gpxtpx:TrackPointExtension>");
        const char *ha = strstr(t, "<pg:hacc>3.52</pg:hacc>");
        const char *ex = strstr(t, "<extensions><gpxtpx:TrackPointExtension>");
        CHECK(sp && cr && te && ha && ex);
        CHECK(ex < sp && sp < cr && cr < te && te < ha);  /* порядок схемы TrackPointExtension: speed, course */
        CHECK(strstr(t, "xmlns=\"http://www.topografix.com/GPX/1/1\"") != NULL);
        CHECK(strstr(t, "TrackPointExtension/v2\"") != NULL);
        CHECK(strstr(t, "<gpx version=\"1.1\"") != NULL);
        free(t);
    }

    /* Без hAcc и без надёжного курса этих тегов нет. */
    logger_fresh();
    f = mk(0);
    CHECK(purrgo_logger_start(&f) && purrgo_logger_add_point(&f));
    t = finish_and_read();
    if (t) {
        CHECK(!strstr(t, "pg:hacc") && !strstr(t, "gpxtpx:course"));
        CHECK(strstr(t, "<gpxtpx:speed>2.57</gpxtpx:speed>") != NULL);
        free(t);
    }

    /* Крайние значения не ломают разметку и помещаются в буфер записи. */
    logger_fresh();
    f = mk(0);
    f.speed_knots = 2000000000; f.course_valid = true; f.course_deg_100 = 35999;
    f.acc_valid = true; f.hacc_mm = 0;
    CHECK(purrgo_logger_start(&f) && purrgo_logger_add_point(&f));
    t = finish_and_read();
    if (t) {
        CHECK(strstr(t, "<gpxtpx:course>359.99</gpxtpx:course>") != NULL);
        CHECK(strstr(t, "<pg:hacc>0.00</pg:hacc>") != NULL);
        CHECK(strstr(t, "</gpx>") != NULL);
        free(t);
    }

    /* Колбэк "начат новый трек": при каждом новом файле, снятие колбэка работает. */
    logger_fresh();
    g_new_track_calls = 0;
    purrgo_logger_set_new_track_callback(on_track);
    f = mk(0);
    CHECK(purrgo_logger_start(&f));
    CHECK(g_new_track_calls == 1);
    CHECK(!purrgo_logger_start(&f));                      /* уже пишется: не новый трек */
    CHECK(g_new_track_calls == 1);
    purrgo_logger_stop();
    CHECK(purrgo_logger_start(&f));
    CHECK(g_new_track_calls == 2);
    /* Смена суток: новый файл - новый трек. */
    purrgo_gnss_solution_t next = mk(100);
    next.day = 20; next.lat_1e7 += 2000;
    next.hdop_100 = 0;
    CHECK(purrgo_logger_add_point(&next) || purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    CHECK(g_new_track_calls == 3);
    purrgo_logger_stop();
    purrgo_logger_set_new_track_callback(NULL);
    CHECK(purrgo_logger_start(&f));
    CHECK(g_new_track_calls == 3);
    purrgo_logger_stop();
}

/* ------------------------------------------------------------------------- */
/* Приложение: поток NMEA + UBX, путевой компьютер, экран                    */
/* ------------------------------------------------------------------------- */

#if PURRGO_GNSS_UBX
#define W 176
#define H 264
static uint8_t g_fb[W * H];
static void cb_draw(void *fb, int16_t x, int16_t y, gfx_color_t c)
{
    (void)fb;
    if (x >= 0 && x < W && y >= 0 && y < H) { g_fb[y * W + x] = (uint8_t)(c + 1); }
}
static gfx_color_t cb_read(void *fb, int16_t x, int16_t y)
{
    (void)fb;
    return (x >= 0 && x < W && y >= 0 && y < H) ? (gfx_color_t)(g_fb[y * W + x] - 1) : 0;
}

static void feed_bytes(const uint8_t *d, size_t n)
{
    for (size_t i = 0; i < n; i++) { purrgo_app_feed_gnss_byte(d[i]); }
}

static void feed_text(const char *s)
{
    for (; *s; s++) { purrgo_app_feed_gnss_byte((uint8_t)*s); }
}

static void run_ticks(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 100) { g_now += 100; purrgo_app_tick(g_now); }
}

static void app_setup(void)
{
    fake_fs_reset();
    fake_power_reset();
    memset(&app_config, 0, sizeof app_config);
    g_now = 200000;
    purrgo_logger_reset();
    purrgo_app_ui_reset();
    purrgo_gnss_adapter_reset();
    purrgo_app_init();
    app_config.log_mode = LOGGER_MODE_STANDARD;
    purrgo_logger_set_mode(app_config.log_mode);
}

static void test_app_stream(void)
{
    uint8_t f[128];
    char line[160];
    pvt_t v = good_pvt();
    size_t n;

    app_setup();
    run_ticks(1000);

    /* Поток вперемешку: NMEA, бинарные кадры, снова NMEA - ни один не портит другой. */
    nmea(line, sizeof line, "GPRMC,123456.00,A,4807.038,N,01131.000,E,0.50,90.00,191026,,,A");
    feed_text(line);
    n = pvt_frame(&v, f, sizeof f);
    feed_bytes(f, n);
    n = odo_frame(1234, f, sizeof f);
    feed_bytes(f, n);
    nmea(line, sizeof line, "GPGGA,123456.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
    feed_text(line);
    n = sec_frame(PURRGO_UBX_JAM_NONE, PURRGO_UBX_SPOOF_NONE, f, sizeof f);
    feed_bytes(f, n);
    n = rf_frame(PURRGO_UBX_ANT_OK, 3000, 30, f, sizeof f);
    feed_bytes(f, n);

    const purrgo_gnss_solution_t *s = purrgo_app_get_gnss_solution();
    CHECK(s->valid && s->lat_1e7 == 507654321 && s->lon_1e7 == 98765432);   /* PVT, не RMC */
    CHECK(s->acc_valid && s->hacc_mm == 3500);
    CHECK(s->hdop_100 == 90);                                                /* HDOP - из GGA */
    CHECK(purrgo_gnss_ubx_status()->odo_distance_m == 1234);
    CHECK(purrgo_gnss_ubx_status()->sec_valid && purrgo_gnss_ubx_status()->rf_valid);

    /* Битый кадр между строками NMEA не мешает следующей строке. */
    n = pvt_frame(&v, f, sizeof f);
    f[n - 1] ^= 0xFF;
    feed_bytes(f, n);
    v.lat = 510000000;
    nmea(line, sizeof line, "GPRMC,123457.00,A,4807.038,N,01131.000,E,0.50,90.00,191026,,,A");
    feed_text(line);
    CHECK(s->lat_1e7 == 507654321);                                          /* RMC ещё под PVT */

    /* Только PVT, без NMEA: приёмник жив, решение не пропадает по таймауту. */
    app_setup();
    for (int i = 0; i < 12; i++) {
        v.sec = (uint8_t)(i % 60);
        n = pvt_frame(&v, f, sizeof f);
        feed_bytes(f, n);
        run_ticks(1000);
    }
    s = purrgo_app_get_gnss_solution();
    CHECK(s->valid && s->acc_valid);
    run_ticks(7000);                                                         /* PVT пропал дольше таймаута */
    CHECK(!s->valid && !s->acc_valid);
}

static void test_app_new_track_resets_odo(void)
{
    uint8_t f[128];
    pvt_t v = good_pvt();
    size_t n, len;
    static const uint8_t reset_frame[8] = { 0xB5, 0x62, 0x01, 0x10, 0x00, 0x00, 0x11, 0x34 };

    app_setup();
    run_ticks(1000);
    n = pvt_frame(&v, f, sizeof f);
    feed_bytes(f, n);
    n = odo_frame(4000, f, sizeof f);
    feed_bytes(f, n);
    run_ticks(1000);                                      /* purrgo_app_update: старт трека */
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    CHECK(fake_gnss_tx_frames() == 1);                    /* RESETODO при начале трека */
    CHECK(memcmp(fake_gnss_tx_data(&len), reset_frame, 8) == 0);

    /* Дистанция трека: от нуля, до значений модуля после сброса. */
    uint32_t d = 0;
    CHECK(purrgo_gnss_ubx_trip_distance(g_now, &d) && d == 0);
    run_ticks(1500);
    n = odo_frame(15, f, sizeof f);
    feed_bytes(f, n);
    CHECK(purrgo_gnss_ubx_trip_distance(g_now, &d) && d == 15);
    run_ticks(1000);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    CHECK(fake_gnss_tx_frames() == 1);                    /* продолжение записи сброса не вызывает */
}

/*
 * Снимок путевого компьютера: логика округления и флага "изменилось" проверяется на
 * самом модуле (приложение само забирает флаг в purrgo_app_update).
 */
static void test_trip_info_changes(void)
{
    purrgo_gnss_solution_t s;
    uint8_t f[128];
    pvt_t v = good_pvt();
    size_t n;

    memset(&s, 0, sizeof s);
    purrgo_gnss_ubx_init();
    purrgo_trip_computer_init();
    const purrgo_trip_info_t *t = purrgo_trip_computer_info();

    g_now = 10000;
    purrgo_trip_computer_update(&s);
    CHECK(!t->dist_valid && !t->acc_valid && !t->sig_valid && !t->rf_valid);
    CHECK(!purrgo_trip_computer_take_changed());          /* ничего не пришло - ничего не изменилось */

    /* Пришли данные: снимок меняется только в update(), не по приходу байтов. */
    n = pvt_frame(&v, f, sizeof f); process(f, n, &s, g_now);
    n = odo_frame(12340, f, sizeof f); process(f, n, &s, g_now);
    n = sec_frame(PURRGO_UBX_JAM_NONE, PURRGO_UBX_SPOOF_NONE, f, sizeof f); process(f, n, &s, g_now);
    n = rf_frame(PURRGO_UBX_ANT_OK, 3000, 30, f, sizeof f); process(f, n, &s, g_now);
    CHECK(!t->dist_valid && !purrgo_trip_computer_take_changed());
    purrgo_trip_computer_update(&s);
    CHECK(t->dist_valid && t->dist_m == 12340);
    CHECK(t->acc_valid && t->hacc_dm == 35 && t->vacc_dm == 52);
    CHECK(t->sig_valid && t->jam_state == PURRGO_UBX_JAM_NONE && t->spoof_state == PURRGO_UBX_SPOOF_NONE);
    CHECK(t->rf_valid && t->ant_status == PURRGO_UBX_ANT_OK);
    CHECK(t->agc_percent == 35 && t->cw_percent == 10);   /* 3000/8191 = 36,6 % -> 35; 30/255 = 11,8 % -> 10 */
    CHECK(purrgo_trip_computer_take_changed());
    CHECK(!purrgo_trip_computer_take_changed());          /* флаг сбрасывается */

    /* Тот же снимок и изменения меньше разрешения экрана перерисовку не вызывают. */
    purrgo_trip_computer_update(&s);
    CHECK(!purrgo_trip_computer_take_changed());
    g_now += 1000;
    n = odo_frame(12345, f, sizeof f); process(f, n, &s, g_now);     /* +5 м: тот же шаг 10 м */
    n = sec_frame(PURRGO_UBX_JAM_NONE, PURRGO_UBX_SPOOF_NONE, f, sizeof f); process(f, n, &s, g_now);
    n = rf_frame(PURRGO_UBX_ANT_OK, 3010, 31, f, sizeof f); process(f, n, &s, g_now);
    purrgo_trip_computer_update(&s);
    CHECK(t->dist_m == 12345 && !purrgo_trip_computer_take_changed());
    n = odo_frame(12360, f, sizeof f); process(f, n, &s, g_now);     /* +15 м: другая строка */
    purrgo_trip_computer_update(&s);
    CHECK(purrgo_trip_computer_take_changed());

    /* Глушение, спуфинг и антенна меняют снимок. */
    n = sec_frame(PURRGO_UBX_JAM_WARNING, PURRGO_UBX_SPOOF_INDICATED, f, sizeof f); process(f, n, &s, g_now);
    purrgo_trip_computer_update(&s);
    CHECK(t->jam_state == PURRGO_UBX_JAM_WARNING && t->spoof_state == PURRGO_UBX_SPOOF_INDICATED);
    CHECK(purrgo_trip_computer_take_changed());
    n = rf_frame(PURRGO_UBX_ANT_OPEN, 7000, 200, f, sizeof f); process(f, n, &s, g_now);
    purrgo_trip_computer_update(&s);
    CHECK(t->ant_status == PURRGO_UBX_ANT_OPEN && purrgo_trip_computer_take_changed());

    /* Потеря фикса убирает точность; устаревшие сообщения - остальное. */
    s.valid = false;
    purrgo_trip_computer_update(&s);
    CHECK(!t->acc_valid && purrgo_trip_computer_take_changed());
    g_now += PURRGO_UBX_STALE_MS + 1U;
    purrgo_trip_computer_update(&s);
    CHECK(!t->dist_valid && !t->sig_valid && !t->rf_valid && purrgo_trip_computer_take_changed());
}

static void test_trip_snapshot_and_screen(void)
{
    uint8_t f[128];
    pvt_t v = good_pvt();
    size_t n;
    gfx_context_t g;

    app_setup();
    app_config.log_mode = LOGGER_MODE_OFF;                /* трек не пишем: сброса одометра нет */
    purrgo_logger_set_mode(app_config.log_mode);
    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));
    run_ticks(1000);

    const purrgo_trip_info_t *t = purrgo_trip_computer_info();
    CHECK(!t->dist_valid && !t->sig_valid && !t->rf_valid);

    n = pvt_frame(&v, f, sizeof f); feed_bytes(f, n);
    n = odo_frame(12340, f, sizeof f); feed_bytes(f, n);
    n = sec_frame(PURRGO_UBX_JAM_NONE, PURRGO_UBX_SPOOF_NONE, f, sizeof f); feed_bytes(f, n);
    n = rf_frame(PURRGO_UBX_ANT_OK, 3000, 30, f, sizeof f); feed_bytes(f, n);

    /* Снимок обновляется на тике приложения (раз в секунду), а не по приходу байтов. */
    CHECK(!t->dist_valid);
    run_ticks(1000);
    CHECK(t->dist_valid && t->dist_m == 12340);
    CHECK(t->acc_valid && t->hacc_dm == 35 && t->sig_valid && t->rf_valid);

    /* Экран Trip computer: строки UBX рисуются со значениями и без них (проверка под ASan). */
    purrgo_gnss_solution_t sol = *purrgo_app_get_gnss_solution();
    ui_trip_render_grid(&g);
    ui_trip_render_values(&g, &sol, NULL);
    const uint8_t bg = g_fb[240 * W + 175];
    int ink = 0;
    for (int y = 190; y < 236; y++) {                     /* строки TRP..RF */
        for (int x = 0; x < W; x++) { if (g_fb[y * W + x] != bg) { ink++; } }
    }
    CHECK(ink > 100);
    run_ticks(6000);                                      /* кадры перестали приходить: данные протухли */
    CHECK(!t->dist_valid && !t->sig_valid && !t->rf_valid);
    ui_trip_render_values(&g, &sol, NULL);
}
#endif

int main(void)
{
    test_parser();
    test_pvt_mapping();
    test_pvt_over_nmea();
    test_status_messages();
    test_trip_distance();
    test_configure_frames();
    test_logger_hacc_and_gpx();
#if PURRGO_GNSS_UBX
    test_app_stream();
    test_app_new_track_resets_odo();
    test_trip_info_changes();
    test_trip_snapshot_and_screen();
#endif
    return test_finish("test_gnss_ubx");
}
