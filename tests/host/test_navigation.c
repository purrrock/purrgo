/*
 * Host-тесты расчёта расстояния и азимута до точки (geo.c) и обёртки
 * purrgo_nav_update() (navigation.c): эталонные пары, стороны света, переход
 * через 180-й меридиан, совпадающие точки, невалидный фикс, радиус прибытия.
 *
 * Эталоны посчитаны независимо (гаверсинус и начальный азимут на сфере
 * R = 6371 км). Расчёт в прошивке целочисленный (LUT косинуса и арктангенса),
 * поэтому допуски: расстояние 3 м + 0,5 %, азимут 2 градуса.
 */
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "purrgo/geo.h"
#include "purrgo/navigation.h"
#include "test_common.h"

static int32_t deg_1e7(double d)
{
    return (int32_t)(d * 10000000.0 + (d >= 0 ? 0.5 : -0.5));
}

static int near_dist(uint32_t got, double ref)
{
    const double tol = 3.0 + ref * 0.005;
    const double diff = (double)got - ref;

    return diff <= tol && diff >= -tol;
}

/* Разность азимутов по кругу, градусы. */
static int az_diff(int a, double b)
{
    double d = (double)a - b;

    while (d > 180.0) d -= 360.0;
    while (d < -180.0) d += 360.0;
    return (int)(d < 0 ? -d : d + 0.999);   /* вверх: допуск строгий */
}

typedef struct {
    double lat1, lon1, lat2, lon2;
    double ref_dist_m;
    double ref_az_deg;
    const char *what;
} pair_t;

static const pair_t k_pairs[] = {
    { 53.9006,  27.5590,  52.4345,  30.9754,  280019.7, 124.2, "Минск - Гомель" },
    {  0.0,      0.0,       1.0,      0.0,     111194.9,   0.0, "1 градус на север" },
    {  0.0,      0.0,       0.0,      1.0,     111194.9,  90.0, "1 градус на восток" },
    {  0.0,      0.0,      -1.0,     -1.0,     157249.4, 225.0, "юго-запад на экваторе" },
    { 53.9006,  27.5590,   53.9015,  27.5590,     100.1,   0.0, "100 м на север" },
    { 53.9006,  27.5590,   53.9006,  27.5605,      98.3,  90.0, "100 м на восток" },
    { 80.0,     10.0,      80.05,    11.0,     20047.1,  73.4, "80 с.ш." },
    {-33.7,    151.1,     -33.8,    151.2,     14461.1, 140.3, "южное полушарие" },
    {  0.0,    179.9,       0.0,   -179.9,     22239.0,  90.0, "через 180 меридиан на восток" },
    {  0.0,   -179.9,       0.0,    179.9,     22239.0, 270.0, "через 180 меридиан на запад" },
    { 60.0,   -179.0,      60.05,   179.0,    111245.6, 273.7, "через 180 меридиан, 60 с.ш." },
    {-90.0,      0.0,      90.0,      0.0,   20015086.8,   0.0, "от полюса до полюса" },
};

static void test_reference_pairs(void)
{
    for (size_t i = 0; i < sizeof k_pairs / sizeof k_pairs[0]; i++) {
        const pair_t *p = &k_pairs[i];
        const int32_t a = deg_1e7(p->lat1), b = deg_1e7(p->lon1);
        const int32_t c = deg_1e7(p->lat2), d = deg_1e7(p->lon2);
        const uint32_t dist = purrgo_geo_distance_m(a, b, c, d);
        const uint16_t az = purrgo_geo_azimuth_deg(a, b, c, d);

        if (!near_dist(dist, p->ref_dist_m)) {
            printf("  расстояние [%s]: %u, ожидалось %.1f\n", p->what, (unsigned)dist, p->ref_dist_m);
        }
        CHECK(near_dist(dist, p->ref_dist_m));

        if (p->ref_dist_m > 1000000.0) {
            continue;       /* для полюсов азимут не определён */
        }
        if (az_diff((int)az, p->ref_az_deg) > 2) {
            printf("  азимут [%s]: %u, ожидалось %.1f\n", p->what, (unsigned)az, p->ref_az_deg);
        }
        CHECK(az_diff((int)az, p->ref_az_deg) <= 2);
        CHECK(az < 360);
    }
}

static void test_cardinals_exact(void)
{
    const int32_t o = deg_1e7(50.0);
    const int32_t s = 10000;        /* 0,001 градуса */

    CHECK(purrgo_geo_azimuth_deg(o, o,  o + s, o) == 0);
    CHECK(purrgo_geo_azimuth_deg(o, o,  o, o + s) == 90);
    CHECK(purrgo_geo_azimuth_deg(o, o,  o - s, o) == 180);
    CHECK(purrgo_geo_azimuth_deg(o, o,  o, o - s) == 270);
}

static void test_azimuth_sweep(void)
{
    /* Точка на окружности радиусом ~1 км вокруг центра: азимут совпадает с углом точки. */
    const int32_t lat0 = deg_1e7(55.0), lon0 = deg_1e7(37.0);

    for (int a = 0; a < 360; a += 5) {
        /* dlat = cos(a), dlon = sin(a) / cos(lat); 1 градус 1e7 = 111 км. */
        const double rad = (double)a * 3.14159265358979 / 180.0;
        const int32_t dlat = (int32_t)(90000.0 * cos(rad));
        const int32_t dlon = (int32_t)(90000.0 * sin(rad) / 0.573576);   /* cos 55 */
        const int az = (int)purrgo_geo_azimuth_deg(lat0, lon0, lat0 + dlat, lon0 + dlon);

        CHECK(az_diff(az, (double)a) <= 2);
    }
}

static void test_coincident_points(void)
{
    CHECK(purrgo_geo_distance_m(500000000, 100000000, 500000000, 100000000) == 0);
    CHECK(purrgo_geo_azimuth_deg(500000000, 100000000, 500000000, 100000000) == 0);
    CHECK(purrgo_geo_distance_m(0, 0, 0, 0) == 0);
}

static void test_symmetry(void)
{
    const int32_t a = deg_1e7(53.9006), b = deg_1e7(27.5590);
    const int32_t c = deg_1e7(52.4345), d = deg_1e7(30.9754);
    const uint32_t d1 = purrgo_geo_distance_m(a, b, c, d);
    const uint32_t d2 = purrgo_geo_distance_m(c, d, a, b);
    const int diff = (int)d1 - (int)d2;

    CHECK(diff >= -5 && diff <= 5);
}

static purrgo_gnss_solution_t fix_at(double lat, double lon, bool valid)
{
    purrgo_gnss_solution_t f;

    memset(&f, 0, sizeof f);
    f.valid = valid;
    f.lat_1e7 = deg_1e7(lat);
    f.lon_1e7 = deg_1e7(lon);
    return f;
}

static purrgo_waypoint_t wp_at(double lat, double lon)
{
    purrgo_waypoint_t w;

    memset(&w, 0, sizeof w);
    w.lat_1e7 = deg_1e7(lat);
    w.lon_1e7 = deg_1e7(lon);
    return w;
}

static void test_nav_update_basic(void)
{
    const purrgo_gnss_solution_t fix = fix_at(53.9006, 27.5590, true);
    const purrgo_waypoint_t wp = wp_at(52.4345, 30.9754);
    purrgo_nav_status_t st;

    memset(&st, 0, sizeof st);
    purrgo_nav_update(&fix, &wp, 15, &st);

    CHECK(near_dist(st.distance_to_wp_m, 280019.7));
    CHECK(az_diff((int)st.bearing_to_wp_deg, 124.2) <= 2);
    CHECK(!st.is_arrived);
}

static void test_nav_update_ignores_invalid(void)
{
    const purrgo_waypoint_t wp = wp_at(52.4345, 30.9754);
    purrgo_gnss_solution_t fix = fix_at(53.9006, 27.5590, false);
    purrgo_nav_status_t st = { 777u, 123u, true };

    /* Нет фикса: статус не меняется (экран покажет "---" по флагу, а не мусор). */
    purrgo_nav_update(&fix, &wp, 15, &st);
    CHECK(st.distance_to_wp_m == 777u && st.bearing_to_wp_deg == 123u && st.is_arrived);

    /* Нулевые указатели не падают. */
    fix.valid = true;
    purrgo_nav_update(NULL, &wp, 15, &st);
    purrgo_nav_update(&fix, NULL, 15, &st);
    purrgo_nav_update(&fix, &wp, 15, NULL);
    CHECK(st.distance_to_wp_m == 777u);
}

static void test_arrival_radius(void)
{
    const purrgo_waypoint_t wp = wp_at(53.9006, 27.5590);
    purrgo_nav_status_t st;
    memset(&st, 0, sizeof st);

    /* Точно в цели. */
    purrgo_gnss_solution_t fix = fix_at(53.9006, 27.5590, true);
    purrgo_nav_update(&fix, &wp, 15, &st);
    CHECK(st.distance_to_wp_m == 0 && st.is_arrived);

    /* Нулевой радиус: совпадение координат всё равно прибытие. */
    purrgo_nav_update(&fix, &wp, 0, &st);
    CHECK(st.is_arrived);

    /* ~11 м к северу (0,0001 градуса): внутри 15 м. */
    fix = fix_at(53.9007, 27.5590, true);
    purrgo_nav_update(&fix, &wp, 15, &st);
    CHECK(st.distance_to_wp_m >= 10 && st.distance_to_wp_m <= 12);
    CHECK(st.is_arrived);

    /* Граница включительная: радиус равен расстоянию - прибыли, на метр меньше - нет. */
    const uint32_t d = st.distance_to_wp_m;
    purrgo_nav_update(&fix, &wp, d, &st);
    CHECK(st.is_arrived);
    purrgo_nav_update(&fix, &wp, d - 1u, &st);
    CHECK(!st.is_arrived);

    /* ~111 м: вне радиуса, флаг снимается. */
    fix = fix_at(53.9016, 27.5590, true);
    purrgo_nav_update(&fix, &wp, 15, &st);
    CHECK(st.distance_to_wp_m >= 108 && st.distance_to_wp_m <= 114);
    CHECK(!st.is_arrived);
}

int main(void)
{
    test_reference_pairs();
    test_cardinals_exact();
    test_azimuth_sweep();
    test_coincident_points();
    test_symmetry();
    test_nav_update_basic();
    test_nav_update_ignores_invalid();
    test_arrival_radius();
    return test_finish("test_navigation");
}
