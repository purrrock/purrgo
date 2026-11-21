/*
 * Живое состояние приёмника (src/core/gnss/gnss_feed.c): разбор потока байт и таймаут фикса.
 *   - NMEA (RMC) и бинарный UBX NAV-PVT, вперемешку, байт за байтом: каждое принятое решение
 *     увеличивает fix_seq и обновляет метку "приёмник жив"; кадр UBX не может начаться,
 *     пока начата NMEA-строка; прочие предложения и кадры решением не считаются;
 *   - таймаут: строго "дольше" срока; простой главного цикла даёт новый срок; метка
 *     "из будущего" не даёт ложного таймаута; устойчив к переполнению uint32_t;
 *   - restart/restart_silence/init/invalidate.
 */
#include <string.h>

#include "gnss/gnss_feed.h"
#include "purrgo/gnss_adapter.h"
#include "purrgo/gnss_receiver.h"
#include "test_common.h"

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

static const char RMC[] = "GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W";
static const char GGA[] = "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,";

static void nmea(char *out, size_t size, const char *body)
{
    unsigned cs = 0;
    for (const char *p = body; *p; p++) { cs ^= (unsigned char)*p; }
    snprintf(out, size, "$%s*%02X\r\n", body, cs);
}

static void feed_bytes(purrgo_gnss_feed_t *f, const void *data, size_t n)
{
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < n; i++) { purrgo_gnss_feed_byte(f, p[i]); }
}

static void feed_nmea(purrgo_gnss_feed_t *f, const char *body)
{
    char line[160];
    nmea(line, sizeof line, body);
    feed_bytes(f, line, strlen(line));
}

static void fresh(purrgo_gnss_feed_t *f)
{
    memset(f, 0, sizeof *f);
    purrgo_gnss_adapter_reset();
    purrgo_gnss_ubx_init();
    purrgo_gnss_feed_init(f, g_now);
}

/*
 * Главный цикл вызывает проверку таймаута часто. Прыжок во времени одним вызовом
 * (больше половины срока с прошлой проверки) модуль считает простоем цикла и даёт
 * новый срок, поэтому время в тестах движется шагами, как в реальном цикле.
 */
static void run_checks(purrgo_gnss_feed_t *f, uint32_t from_ms, uint32_t to_ms, uint32_t timeout_ms)
{
    uint32_t t = from_ms;
    while ((int32_t)(to_ms - t) > 0) {
        t += ((to_ms - t) > 500U) ? 500U : (to_ms - t);
        purrgo_gnss_feed_check_timeout(f, t, timeout_ms);
    }
}

#if PURRGO_GNSS_UBX
static void w16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void w32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

#define PVT_LAT 507654321
#define PVT_LON 98765432

static size_t pvt_frame(uint8_t *out, size_t size)
{
    uint8_t p[92];
    memset(p, 0, sizeof p);
    w16(&p[4], 2026); p[6] = 10; p[7] = 19; p[8] = 12; p[9] = 34; p[10] = 56;
    p[11] = 0x03; p[20] = 3; p[21] = 0x01; p[23] = 11;
    w32(&p[24], (uint32_t)PVT_LON); w32(&p[28], (uint32_t)PVT_LAT);
    return purrgo_ubx_build_frame(out, size, 0x01, 0x07, p, 92);
}

static size_t odo_frame(uint8_t *out, size_t size)
{
    uint8_t p[20];
    memset(p, 0, sizeof p);
    w32(&p[8], 1234U);
    return purrgo_ubx_build_frame(out, size, 0x01, 0x09, p, 20);
}
#endif

static void test_init(void)
{
    purrgo_gnss_feed_t f;
    memset(&f, 0, sizeof f);

    f.solution.valid = true;
    f.fix_seq = 7U;
    purrgo_gnss_feed_init(&f, 4242U);

    CHECK(f.last_rx_ms == 4242U && f.last_check_ms == 4242U);
    CHECK(f.solution.valid && f.fix_seq == 7U);   /* решение и счётчик переживают init */
}

static void test_nmea_rmc_counts(void)
{
    purrgo_gnss_feed_t f;
    g_now = 1000U;
    fresh(&f);

    g_now = 5000U;
    feed_nmea(&f, RMC);
    CHECK(f.fix_seq == 1U);
    CHECK(f.last_rx_ms == 5000U);
    CHECK(f.solution.valid);
    CHECK(f.solution.lat_1e7 / 100000 == 4811);   /* 48,1173 градуса */

    g_now = 6000U;
    feed_nmea(&f, RMC);
    CHECK(f.fix_seq == 2U && f.last_rx_ms == 6000U);
}

static void test_nmea_not_a_decision(void)
{
    purrgo_gnss_feed_t f;
    char line[160];
    g_now = 1000U;
    fresh(&f);

    /* GGA - не решение: счётчик и метка не меняются. */
    g_now = 2000U;
    feed_nmea(&f, GGA);
    CHECK(f.fix_seq == 0U && f.last_rx_ms == 1000U);

    /* Неверная контрольная сумма. */
    nmea(line, sizeof line, RMC);
    line[strlen(line) - 4U] ^= 1;
    feed_bytes(&f, line, strlen(line));
    CHECK(f.fix_seq == 0U && f.last_rx_ms == 1000U);

    /* Разборщик не залип: следующая верная RMC принимается. */
    feed_nmea(&f, RMC);
    CHECK(f.fix_seq == 1U && f.last_rx_ms == 2000U);
}

#if PURRGO_GNSS_UBX
static void test_ubx_pvt_counts(void)
{
    purrgo_gnss_feed_t f;
    uint8_t buf[128];
    g_now = 1000U;
    fresh(&f);

    const size_t n = pvt_frame(buf, sizeof buf);
    CHECK(n > 92U);

    g_now = 3000U;
    feed_bytes(&f, buf, n);
    CHECK(f.fix_seq == 1U);
    CHECK(f.last_rx_ms == 3000U);
    CHECK(f.solution.valid && f.solution.lat_1e7 == PVT_LAT && f.solution.lon_1e7 == PVT_LON);
}

static void test_ubx_other_frame_is_not_a_decision(void)
{
    purrgo_gnss_feed_t f;
    uint8_t buf[64];
    g_now = 1000U;
    fresh(&f);

    const size_t n = odo_frame(buf, sizeof buf);
    g_now = 2000U;
    feed_bytes(&f, buf, n);
    CHECK(f.fix_seq == 0U && f.last_rx_ms == 1000U);   /* кадр принят, но "приёмник жив" не отмечен */
}

static void test_ubx_and_nmea_interleaved(void)
{
    purrgo_gnss_feed_t f;
    uint8_t buf[128];
    g_now = 1000U;
    fresh(&f);

    const size_t n = pvt_frame(buf, sizeof buf);

    g_now = 2000U; feed_nmea(&f, RMC);
    CHECK(f.fix_seq == 1U);
    g_now = 2100U; feed_bytes(&f, buf, n);
    CHECK(f.fix_seq == 2U && f.last_rx_ms == 2100U);
    g_now = 2200U; feed_nmea(&f, GGA);
    CHECK(f.fix_seq == 2U);
    g_now = 2300U; feed_nmea(&f, RMC);   /* RMC при владеющем PVT тоже подтверждает, что приёмник жив */
    CHECK(f.fix_seq == 3U && f.last_rx_ms == 2300U);
    g_now = 2400U; feed_bytes(&f, buf, n);
    CHECK(f.fix_seq == 4U && f.last_rx_ms == 2400U);
}

static void test_ubx_cannot_start_inside_nmea_line(void)
{
    purrgo_gnss_feed_t f;
    uint8_t buf[128];
    g_now = 1000U;
    fresh(&f);

    const size_t n = pvt_frame(buf, sizeof buf);

    /* NMEA-строка начата, не закончена: B5 внутри текста не должен срывать разбор. */
    feed_bytes(&f, "$GPRMC,1235", 11U);
    g_now = 2000U;
    feed_bytes(&f, buf, n);
    CHECK(f.fix_seq == 0U);          /* кадр внутри строки кадром не считается */
    feed_bytes(&f, "\r\n", 2U);      /* строка закончилась (мусорная) */

    /* После конца строки разборщики снова в порядке. */
    g_now = 3000U;
    feed_bytes(&f, buf, n);
    CHECK(f.fix_seq == 1U && f.last_rx_ms == 3000U);
    feed_nmea(&f, RMC);
    CHECK(f.fix_seq == 2U);
}
#endif

static void test_timeout_is_strictly_longer(void)
{
    purrgo_gnss_feed_t f;
    const uint32_t T = 5000U;
    g_now = 1000U;
    fresh(&f);

    g_now = 10000U;
    feed_nmea(&f, RMC);
    CHECK(f.solution.valid);

    f.last_check_ms = 10000U;   /* цикл крутится: проверки шли и до этого */

    /* Ровно срок - ещё действует (сравнение строгое). */
    run_checks(&f, 10000U, 10000U + T, T);
    CHECK(f.solution.valid);
    purrgo_gnss_feed_check_timeout(&f, 10000U + T + 1U, T);
    CHECK(!f.solution.valid);
}

static void test_timeout_invalidates_fields(void)
{
    purrgo_gnss_feed_t f;
    g_now = 1000U;
    fresh(&f);

    f.solution.valid = true;
    f.solution.course_valid = true;
    f.solution.speed_knots = 321;
    f.solution.fix_quality = 2;
    f.solution.acc_valid = true;
    f.solution.fix_type = 3;
    f.solution.lat_1e7 = 531000000;
    f.last_rx_ms = 1000U;
    f.last_check_ms = 1000U;

    run_checks(&f, 1000U, 1000U + 5000U, 5000U);
    CHECK(f.solution.valid);   /* срок ещё не вышел */
    purrgo_gnss_feed_check_timeout(&f, 1000U + 5000U + 1U, 5000U);
    CHECK(!f.solution.valid && !f.solution.course_valid && !f.solution.acc_valid);
    CHECK(f.solution.speed_knots == 0 && f.solution.fix_quality == 0);
    CHECK(f.solution.fix_type == 0);              /* "тип неизвестен", не "нет фикса" (1) */
    CHECK(f.solution.lat_1e7 == 531000000);       /* координаты последнего решения остаются */
}

static void test_timeout_caller_chooses_duration(void)
{
    purrgo_gnss_feed_t f;
    g_now = 0U;
    fresh(&f);
    f.solution.valid = true;
    f.last_rx_ms = 0U;

    /* Трекер с PSMOO: срок длиннее, приёмник молчит между фиксами. */
    for (uint32_t t = 1000U; t <= 60000U; t += 1000U) {
        purrgo_gnss_feed_check_timeout(&f, t, 60000U);
        CHECK(f.solution.valid);
    }
    purrgo_gnss_feed_check_timeout(&f, 61000U, 60000U);
    CHECK(!f.solution.valid);
}

static void test_timeout_resets_time_valid(void)
{
    purrgo_gnss_feed_t f;
    g_now = 1000U;
    fresh(&f);

    /* Фикс и время есть: приёмник замолчал - время тоже перестало обновляться, прочерки. */
    f.solution.valid = true;
    f.solution.time_valid = true;
    f.solution.hours = 9; f.solution.minutes = 30;
    f.last_rx_ms = 1000U;
    f.last_check_ms = 1000U;
    run_checks(&f, 1000U, 1000U + 5000U, 5000U);
    CHECK(f.solution.time_valid);                        /* срок ещё не вышел */
    purrgo_gnss_feed_check_timeout(&f, 1000U + 5000U + 1U, 5000U);
    CHECK(!f.solution.valid && !f.solution.time_valid);

    /* Только время (PVT без фикса) тоже не живёт вечно после молчания приёмника. */
    fresh(&f);
    f.solution.valid = false;
    f.solution.time_valid = true;
    f.last_rx_ms = 100000U;
    f.last_check_ms = 100000U;
    run_checks(&f, 100000U, 100000U + 5000U, 5000U);
    CHECK(f.solution.time_valid);
    purrgo_gnss_feed_check_timeout(&f, 100000U + 5000U + 1U, 5000U);
    CHECK(!f.solution.time_valid);
}

static void test_timeout_not_applied_without_fix(void)
{
    purrgo_gnss_feed_t f;
    g_now = 0U;
    fresh(&f);

    f.solution.valid = false;
    f.solution.lat_1e7 = 123;
    f.solution.fix_type = 3;
    purrgo_gnss_feed_check_timeout(&f, 100000U, 5000U);
    CHECK(f.solution.fix_type == 3);   /* нет фикса - нечего сбрасывать, поля не трогаются */
}

static void test_stalled_main_loop_gets_new_deadline(void)
{
    purrgo_gnss_feed_t f;
    const uint32_t T = 5000U;
    const uint32_t HALF = T / 2U;   /* порог простоя считается от переданного timeout_ms */

    /* Цикл простоял дольше половины срока: молчание оценить нельзя - срок даётся заново. */
    g_now = 1000U;
    fresh(&f);
    f.solution.valid = true;
    f.last_rx_ms = 1000U;
    f.last_check_ms = 1000U;
    purrgo_gnss_feed_check_timeout(&f, 1000U + HALF + 1U, T);
    CHECK(f.solution.valid);
    CHECK(f.last_rx_ms == 1000U + HALF + 1U);
    CHECK(f.last_check_ms == 1000U + HALF + 1U);

    /* Ровно половина - это ещё не простой (сравнение строгое): срок не обновляется. */
    fresh(&f);
    f.solution.valid = true;
    f.last_rx_ms = 1000U;
    f.last_check_ms = 1000U;
    purrgo_gnss_feed_check_timeout(&f, 1000U + HALF, T);
    CHECK(f.last_rx_ms == 1000U);

    /* Даже после простоя настоящее молчание потом всё равно приводит к таймауту. */
    fresh(&f);
    f.solution.valid = true;
    f.last_rx_ms = 1000U;
    f.last_check_ms = 1000U;
    uint32_t t = 1000U + HALF + 1U;
    purrgo_gnss_feed_check_timeout(&f, t, T);       /* грейс */
    CHECK(f.solution.valid);
    const uint32_t grace_start = t;
    for (; t < grace_start + T; t += 100U) {
        purrgo_gnss_feed_check_timeout(&f, t + 100U, T);
    }
    purrgo_gnss_feed_check_timeout(&f, grace_start + T + 1U, T);
    CHECK(!f.solution.valid);
}

static void test_stall_threshold_follows_timeout_param(void)
{
    purrgo_gnss_feed_t f;
    const uint32_t T = 60000U;   /* длинный срок (трекер): порог простоя = T/2, а не константа */

    g_now = 1000U;
    fresh(&f);
    f.solution.valid = true;
    f.last_rx_ms = 1000U;
    f.last_check_ms = 1000U;
    /* Пауза больше PURRGO_GNSS_FIX_TIMEOUT_MS/2, но меньше T/2: это не простой цикла. */
    purrgo_gnss_feed_check_timeout(&f, 1000U + PURRGO_GNSS_FIX_TIMEOUT_MS, T);
    CHECK(f.last_rx_ms == 1000U);
    /* Пауза больше T/2 - простой: срок даётся заново. */
    purrgo_gnss_feed_check_timeout(&f, 1000U + PURRGO_GNSS_FIX_TIMEOUT_MS + T / 2U + 1U, T);
    CHECK(f.last_rx_ms == 1000U + PURRGO_GNSS_FIX_TIMEOUT_MS + T / 2U + 1U);
    CHECK(f.solution.valid);
}

static void test_stamp_from_the_future_is_fresh(void)
{
    purrgo_gnss_feed_t f;
    g_now = 1000U;
    fresh(&f);

    /* Кадр разобран позже начала такта и получил метку новее "now" главного цикла. */
    g_now = 1005U;
    feed_nmea(&f, RMC);
    CHECK(f.last_rx_ms == 1005U);

    purrgo_gnss_feed_check_timeout(&f, 1000U, 5000U);   /* now < метки: возраст 0, не ~4,29e9 */
    CHECK(f.solution.valid);
}

static void test_time_wraparound(void)
{
    purrgo_gnss_feed_t f;
    g_now = 0xFFFFFF00U;
    fresh(&f);

    feed_nmea(&f, RMC);                      /* метка до переполнения */
    CHECK(f.last_rx_ms == 0xFFFFFF00U);

    purrgo_gnss_feed_check_timeout(&f, 0x00000100U, 5000U);   /* прошло 512 мс через ноль */
    CHECK(f.solution.valid);

    run_checks(&f, 0x00000100U, 0xFFFFFF00U + 5000U, 5000U);
    CHECK(f.solution.valid);                                  /* ровно 5000 мс - ещё действует */
    purrgo_gnss_feed_check_timeout(&f, 0xFFFFFF00U + 5001U, 5000U);   /* 5001 мс через ноль */
    CHECK(!f.solution.valid);
}

static void test_restart(void)
{
    purrgo_gnss_feed_t f;
    g_now = 0U;
    fresh(&f);
    f.last_rx_ms = 100U;
    f.last_check_ms = 200U;
    f.fix_seq = 9U;
    f.solution.valid = true;

    purrgo_gnss_feed_restart_silence(&f, 5000U);
    CHECK(f.last_rx_ms == 5000U && f.last_check_ms == 200U);   /* счётчик простоя не тронут */

    purrgo_gnss_feed_restart(&f, 6000U);
    CHECK(f.last_rx_ms == 6000U && f.last_check_ms == 6000U);
    CHECK(f.fix_seq == 9U && f.solution.valid);                /* решение и счётчик - не тронуты */
}

static void test_invalidate(void)
{
    purrgo_gnss_feed_t f;
    g_now = 0U;
    fresh(&f);
    f.solution.valid = true;
    f.solution.course_valid = true;
    f.solution.speed_knots = 99;
    f.solution.fix_quality = 1;
    f.solution.acc_valid = true;
    f.solution.fix_type = 3;
    f.last_rx_ms = 77U;
    f.fix_seq = 4U;

    purrgo_gnss_feed_invalidate(&f);
    CHECK(!f.solution.valid && !f.solution.course_valid && !f.solution.acc_valid);
    CHECK(f.solution.speed_knots == 0 && f.solution.fix_quality == 0 && f.solution.fix_type == 0);
    CHECK(f.last_rx_ms == 77U && f.fix_seq == 4U);   /* отсчёт молчания и счётчик решений не трогаются */
}

int main(void)
{
    test_init();
    test_nmea_rmc_counts();
    test_nmea_not_a_decision();
#if PURRGO_GNSS_UBX
    test_ubx_pvt_counts();
    test_ubx_other_frame_is_not_a_decision();
    test_ubx_and_nmea_interleaved();
    test_ubx_cannot_start_inside_nmea_line();
#endif
    test_timeout_is_strictly_longer();
    test_timeout_resets_time_valid();
    test_timeout_invalidates_fields();
    test_timeout_caller_chooses_duration();
    test_timeout_not_applied_without_fix();
    test_stalled_main_loop_gets_new_deadline();
    test_stall_threshold_follows_timeout_param();
    test_stamp_from_the_future_is_fresh();
    test_time_wraparound();
    test_restart();
    test_invalidate();
    return test_finish("test_gnss_feed");
}
