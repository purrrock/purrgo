/*
 * Host-тесты последнего известного времени UTC (purrgo/utc_hint.h):
 *   - кадры UBX-MGA-INI-TIME_UTC и POS_LLH (побайтно, контрольная сумма);
 *   - оценка времени: время приёмника в сеансе плюс прошедшее, монотонность;
 *   - проверка времени приёмника: время раньше известного (с допуском) отбрасывается;
 *   - PURRGO.CFG: LAST_UTC пишется при сохранении, читается при загрузке, не затирается без времени в
 *     сеансе, мусорное значение игнорируется;
 *   - подсказка приёмнику: только если приёмник жив, но времени не знает; один раз за период; время из
 *     PURRGO.CFG (грубо) после включения и время сеанса (точно) после перезапуска приёмника.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gnss/gnss_feed.h"
#include "purrgo/config.h"
#include "purrgo/gnss_receiver.h"
#include "purrgo/gnss_ubx.h"
#include "purrgo/purrgo_time.h"
#include "purrgo/utc_hint.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

static const char *CFG = "PURRGO.CFG";

static void w16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void w32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint32_t r32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t ep(unsigned y, unsigned mo, unsigned d, unsigned h, unsigned mi, unsigned s)
{
    uint32_t e = 0;
    if (!purrgo_time_datetime_to_epoch((uint8_t)(y - 2000U), (uint8_t)mo, (uint8_t)d, (uint8_t)h,
                                       (uint8_t)mi, (uint8_t)s, &e)) {
        printf("bad test date\n");
        exit(2);
    }
    return e;
}

/* Контрольная сумма Fletcher-8, независимая от purrgo_ubx_build_frame(). */
static bool frame_ok(const uint8_t *f, size_t len, uint8_t cls, uint8_t id, uint16_t plen)
{
    if (len != (size_t)plen + 8U || f[0] != 0xB5 || f[1] != 0x62 || f[2] != cls || f[3] != id ||
        f[4] != (plen & 0xFF) || f[5] != (plen >> 8)) {
        return false;
    }
    uint8_t a = 0, b = 0;
    for (size_t i = 2; i < 6U + plen; i++) { a = (uint8_t)(a + f[i]); b = (uint8_t)(b + a); }
    return f[6U + plen] == a && f[7U + plen] == b;
}

/* ---------- кадры PVT через feed (как в приборе) ---------- */

static purrgo_gnss_feed_t feed;

static void feed_frame(uint8_t cls, uint8_t id, const uint8_t *pl, uint16_t len)
{
    uint8_t f[128];
    const size_t n = purrgo_ubx_build_frame(f, sizeof f, cls, id, pl, len);
    for (size_t i = 0; i < n; i++) { purrgo_gnss_feed_byte(&feed, f[i]); }
}

/* known: приёмник знает время (validDate+validTime, tAcc 50 нс) и дату из аргументов; иначе validTime = 0. */
static void feed_pvt(bool known, unsigned y, unsigned mo, unsigned d, unsigned h, unsigned mi, unsigned s,
                     bool fix, int32_t lat, int32_t lon)
{
    uint8_t p[92];
    memset(p, 0, sizeof p);
    if (known) {
        w16(&p[4], y); p[6] = (uint8_t)mo; p[7] = (uint8_t)d; p[8] = (uint8_t)h; p[9] = (uint8_t)mi;
        p[10] = (uint8_t)s; p[11] = 0x03; w32(&p[12], 50U);
    }
    if (fix) {
        p[20] = 3; p[21] = 0x01; p[23] = 9;
        w32(&p[24], (uint32_t)lon); w32(&p[28], (uint32_t)lat); w32(&p[36], 150000U);
        w32(&p[40], 3000U); w32(&p[44], 5000U);
    }
    feed_frame(0x01, 0x07, p, 92);
}

static void start(void)
{
    g_now = 1000;
    fake_fs_reset();
    fake_power_reset();
    purrgo_config_init();
    purrgo_utc_hint_reset();
    purrgo_gnss_ubx_init();
    purrgo_gnss_feed_init(&feed, g_now);
    memset(&feed.solution, 0, sizeof feed.solution);
}

/* ------------------------------------------------------------------------- */

static void test_frames(void)
{
    uint8_t f[40];
    size_t n = purrgo_ubx_build_mga_ini_time_utc(f, sizeof f, ep(2026, 11, 25, 10, 20, 30), 65535U);
    CHECK(n == 32);
    CHECK(frame_ok(f, n, 0x13, 0x40, 24));
    CHECK(f[6] == 0x10 && f[7] == 0x00 && f[8] == 0x00 && f[9] == 0x80);
    CHECK(f[10] == (2026 & 0xFF) && f[11] == (2026 >> 8) && f[12] == 11 && f[13] == 25);
    CHECK(f[14] == 10 && f[15] == 20 && f[16] == 30 && f[17] == 0);
    CHECK(r32(&f[18]) == 0U);                         /* ns */
    CHECK(f[22] == 0xFF && f[23] == 0xFF);            /* tAccS */
    CHECK(r32(&f[26]) == 0U);                         /* tAccNs */

    n = purrgo_ubx_build_mga_ini_time_utc(f, sizeof f, ep(2026, 11, 25, 10, 20, 30), 100000U);
    CHECK(n == 32 && f[22] == 0xFF && f[23] == 0xFF);   /* точность урезается до 65535 с */
    n = purrgo_ubx_build_mga_ini_time_utc(f, sizeof f, 3155760000UL, 1U);
    CHECK(n == 0);                                       /* вне 2000-2099 */
    n = purrgo_ubx_build_mga_ini_time_utc(f, 31, 1U, 1U);
    CHECK(n == 0);                                       /* кадр не помещается */

    n = purrgo_ubx_build_mga_ini_pos_llh(f, sizeof f, 537135000, -284199000, -1234, 50000000U);
    CHECK(n == 28);
    CHECK(frame_ok(f, n, 0x13, 0x40, 20));
    CHECK(f[6] == 0x01 && f[7] == 0x00);
    CHECK((int32_t)r32(&f[10]) == 537135000);
    CHECK((int32_t)r32(&f[14]) == -284199000);
    CHECK((int32_t)r32(&f[18]) == -1234);
    CHECK(r32(&f[22]) == 50000000U);
}

static void test_estimate_and_filter(void)
{
    uint32_t e = 0;

    start();
    CHECK(!purrgo_utc_hint_estimate(&e));                /* ничего не известно */

    g_now = 1000;
    feed_pvt(true, 2026, 11, 25, 10, 20, 30, true, 537000000, 284000000);
    CHECK(feed.solution.time_valid);
    CHECK(purrgo_utc_hint_estimate_at(1000, &e) && e == ep(2026, 11, 25, 10, 20, 30));
    CHECK(purrgo_utc_hint_estimate_at(6500, &e) && e == ep(2026, 11, 25, 10, 20, 35));   /* +5 с (целые) */

    /* Приёмник с ошибочной датой (заводское состояние): время отбрасывается, оценка не портится. */
    g_now = 2000;
    feed_pvt(true, 2006, 5, 1, 12, 0, 0, true, 537000000, 284000000);
    CHECK(!feed.solution.time_valid);
    CHECK(purrgo_utc_hint_estimate_at(2000, &e) && e == ep(2026, 11, 25, 10, 20, 31));

    /* Время чуть раньше (в пределах допуска) принимается, но оценка не откатывается назад. */
    g_now = 3000;
    feed_pvt(true, 2026, 11, 25, 10, 20, 0, true, 537000000, 284000000);
    CHECK(feed.solution.time_valid);
    CHECK(purrgo_utc_hint_estimate_at(3000, &e) && e >= ep(2026, 11, 25, 10, 20, 32) - 40U);

    /* Дальше вперёд - принимается. */
    g_now = 4000;
    feed_pvt(true, 2026, 11, 25, 10, 25, 0, true, 537000000, 284000000);
    CHECK(feed.solution.time_valid);
    CHECK(purrgo_utc_hint_estimate_at(4000, &e) && e == ep(2026, 11, 25, 10, 25, 0));

    /* Календарно невозможная дата при time_valid - тоже недостоверна. */
    g_now = 5000;
    feed_pvt(true, 2026, 13, 40, 10, 25, 0, true, 537000000, 284000000);
    CHECK(!feed.solution.time_valid);
}

static const char *cfg_text(void)
{
    static char copy[2048];
    uint32_t size = 0;
    const uint8_t *d = fake_fs_data(CFG, &size);
    if (d == NULL) { return NULL; }
    memcpy(copy, d, size);
    copy[size] = '\0';
    return copy;
}

static void test_config_roundtrip(void)
{
    uint32_t e = 0;
    const char *t;

    start();
    purrgo_config_load();                                /* файла нет: записываются умолчания */
    t = cfg_text();
    CHECK(t != NULL && strstr(t, "LAST_UTC=\n") != NULL);   /* время неизвестно: пустое значение */
    CHECK(app_config.last_utc_epoch == 0U);

    g_now = 10000;
    feed_pvt(true, 2026, 11, 25, 10, 20, 30, true, 537000000, 284000000);
    g_now = 70000;                                       /* минута спустя, новых кадров нет */
    purrgo_utc_hint_service(g_now);                      /* момент "сейчас" (в приборе - раз в секунду) */
    CHECK(purrgo_config_save());
    t = cfg_text();
    CHECK(t != NULL && strstr(t, "LAST_UTC=2026-11-25T10:21:30Z\n") != NULL);
    CHECK(app_config.last_utc_epoch == ep(2026, 11, 25, 10, 21, 30));

    /* "Перезагрузка": состояние времени сброшено, файл прочитан. */
    purrgo_utc_hint_reset();
    CHECK(purrgo_config_load());
    CHECK(app_config.last_utc_epoch == ep(2026, 11, 25, 10, 21, 30));
    CHECK(purrgo_utc_hint_floor() == ep(2026, 11, 25, 10, 21, 30));
    CHECK(purrgo_utc_hint_estimate(&e) && e == ep(2026, 11, 25, 10, 21, 30));

    /* Сеанс без времени приёмника: сохранённое значение остаётся в файле. */
    CHECK(purrgo_config_save());
    t = cfg_text();
    CHECK(t != NULL && strstr(t, "LAST_UTC=2026-11-25T10:21:30Z\n") != NULL);

    /* Мусорное и нечитаемое значение игнорируется. */
    fake_fs_put(CFG, "LAST_UTC=yesterday\n");
    purrgo_utc_hint_reset();
    purrgo_config_load();
    CHECK(app_config.last_utc_epoch == 0U && purrgo_utc_hint_floor() == 0U);
    fake_fs_put(CFG, "LAST_UTC=2026-13-25T10:21:30Z\n");
    purrgo_config_load();
    CHECK(app_config.last_utc_epoch == 0U);
    fake_fs_put(CFG, "LAST_UTC=1999-12-31T23:59:59Z\n");
    purrgo_config_load();
    CHECK(app_config.last_utc_epoch == 0U);
    /* purrgo_config_init() сбрасывает и "пол". */
    fake_fs_put(CFG, "LAST_UTC=2026-11-25T10:21:30Z\n");
    purrgo_config_load();
    CHECK(purrgo_utc_hint_floor() != 0U);
    purrgo_config_init();
    CHECK(purrgo_utc_hint_floor() == 0U);
}

static void test_floor_from_config(void)
{
    uint32_t e = 0;

    start();
    fake_fs_put(CFG, "LAST_UTC=2026-11-25T10:30:00Z\n");
    purrgo_config_load();

    g_now = 2000;
    feed_pvt(true, 2026, 11, 25, 9, 0, 0, true, 537000000, 284000000);
    CHECK(!feed.solution.time_valid);                    /* раньше сохранённого */
    CHECK(purrgo_utc_hint_estimate_at(2000, &e) && e == ep(2026, 11, 25, 10, 30, 0));

    g_now = 3000;
    feed_pvt(true, 2026, 11, 25, 10, 20, 0, true, 537000000, 284000000);
    CHECK(!feed.solution.time_valid);                    /* на 10 мин раньше: вне допуска */

    g_now = 4000;
    feed_pvt(true, 2026, 11, 25, 10, 26, 0, true, 537000000, 284000000);
    CHECK(feed.solution.time_valid);                     /* на 4 мин раньше: в пределах допуска 5 мин */

    g_now = 5000;
    feed_pvt(true, 2026, 11, 25, 11, 0, 0, true, 537000000, 284000000);
    CHECK(feed.solution.time_valid);
    CHECK(purrgo_utc_hint_estimate_at(5000, &e) && e == ep(2026, 11, 25, 11, 0, 0));
}

static void tx_frame(int idx, const uint8_t **f, size_t *len)
{
    size_t total = 0;
    const uint8_t *d = fake_gnss_tx_data(&total);
    /* кадры идут подряд: TIME_UTC (32 байта), затем POS_LLH (28 байт) */
    if (idx == 0) { *f = d; *len = 32; } else { *f = d + 32; *len = 28; }
}

static void test_hint_service(void)
{
    const uint8_t *f;
    size_t len, total;

    /* --- холодное включение: время и центр карты из PURRGO.CFG, приёмник жив, но времени не знает --- */
    start();
    fake_fs_put(CFG, "LAST_UTC=2026-11-25T10:30:00Z\nLAST_LAT_1E7=537135000\nLAST_LON_1E7=284199000\n");
    purrgo_config_load();

    g_now = 1000;
    feed_pvt(false, 0, 0, 0, 0, 0, 0, false, 0, 0);
    purrgo_utc_hint_service(g_now);                      /* начало отсчёта */
    g_now = 3000;
    feed_pvt(false, 0, 0, 0, 0, 0, 0, false, 0, 0);
    purrgo_utc_hint_service(g_now);
    CHECK(fake_gnss_tx_frames() == 0);                   /* меньше PURRGO_UTC_HINT_GRACE_MS */

    g_now = 5100;
    feed_pvt(false, 0, 0, 0, 0, 0, 0, false, 0, 0);
    purrgo_utc_hint_service(g_now);
    CHECK(fake_gnss_tx_frames() == 2);
    (void)fake_gnss_tx_data(&total);
    CHECK(total == 60);
    tx_frame(0, &f, &len);
    CHECK(frame_ok(f, len, 0x13, 0x40, 24) && f[6] == 0x10);
    CHECK(f[10] == (2026 & 0xFF) && f[11] == (2026 >> 8) && f[12] == 11 && f[13] == 25 && f[14] == 10 &&
          f[15] == 30 && f[16] == 0);
    CHECK(f[22] == 0xFF && f[23] == 0xFF);               /* грубая точность: сколько прошло, неизвестно */
    tx_frame(1, &f, &len);
    CHECK(frame_ok(f, len, 0x13, 0x40, 20) && f[6] == 0x01);
    CHECK((int32_t)r32(&f[10]) == 537135000 && (int32_t)r32(&f[14]) == 284199000);
    CHECK(r32(&f[22]) == PURRGO_UTC_HINT_BOOT_POS_ACC_M * 100U);

    /* Один раз за период. */
    g_now = 8000;
    feed_pvt(false, 0, 0, 0, 0, 0, 0, false, 0, 0);
    purrgo_utc_hint_service(g_now);
    g_now = 20000;
    feed_pvt(false, 0, 0, 0, 0, 0, 0, false, 0, 0);
    purrgo_utc_hint_service(g_now);
    CHECK(fake_gnss_tx_frames() == 2);

    /* --- приёмник сообщил время: подсказка не нужна, период закончен --- */
    g_now = 30000;
    feed_pvt(true, 2026, 11, 25, 10, 40, 0, true, 537200000, 284300000);
    purrgo_utc_hint_service(g_now);
    CHECK(fake_gnss_tx_frames() == 2);

    /* --- приёмник перезапущен (молчал дольше 10 с), времени снова не знает: подсказка по данным сеанса --- */
    g_now = 31000;
    purrgo_utc_hint_service(g_now);                      /* PVT перестали приходить */
    g_now = 45000;
    purrgo_utc_hint_service(g_now);                      /* молчание дольше PURRGO_UTC_HINT_SILENCE_RESET_MS */
    g_now = 46000;
    feed_pvt(false, 0, 0, 0, 0, 0, 0, false, 0, 0);
    purrgo_utc_hint_service(g_now);
    g_now = 50500;
    feed_pvt(false, 0, 0, 0, 0, 0, 0, false, 0, 0);
    purrgo_utc_hint_service(g_now);
    CHECK(fake_gnss_tx_frames() == 4);
    (void)fake_gnss_tx_data(&total);
    CHECK(total == 120);
    {
        size_t t2;
        const uint8_t *d = fake_gnss_tx_data(&t2) + 60;
        CHECK(frame_ok(d, 32, 0x13, 0x40, 24) && d[6] == 0x10);
        /* 10:40:00 в 30000 мс, сейчас 50500 мс: +20 с -> 10:40:20, точность 2 с */
        CHECK(d[12] == 11 && d[13] == 25 && d[14] == 10 && d[15] == 40 && d[16] == 20);
        CHECK(d[22] == 2 && d[23] == 0);
        CHECK(frame_ok(d + 32, 28, 0x13, 0x40, 20) && d[38] == 0x01);
        CHECK((int32_t)r32(&d[42]) == 537200000 && (int32_t)r32(&d[46]) == 284300000);
        CHECK(r32(&d[54]) == (2000U + 30U * 20U) * 100U);   /* 2 км плюс 30 м/с за 20 с */
    }

    /* --- нет связи с приёмником (purrgo_gnss_write == false): повтор на следующей секунде --- */
    start();
    fake_fs_put(CFG, "LAST_UTC=2026-11-25T10:30:00Z\n");
    purrgo_config_load();
    fake_gnss_tx_set_fail(true);
    g_now = 1000;
    feed_pvt(false, 0, 0, 0, 0, 0, 0, false, 0, 0);
    purrgo_utc_hint_service(g_now);
    g_now = 5500;
    feed_pvt(false, 0, 0, 0, 0, 0, 0, false, 0, 0);
    purrgo_utc_hint_service(g_now);
    CHECK(fake_gnss_tx_frames() == 0);
    fake_gnss_tx_set_fail(false);
    g_now = 6500;
    feed_pvt(false, 0, 0, 0, 0, 0, 0, false, 0, 0);
    purrgo_utc_hint_service(g_now);
    CHECK(fake_gnss_tx_frames() == 2);                   /* время и позиция (центр карты) - всё ушло после восстановления связи */

    /* --- приёмник знает время сразу (горячий старт): подсказки нет --- */
    start();
    fake_fs_put(CFG, "LAST_UTC=2026-11-25T10:30:00Z\n");
    purrgo_config_load();
    for (uint32_t t = 1000; t <= 12000; t += 1000) {
        g_now = t;
        feed_pvt(true, 2026, 11, 25, 10, 31, 0, true, 537000000, 284000000);
        purrgo_utc_hint_service(g_now);
    }
    CHECK(fake_gnss_tx_frames() == 0);

    /* --- ничего не известно (первый запуск): подсказывать нечем --- */
    start();
    for (uint32_t t = 1000; t <= 12000; t += 1000) {
        g_now = t;
        feed_pvt(false, 0, 0, 0, 0, 0, 0, false, 0, 0);
        purrgo_utc_hint_service(g_now);
    }
    CHECK(fake_gnss_tx_frames() == 0);
}

int main(void)
{
    test_frames();
    test_estimate_and_filter();
    test_config_roundtrip();
    test_floor_from_config();
    test_hint_service();
    return test_finish("test_utc_hint");
}
