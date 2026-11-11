/*
 * Диагностическая сборка track_logger (PURRGO_TRACK_DIAG=1): в GPX пишется каждая
 * валидная эпоха, а pg:rej показывает, какие фильтры обычная сборка применила бы.
 * track_logger.c включается прямо в тест (с определённым PURRGO_TRACK_DIAG), чтобы
 * не пересобирать ядро: символы логгера берутся отсюда, а не из библиотеки.
 */
#define PURRGO_TRACK_DIAG 1
#include "track_logger.c"

#include <stdlib.h>
#include "fake_fs.h"
#include "test_common.h"

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

#define LAT0 537000000
#define LON0 284000000

static purrgo_gnss_solution_t mk(uint32_t t, int north_m)
{
    purrgo_gnss_solution_t f;
    memset(&f, 0, sizeof f);
    f.valid = true; f.year = 26; f.month = 10; f.day = 19;
    f.hours = (uint8_t)(t / 3600U); f.minutes = (uint8_t)((t / 60U) % 60U); f.seconds = (uint8_t)(t % 60U);
    f.lat_1e7 = LAT0 + north_m * PURRGO_1E7_PER_METER;
    f.lon_1e7 = LON0; f.alt_m = 100;
    f.fix_type = 3; f.satellites_tracked = 8; f.hdop_100 = 100;
    f.speed_knots = 1000;
    return f;
}

static int rej_of_point(const char *gpx, int index)
{
    const char *p = gpx;
    for (int i = 0; i <= index; i++) {
        p = strstr(p, "<trkpt");
        if (!p) { return -1; }
        if (i < index) { p += 6; }
    }
    const char *end = strstr(p, "</trkpt>");
    const char *r = strstr(p, "<pg:rej>");
    if (!r || !end || r > end) { return -1; }
    return atoi(r + 8);
}

static int count_points(const char *gpx)
{
    int c = 0;
    for (const char *p = gpx; (p = strstr(p, "<trkpt")); p += 6) { c++; }
    return c;
}

static void test_diag_logs_every_epoch_with_reasons(void)
{
    purrgo_logger_reset();
    fake_fs_reset();
    app_config.tz_offset_minutes = 0;

    purrgo_gnss_solution_t f = mk(0, 0);
    CHECK(purrgo_logger_start(&f));
    CHECK(purrgo_logger_add_point(&f));                          /* #0: записана, rej = 0 */

    f = mk(1, 10);                      CHECK(purrgo_logger_add_point(&f));    /* #1: направление неизвестно - пишется */
    f = mk(2, 20);                      CHECK(!purrgo_logger_add_point(&f));   /* #2: прямая - прореживание */
    f = mk(3, 30); f.fix_type = 2;      CHECK(!purrgo_logger_add_point(&f));   /* #3: 2D */
    f = mk(4, 40); f.hdop_100 = 700; f.satellites_tracked = 3;
                                        CHECK(!purrgo_logger_add_point(&f));   /* #4: HDOP и спутники */
    f = mk(5, 50); f.speed_knots = 10;  CHECK(!purrgo_logger_add_point(&f));   /* #5: стоим */
    f = mk(6, 0);  f.lat_1e7 += 1000000;CHECK(!purrgo_logger_add_point(&f));   /* #6: выброс (111 км за 1 с) */

    CHECK(purrgo_logger_get_track_point_count() == 2);            /* экранный трек - только записанные */

    char name[64];
    snprintf(name, sizeof name, "%s", purrgo_logger_get_active_filename());
    purrgo_logger_stop();
    uint32_t sz = 0;
    const uint8_t *d = fake_fs_data(name, &sz);
    CHECK(d != NULL);
    if (!d) { return; }
    char *t = malloc((size_t)sz + 1u);
    memcpy(t, d, sz); t[sz] = 0;

    CHECK(count_points(t) == 7);
    CHECK(rej_of_point(t, 0) == 0);
    CHECK(rej_of_point(t, 1) == 0);
    CHECK(rej_of_point(t, 2) == LOGGER_REJ_DECIMATED);
    CHECK(rej_of_point(t, 3) == LOGGER_REJ_FIX_TYPE);
    CHECK(rej_of_point(t, 4) == (LOGGER_REJ_HDOP | LOGGER_REJ_SATS));
    CHECK(rej_of_point(t, 5) == LOGGER_REJ_STATIONARY);
    CHECK(rej_of_point(t, 6) == LOGGER_REJ_OUTLIER);
    CHECK(strstr(t, "</gpx>") != NULL);
    free(t);
}

static void test_diag_hacc_and_sacc(void)
{
    /* UBX: при известной hAcc HDOP не учитывается; hAcc выше порога - бит LOGGER_REJ_HACC. */
    purrgo_logger_reset();
    fake_fs_reset();
    app_config.tz_offset_minutes = 0;

    purrgo_gnss_solution_t f = mk(0, 0);
    f.acc_valid = true; f.hacc_mm = 3500; f.sacc_mm_s = 120;
    CHECK(purrgo_logger_start(&f));
    CHECK(purrgo_logger_add_point(&f));                           /* #0: записана */

    f = mk(1, 10);
    f.acc_valid = true; f.hacc_mm = PURRGO_GNSS_TRACK_MAX_HACC_M * 1000U + 1U;
    f.hdop_100 = 900; f.sacc_mm_s = 450;                          /* HDOP выше порога, но hAcc точнее */
    CHECK(!purrgo_logger_add_point(&f));                          /* #1: hAcc > порога */

    f = mk(2, 20);
    f.acc_valid = true; f.hacc_mm = 4000; f.hdop_100 = 900;
    CHECK(purrgo_logger_add_point(&f));                           /* #2: hAcc в норме, HDOP не смотрим */

    f = mk(3, 30);                                                /* без PVT: фильтр по HDOP, sAcc не пишется */
    f.hdop_100 = 900;
    CHECK(!purrgo_logger_add_point(&f));                          /* #3: HDOP > 5 */

    char name[64];
    snprintf(name, sizeof name, "%s", purrgo_logger_get_active_filename());
    purrgo_logger_stop();
    uint32_t sz = 0;
    const uint8_t *d = fake_fs_data(name, &sz);
    CHECK(d != NULL);
    if (!d) { return; }
    char *t = malloc((size_t)sz + 1u);
    memcpy(t, d, sz); t[sz] = 0;

    CHECK(count_points(t) == 4);
    CHECK(rej_of_point(t, 0) == 0);
    CHECK(rej_of_point(t, 1) == LOGGER_REJ_HACC);                 /* без бита HDOP */
    CHECK(rej_of_point(t, 2) == 0);
    CHECK(rej_of_point(t, 3) == LOGGER_REJ_HDOP);
    CHECK(strstr(t, "<pg:sacc>0.120</pg:sacc>") != NULL);
    CHECK(strstr(t, "<pg:sacc>0.450</pg:sacc>") != NULL);
    /* sAcc только у точек с оценками PVT: у #3 его нет (всего 3 тега на 4 точки). */
    int sacc_tags = 0;
    for (const char *q = t; (q = strstr(q, "<pg:sacc>")); q += 9) { sacc_tags++; }
    CHECK(sacc_tags == 3);
    free(t);
}

static void test_diag_sync_cadence(void)
{
    /* Диагностические точки идут на карту: sync по числу точек, как у обычных. */
    purrgo_logger_reset();
    fake_fs_reset();
    app_config.tz_offset_minutes = 0;
    purrgo_gnss_solution_t f = mk(0, 0);
    CHECK(purrgo_logger_start(&f) && purrgo_logger_add_point(&f));
    for (uint32_t i = 1; i <= 120; i++) {
        f = mk(i, (int)i);                                        /* ~1 м/с по прямой: почти все точки отбракованы */
        f.speed_knots = 200;
        (void)purrgo_logger_add_point(&f);
    }
    CHECK(fake_fs_sync_calls() >= 2);
    purrgo_logger_stop();
    CHECK(fake_fs_open_handles() == 0);
}

int main(void)
{
    test_diag_logs_every_epoch_with_reasons();
    test_diag_hacc_and_sacc();
    test_diag_sync_cadence();
    return test_finish("test_track_diag");
}
