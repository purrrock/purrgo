/*
 * Host-тесты фильтра "незначительных изменений" в track_logger: точка пишется,
 * только если трек в этом месте заметно изменился (отклонение от продолжения
 * последнего отрезка, разворот, предельная хорда) или по таймауту.
 * Геометрия задаётся в метрах от опорной точки, чтобы тесты не зависели от
 * внутренних единиц (1e-7 градуса).
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/geo.h"
#include "purrgo/track_logger.h"
#include "purrgo/config.h"
#include "fake_fs.h"
#include "test_common.h"

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

#define LAT0 537000000
#define LON0 284000000

/* Фикс через t секунд от 00:00:00 UTC (t < 86400), смещение в метрах на север и восток. */
static purrgo_gnss_solution_t mk(uint32_t t, int north_m, int east_m)
{
    purrgo_gnss_solution_t f;
    memset(&f, 0, sizeof f);
    f.valid = true; f.year = 26; f.month = 10; f.day = 19;
    f.hours = (uint8_t)(t / 3600U); f.minutes = (uint8_t)((t / 60U) % 60U); f.seconds = (uint8_t)(t % 60U);
    const int32_t cos10k = purrgo_geo_cos_10k(LAT0);
    f.lat_1e7 = LAT0 + north_m * PURRGO_1E7_PER_METER;
    f.lon_1e7 = LON0 + (int32_t)((int64_t)east_m * PURRGO_1E7_PER_METER * 10000 / cos10k);
    f.alt_m = 100;
    f.speed_knots = 500;                 /* 5 уз: движемся */
    return f;
}

static void begin(track_logger_mode_t mode)
{
    purrgo_logger_reset();
    fake_fs_reset();
    app_config.tz_offset_minutes = 0;
    purrgo_logger_set_mode(mode);
    purrgo_gnss_solution_t f = mk(0, 0, 0);
    CHECK(purrgo_logger_start(&f));
    CHECK(purrgo_logger_add_point(&f));  /* первая точка пишется всегда */
}

static bool add(uint32_t t, int north_m, int east_m)
{
    purrgo_gnss_solution_t f = mk(t, north_m, east_m);
    return purrgo_logger_add_point(&f);
}

/* Движение на север шагом 10 м за секунду, k = from..to; возвращает число записанных точек. */
static int walk_north(int from, int to)
{
    int n = 0;
    for (int k = from; k <= to; k++) {
        if (add((uint32_t)k, k * 10, 0)) { n++; }
    }
    return n;
}

static void test_straight_line_is_thinned(void)
{
    begin(LOGGER_MODE_STANDARD);
    int n = 1 + walk_north(1, 100);                 /* 1 км по прямой */
    /*
     * Раньше писалась каждая точка (100 штук). Теперь: первая, вторая (направление
     * ещё неизвестно) и по одной на каждые 250 м предельной хорды.
     */
    CHECK(n >= 4 && n <= 6);
    CHECK((size_t)n == purrgo_logger_get_track_point_count());

    /* Все записанные точки лежат на одной линии. */
    track_point_t p;
    for (size_t i = 0; i < purrgo_logger_get_track_point_count(); i++) {
        CHECK(purrgo_logger_get_track_point(i, &p) && p.lon_1e7 == mk(0, 0, 0).lon_1e7);
    }
    purrgo_logger_stop();
}

static void test_speed_change_is_not_a_change(void)
{
    /* Разгон и торможение по прямой - не повод для точки: меняется шаг, а не геометрия. */
    begin(LOGGER_MODE_STANDARD);
    int n = 0;
    int north = 0;
    n += add(1, north += 6, 0);                     /* направление ещё неизвестно: пишется */
    for (int t = 2; t <= 20; t++) {
        north += (t < 10) ? t : (20 - t);           /* шаг растёт и падает, курс прежний */
        n += add((uint32_t)t, north, 0);
    }
    CHECK(n <= 2);
    purrgo_logger_stop();
}

static void test_corner_is_recorded(void)
{
    begin(LOGGER_MODE_STANDARD);
    walk_north(1, 10);                              /* 100 м на север */
    size_t before = purrgo_logger_get_track_point_count();

    CHECK(add(11, 100, 10));                        /* первый шаг после поворота на восток: отклонение 10 м >= 5 м */
    CHECK(purrgo_logger_get_track_point_count() == before + 1);
    /*
     * Хорда последнего отрезка (от последней точки до первой за поворотом) ещё
     * идёт почти на север, поэтому следующая точка тоже заметна и пишется; после
     * неё направление - на восток, и дальше прямо на восток точек нет.
     */
    CHECK(add(12, 100, 20));
    CHECK(!add(13, 100, 30));
    CHECK(!add(14, 100, 40));
    purrgo_logger_stop();
}

static void test_small_deviation_is_ignored(void)
{
    begin(LOGGER_MODE_STANDARD);
    walk_north(1, 5);
    /* Шум приёмника в пределах допуска (3 м вбок) трек не меняет. */
    CHECK(!add(6, 60, 3));
    CHECK(!add(7, 70, -3));
    /* А 6 м вбок - уже заметно. */
    CHECK(add(8, 80, 6));
    purrgo_logger_stop();
}

static void test_there_and_back_keeps_turnaround(void)
{
    /*
     * Прогулка к смотровой и обратно по той же тропе: поперечного отклонения нет,
     * но точка разворота обязана остаться в треке (с точностью до допуска).
     */
    begin(LOGGER_MODE_STANDARD);
    walk_north(1, 20);                              /* 200 м вперёд */
    int turned = 0;
    for (int k = 19; k >= 0; k--) {                 /* и обратно к старту */
        turned += add((uint32_t)(21 + (19 - k)), k * 10, 0);
    }
    CHECK(turned >= 1);

    int32_t max_lat = LAT0;
    track_point_t p;
    for (size_t i = 0; i < purrgo_logger_get_track_point_count(); i++) {
        CHECK(purrgo_logger_get_track_point(i, &p));
        if (p.lat_1e7 > max_lat) { max_lat = p.lat_1e7; }
    }
    CHECK(max_lat >= LAT0 + 190 * PURRGO_1E7_PER_METER);   /* пик не срезан дальше чем на шаг */

    /* Движение назад по прямой дальше точками не покрывается. */
    size_t cnt = purrgo_logger_get_track_point_count();
    CHECK(!add(60, -100, 0) || purrgo_logger_get_track_point_count() == cnt + 1);
    purrgo_logger_stop();
}

static void test_expedition_has_wider_tolerance(void)
{
    /* Отклонение 8 м: в STANDARD (допуск 5 м) заметно, в EXPEDITION (допуск 10 м) - нет. */
    begin(LOGGER_MODE_STANDARD);
    walk_north(1, 3);
    CHECK(add(4, 40, 8));
    purrgo_logger_stop();

    begin(LOGGER_MODE_EXPEDITION);
    walk_north(1, 3);                               /* 10 м: вторая точка есть, направление известно */
    CHECK(!add(4, 40, 8));
    CHECK(add(5, 50, 12));                          /* 12 м от линии - уже да */
    purrgo_logger_stop();
}

static void test_expedition_min_step(void)
{
    begin(LOGGER_MODE_EXPEDITION);
    CHECK(!add(1, 7, 0));                           /* 7 м < 10 м: шум */
    CHECK(add(2, 12, 0));                           /* 12 м: направление неизвестно - пишется */
    purrgo_logger_stop();
}

static void test_max_chord(void)
{
    begin(LOGGER_MODE_EXPEDITION);
    CHECK(walk_north(1, 1) == 1);                   /* 10 м: направление */
    int n = walk_north(2, 99);                      /* до 990 м по прямой */
    CHECK(n == 0);                                  /* хорда < 1 км: точек нет */
    CHECK(add(100, 1010, 0));                       /* >= 1 км от последней записанной (10 м) */
    purrgo_logger_stop();
}

static void test_timeout_and_direction_reset(void)
{
    begin(LOGGER_MODE_STANDARD);
    CHECK(add(1, 10, 0));                           /* #2: направление север */

    /* Стоим: точка по таймауту (300 с) записывается, хорда короче шага - направление теряется. */
    purrgo_gnss_solution_t f = mk(302, 10, 1);
    f.speed_knots = 0;
    CHECK(purrgo_logger_add_point(&f));

    /* Тронулись на восток. Направление неизвестно, поэтому первая точка за пределами шума пишется ... */
    CHECK(add(303, 10, 10));
    /* ... а дальше прямо на восток - нет. */
    CHECK(!add(304, 10, 20));
    CHECK(!add(305, 10, 30));
    purrgo_logger_stop();
}

static void test_outlier_reset_drops_direction(void)
{
    begin(LOGGER_MODE_STANDARD);
    CHECK(add(1, 10, 0));
    CHECK(add(2, 20, 8));                           /* заметный поворот: пишется */

    /* Пять согласованных "выбросов" на ~111 км: пятый становится новой опорной точкой. */
    for (int s = 3; s <= 6; s++) {
        purrgo_gnss_solution_t f = mk((uint32_t)s, 0, 0);
        f.lat_1e7 += 1000000;
        CHECK(!purrgo_logger_add_point(&f));
    }
    purrgo_gnss_solution_t f = mk(7, 0, 0);
    f.lat_1e7 += 1000000;
    CHECK(purrgo_logger_add_point(&f));

    /* Старое направление отброшено: первая точка дальше шума пишется независимо от курса, затем прямая не пишется. */
    f = mk(8, 12, 0); f.lat_1e7 += 1000000;
    CHECK(purrgo_logger_add_point(&f));
    f = mk(9, 24, 0); f.lat_1e7 += 1000000;
    CHECK(!purrgo_logger_add_point(&f));
    purrgo_logger_stop();
}

static void test_antipodal_chord_no_overflow(void)
{
    /*
     * Хорда в пол-Земли (допустимая по скорости: 12 ч): векторы вне диапазона
     * целочисленной арифметики считаются изменением, без переполнения (UBSan).
     */
    purrgo_logger_reset();
    fake_fs_reset();
    app_config.tz_offset_minutes = 0;
    purrgo_logger_set_mode(LOGGER_MODE_STANDARD);

    purrgo_gnss_solution_t a = mk(0, 0, 0);
    a.lat_1e7 = 0; a.lon_1e7 = 0;
    CHECK(purrgo_logger_start(&a) && purrgo_logger_add_point(&a));
    purrgo_gnss_solution_t b = a;
    b.hours = 0; b.minutes = 5;                     /* #2 рядом: направление известно */
    b.lat_1e7 = 1000;
    CHECK(purrgo_logger_add_point(&b));

    purrgo_gnss_solution_t c = a;
    c.hours = 12; c.lon_1e7 = 1799999999;           /* ~20 000 км за 12 ч: 463 м/с, не выброс */
    c.lat_1e7 = 1000;
    CHECK(purrgo_logger_add_point(&c));             /* по таймауту */

    purrgo_gnss_solution_t d = c;
    d.hours = 12; d.minutes = 1;
    d.lat_1e7 = 2000;
    CHECK(purrgo_logger_add_point(&d));             /* направление известно, вектор огромный */
    purrgo_logger_stop();
}

static void test_gpx_matches_ram_track(void)
{
    begin(LOGGER_MODE_STANDARD);
    walk_north(1, 60);
    add(61, 600, 20);
    char name[64];
    snprintf(name, sizeof name, "%s", purrgo_logger_get_active_filename());
    size_t n = purrgo_logger_get_track_point_count();
    purrgo_logger_stop();

    uint32_t sz;
    const uint8_t *d = fake_fs_data(name, &sz);
    CHECK(d != NULL);
    if (d) {
        char *t = malloc((size_t)sz + 1u);
        memcpy(t, d, sz);
        t[sz] = 0;
        int c = 0;
        for (const char *q = t; (q = strstr(q, "<trkpt")); q += 6) { c++; }
        CHECK((size_t)c == n);
        CHECK(strstr(t, "</gpx>") != NULL);
        free(t);
    }
}

int main(void)
{
    test_straight_line_is_thinned();
    test_speed_change_is_not_a_change();
    test_corner_is_recorded();
    test_small_deviation_is_ignored();
    test_there_and_back_keeps_turnaround();
    test_expedition_has_wider_tolerance();
    test_expedition_min_step();
    test_max_chord();
    test_timeout_and_direction_reset();
    test_outlier_reset_drops_direction();
    test_antipodal_chord_no_overflow();
    test_gpx_matches_ram_track();
    return test_finish("test_track_filter");
}
