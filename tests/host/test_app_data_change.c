/*
 * "Заметные изменения данных" (src/core/ui/app_data_change.c): когда изменение
 * позиции, скорости, высоты, числа спутников и цели навигации считается поводом
 * перерисовать экран.
 *   - позиция: эталон на карте - нарисованный маркер, на других экранах - позиция,
 *     уже отправленная на вывод; порог PURRGO_GNSS_MARKER_UPDATE_DISTANCE_M, потеря и
 *     появление фикса - всегда заметны;
 *   - Trip computer: пороги скорости/высоты, любое изменение числа спутников,
 *     цель навигации (валидность, прибытие, расстояние, пеленг через 0/360);
 *   - снимок цели навигации запоминается сразу, остальное - только в _commit.
 */
#include <string.h>

#include "purrgo/geo.h"
#include "ui/app_data_change.h"
#include "test_common.h"

#define LAT0 531000000     /* 53.1000000 */
#define LON0 275000000     /* 27.5000000 */

static purrgo_gnss_solution_t fix_at(int32_t lat, int32_t lon)
{
    purrgo_gnss_solution_t f;
    memset(&f, 0, sizeof f);
    f.valid = true;
    f.lat_1e7 = lat;
    f.lon_1e7 = lon;
    return f;
}

static purrgo_wpt_nav_t wpt(bool valid, uint32_t dist, uint16_t bearing, bool arrived)
{
    purrgo_wpt_nav_t w;
    memset(&w, 0, sizeof w);
    w.valid = valid;
    w.nav.distance_to_wp_m = dist;
    w.nav.bearing_to_wp_deg = bearing;
    w.nav.is_arrived = arrived;
    return w;
}

/* Наименьший сдвиг по широте (в 1e-7 град), при котором geo считает расстояние >= порога. */
static int32_t lat_shift_for_threshold(void)
{
    for (int32_t d = 1; d < 5000; d++) {
        if (purrgo_geo_distance_m(LAT0, LON0, LAT0 + d, LON0) >= PURRGO_GNSS_MARKER_UPDATE_DISTANCE_M) {
            return d;
        }
    }
    return -1;
}

static void test_pos_without_reference(void)
{
    purrgo_app_data_change_t dc = {0};
    purrgo_gnss_solution_t fix = fix_at(LAT0, LON0);
    purrgo_gnss_solution_t nofix = fix_at(LAT0, LON0);
    nofix.valid = false;

    /* Эталона нет: появление фикса заметно, отсутствие фикса - нет (убирать нечего). */
    CHECK(purrgo_app_data_change_pos_moved(&dc, true, &fix));
    CHECK(purrgo_app_data_change_pos_moved(&dc, false, &fix));
    CHECK(!purrgo_app_data_change_pos_moved(&dc, true, &nofix));
    CHECK(!purrgo_app_data_change_pos_moved(&dc, false, &nofix));
}

static void test_pos_threshold(void)
{
    purrgo_app_data_change_t dc = {0};
    const int32_t d = lat_shift_for_threshold();
    CHECK(d > 1);

    purrgo_gnss_solution_t ref = fix_at(LAT0, LON0);
    purrgo_app_data_change_commit(&dc, &ref);   /* показанная позиция (не на карте) */
    purrgo_app_data_change_marker_rendered(&dc, &ref);   /* маркер (на карте) */

    purrgo_gnss_solution_t same = fix_at(LAT0, LON0);
    purrgo_gnss_solution_t below = fix_at(LAT0 + d - 1, LON0);
    purrgo_gnss_solution_t at = fix_at(LAT0 + d, LON0);
    purrgo_gnss_solution_t far_south = fix_at(LAT0 - 10 * d, LON0);   /* порог симметричен */

    for (int on_map = 0; on_map <= 1; on_map++) {
        CHECK(!purrgo_app_data_change_pos_moved(&dc, on_map, &same));
        CHECK(!purrgo_app_data_change_pos_moved(&dc, on_map, &below));   /* порог не достигнут */
        CHECK(purrgo_app_data_change_pos_moved(&dc, on_map, &at));       /* >= : ровно порог заметен */
        CHECK(purrgo_app_data_change_pos_moved(&dc, on_map, &far_south));
    }
}

static void test_pos_lost(void)
{
    purrgo_app_data_change_t dc = {0};
    purrgo_gnss_solution_t ref = fix_at(LAT0, LON0);
    purrgo_gnss_solution_t nofix = fix_at(LAT0, LON0);
    nofix.valid = false;

    purrgo_app_data_change_commit(&dc, &ref);
    purrgo_app_data_change_marker_rendered(&dc, &ref);

    CHECK(purrgo_app_data_change_pos_moved(&dc, true, &nofix));    /* маркер нужно убрать */
    CHECK(purrgo_app_data_change_pos_moved(&dc, false, &nofix));

    /* Уже показан "нет фикса": повторное отсутствие не заметно. */
    purrgo_app_data_change_commit(&dc, &nofix);
    purrgo_app_data_change_marker_rendered(&dc, &nofix);
    CHECK(!purrgo_app_data_change_pos_moved(&dc, true, &nofix));
    CHECK(!purrgo_app_data_change_pos_moved(&dc, false, &nofix));
}

static void test_reference_depends_on_screen(void)
{
    purrgo_app_data_change_t dc = {0};
    const int32_t d = lat_shift_for_threshold();
    purrgo_gnss_solution_t marker = fix_at(LAT0, LON0);
    purrgo_gnss_solution_t shown = fix_at(LAT0 + 100 * d, LON0);   /* далеко от маркера */

    purrgo_app_data_change_marker_rendered(&dc, &marker);
    purrgo_app_data_change_commit(&dc, &shown);

    /* Текущая позиция совпадает с маркером: на карте движения нет, на других экранах - есть. */
    CHECK(!purrgo_app_data_change_pos_moved(&dc, true, &marker));
    CHECK(purrgo_app_data_change_pos_moved(&dc, false, &marker));

    /* И наоборот: совпадает с показанной позицией. */
    CHECK(purrgo_app_data_change_pos_moved(&dc, true, &shown));
    CHECK(!purrgo_app_data_change_pos_moved(&dc, false, &shown));

    /*
     * Валидность эталона тоже зависит от экрана: маркер "забыт" (экран перерисован без
     * него), а показанная позиция действительна. На карте эталона нет - позиция заметна;
     * на других экранах позиция уже показана - изменения нет.
     */
    purrgo_app_data_change_forget_marker(&dc);
    CHECK(purrgo_app_data_change_pos_moved(&dc, true, &shown));
    CHECK(!purrgo_app_data_change_pos_moved(&dc, false, &shown));

    /* Обратно: маркер нарисован, показанной позиции нет. */
    purrgo_app_data_change_marker_rendered(&dc, &marker);
    purrgo_app_data_change_forget_pos(&dc);
    CHECK(!purrgo_app_data_change_pos_moved(&dc, true, &marker));
    CHECK(purrgo_app_data_change_pos_moved(&dc, false, &marker));
}

static void test_marker_rendered_and_forget(void)
{
    purrgo_app_data_change_t dc = {0};
    purrgo_gnss_solution_t fix = fix_at(LAT0, LON0);
    purrgo_gnss_solution_t nofix = fix_at(LAT0, LON0);
    nofix.valid = false;

    purrgo_app_data_change_marker_rendered(&dc, &fix);
    CHECK(dc.marker_valid && dc.marker_lat_1e7 == LAT0 && dc.marker_lon_1e7 == LON0);

    purrgo_app_data_change_marker_rendered(&dc, &nofix);   /* маркера нет */
    CHECK(!dc.marker_valid);

    purrgo_app_data_change_marker_rendered(&dc, &fix);
    purrgo_app_data_change_marker_rendered(&dc, NULL);     /* NULL - тоже "маркера нет" */
    CHECK(!dc.marker_valid);

    purrgo_app_data_change_marker_rendered(&dc, &fix);
    purrgo_app_data_change_commit(&dc, &fix);
    purrgo_app_data_change_forget_marker(&dc);
    CHECK(!dc.marker_valid && dc.pos_valid);   /* показанная позиция не затронута */
    purrgo_app_data_change_forget_pos(&dc);
    CHECK(!dc.pos_valid);
}

static void test_reset_keeps_marker(void)
{
    purrgo_app_data_change_t dc = {0};
    purrgo_gnss_solution_t fix = fix_at(LAT0, LON0);

    purrgo_app_data_change_marker_rendered(&dc, &fix);
    purrgo_app_data_change_commit(&dc, &fix);
    purrgo_app_data_change_reset(&dc);

    CHECK(!dc.pos_valid);
    CHECK(dc.marker_valid);   /* маркер реально нарисован на панели и переживает init */
}

static void test_trip_speed_alt_satellites(void)
{
    purrgo_app_data_change_t dc = {0};
    const purrgo_wpt_nav_t w = wpt(false, 0, 0, false);
    purrgo_gnss_solution_t fix = fix_at(LAT0, LON0);
    fix.speed_knots = 500;
    fix.alt_m = 100;
    fix.satellites_tracked = 8;
    purrgo_app_data_change_commit(&dc, &fix);

    CHECK(!purrgo_app_data_change_trip_changed(&dc, &fix, &w));

    fix.speed_knots = 500 + PURRGO_UI_SPEED_THRESHOLD_KNOTS_100 - 1;
    CHECK(!purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    fix.speed_knots = 500 + PURRGO_UI_SPEED_THRESHOLD_KNOTS_100;
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    fix.speed_knots = 500 - PURRGO_UI_SPEED_THRESHOLD_KNOTS_100;   /* модуль разности */
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    fix.speed_knots = 500;

    fix.alt_m = 100 + PURRGO_UI_ALT_THRESHOLD_M - 1;
    CHECK(!purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    fix.alt_m = 100 + PURRGO_UI_ALT_THRESHOLD_M;
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    fix.alt_m = 100 - PURRGO_UI_ALT_THRESHOLD_M;
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    fix.alt_m = 100;

    fix.satellites_tracked = 9;   /* любое изменение числа спутников заметно */
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    fix.satellites_tracked = 7;
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));
}

static void test_trip_values_latched_only_by_commit(void)
{
    purrgo_app_data_change_t dc = {0};
    const purrgo_wpt_nav_t w = wpt(false, 0, 0, false);
    purrgo_gnss_solution_t fix = fix_at(LAT0, LON0);
    fix.speed_knots = 300;

    /* Без commit изменение скорости остаётся "заметным" при каждом опросе. */
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));

    purrgo_app_data_change_commit(&dc, &fix);
    CHECK(dc.speed_knots_100 == 300 && dc.pos_valid && dc.lat_1e7 == LAT0 && dc.lon_1e7 == LON0);
    CHECK(!purrgo_app_data_change_trip_changed(&dc, &fix, &w));
}

static void test_trip_waypoint(void)
{
    purrgo_app_data_change_t dc = {0};
    const purrgo_gnss_solution_t fix = fix_at(LAT0, LON0);
    purrgo_app_data_change_commit(&dc, &fix);

    /* Цель не задана и не была показана: ничего не меняется, расстояние игнорируется. */
    purrgo_wpt_nav_t none = wpt(false, 12345, 77, true);
    CHECK(!purrgo_app_data_change_trip_changed(&dc, &fix, &none));

    /* Цель появилась: заметно, снимок запомнен сразу (без commit). */
    purrgo_wpt_nav_t w = wpt(true, 1000, 90, false);
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    CHECK(dc.wpt_valid && dc.wpt_dist_m == 1000U && dc.wpt_bearing_deg == 90U && !dc.wpt_arrived);
    CHECK(!purrgo_app_data_change_trip_changed(&dc, &fix, &w));   /* второй опрос - тишина */

    /* Расстояние: порог. Снимок не сдвигается, пока порог не достигнут. */
    w = wpt(true, 1000 + PURRGO_UI_WPT_DIST_THRESHOLD_M - 1, 90, false);
    CHECK(!purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    CHECK(dc.wpt_dist_m == 1000U);
    w = wpt(true, 1000 + PURRGO_UI_WPT_DIST_THRESHOLD_M, 90, false);
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    CHECK(dc.wpt_dist_m == 1000U + PURRGO_UI_WPT_DIST_THRESHOLD_M);
    w = wpt(true, 1000, 90, false);   /* в обратную сторону - тоже по модулю */
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));

    /* Пеленг: порог по модулю. */
    w = wpt(true, 1000, 90 + PURRGO_UI_WPT_BEARING_THRESHOLD_DEG - 1, false);
    CHECK(!purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    w = wpt(true, 1000, 90 + PURRGO_UI_WPT_BEARING_THRESHOLD_DEG, false);
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));

    /* Прибытие. */
    w = wpt(true, 1000, 95, true);
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    CHECK(dc.wpt_arrived);

    /* Цель снята. */
    w = wpt(false, 0, 0, false);
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    CHECK(!dc.wpt_valid);
    CHECK(!purrgo_app_data_change_trip_changed(&dc, &fix, &w));
}

static void test_trip_bearing_wraps_through_north(void)
{
    purrgo_app_data_change_t dc = {0};
    const purrgo_gnss_solution_t fix = fix_at(LAT0, LON0);
    purrgo_app_data_change_commit(&dc, &fix);

    purrgo_wpt_nav_t w = wpt(true, 500, 359, false);
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));   /* цель появилась */

    /* 359 -> 2: на деле 3 градуса, а не 357: ниже порога. */
    w = wpt(true, 500, 2, false);
    CHECK(!purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    /* 359 -> 4: 5 градусов через север - порог достигнут. */
    w = wpt(true, 500, 4, false);
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    /* 4 -> 359: в обратную сторону - тот же результат. */
    w = wpt(true, 500, 359, false);
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    /* 359 -> 179: развернулись на 180 градусов. */
    w = wpt(true, 500, 179, false);
    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));
}

static void test_trip_waypoint_and_speed_both_evaluated(void)
{
    /* Изменение скорости не должно мешать запомнить цель: обе проверки выполняются всегда. */
    purrgo_app_data_change_t dc = {0};
    purrgo_gnss_solution_t fix = fix_at(LAT0, LON0);
    fix.speed_knots = 900;
    const purrgo_wpt_nav_t w = wpt(true, 250, 45, false);

    CHECK(purrgo_app_data_change_trip_changed(&dc, &fix, &w));
    CHECK(dc.wpt_valid && dc.wpt_dist_m == 250U && dc.wpt_bearing_deg == 45U);
}

int main(void)
{
    test_pos_without_reference();
    test_pos_threshold();
    test_pos_lost();
    test_reference_depends_on_screen();
    test_marker_rendered_and_forget();
    test_reset_keeps_marker();
    test_trip_speed_alt_satellites();
    test_trip_values_latched_only_by_commit();
    test_trip_waypoint();
    test_trip_bearing_wraps_through_north();
    test_trip_waypoint_and_speed_both_evaluated();
    return test_finish("test_app_data_change");
}
