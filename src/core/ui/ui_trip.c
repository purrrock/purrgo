#include "ui_trip.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_text.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/config.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/pgo_text.h"
#include "purrgo/waypoint.h"
#include "purrgo/gfx_line.h"
#include "purrgo/gfx_circle.h"
#include <string.h>

/* Порядок строк (шаг 12 px, первая строка y = 10). */
#define TRIP_ROW_Y0    10
#define TRIP_ROW_STEP  12
#define TRIP_ROW_Y(i)  ((int16_t)(TRIP_ROW_Y0 + (i) * TRIP_ROW_STEP))

#define TRIP_ROW_LOC   0
#define TRIP_ROW_UTC   1
#define TRIP_ROW_SPD   2
#define TRIP_ROW_CRS   3
#define TRIP_ROW_WPT   4   /* имя цели; DST и BRG - следующие две строки */
#define TRIP_ROW_FIX   7   /* FIX и SAT */
#define TRIP_ROW_LAT   8
#define TRIP_ROW_LON   9
#define TRIP_ROW_ALT   10
#define TRIP_ROW_SUN   11  /* SUN, TO SUNSET/SUNRISE, DAY */

/*
 * Компас-стрелка на цель: круг правее значений DST / BRG / FIX (ниже имени цели,
 * чтобы имя могло занимать всю ширину). Окно очищается при каждой перерисовке.
 */
#define TRIP_ARROW_BOX_X 134
#define TRIP_ARROW_BOX_Y 70
#define TRIP_ARROW_BOX_W 42
#define TRIP_ARROW_BOX_H 36
#define TRIP_ARROW_CX    155
#define TRIP_ARROW_CY    88
#define TRIP_ARROW_RING  16   /* радиус круга */
#define TRIP_ARROW_R     12   /* радиус стрелки (остриё), внутри круга */

/* Ширина поля значения в символах (от x = 40 до края экрана 176). */
#define TRIP_VALUE_CHARS 22

/* Дополняет строку пробелами до width символов (стирает прежний текст при перерисовке). */
static void pad_to(char* buf, size_t size, size_t width)
{
    size_t n = strlen(buf);

    if (width + 1 > size) {
        width = size - 1;
    }
    if (n > width) {
        n = width;
        buf[n] = '\0';
    }
    while (n < width) {
        buf[n++] = ' ';
    }
    buf[n] = '\0';
}

/* Блок цели навигации: имя, расстояние, пеленг и стрелка в круге. y_pos - первая из трёх строк. */
static void ui_trip_render_wpt(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, int16_t y_pos)
{
    char buf[48];
    const purrgo_waypoint_t* wpt = purrgo_wpt_target();
    const purrgo_wpt_nav_t* nav = purrgo_wpt_nav();
    const int16_t val_x = 40;
    const bool nav_ok = (wpt != NULL && nav->valid);
    uint16_t angle = 0;
    const bool course_up = nav_ok &&
                           purrgo_wpt_arrow_angle(gnss, nav->nav.bearing_to_wp_deg, &angle);

    /* Окно стрелки очищается при каждой перерисовке. */
    gfx_set_color(gfx, 0, 3);     /* fill_rect заливает цветом фона (белый) */
    gfx_fill_rect(gfx, TRIP_ARROW_BOX_X, TRIP_ARROW_BOX_Y, TRIP_ARROW_BOX_W, TRIP_ARROW_BOX_H);
    gfx_set_color(gfx, 0, 3);

    /* Имя (UTF-8 из GPX -> PurrGO-256). */
    purrgo_snprintf(buf, sizeof(buf), "%s", (wpt != NULL) ? wpt->name : "NONE");
    purrgo_utf8_to_pgo_inplace(buf);
    pad_to(buf, sizeof(buf), TRIP_VALUE_CHARS);
    gfx_draw_string(gfx, val_x, y_pos, buf);
    y_pos = (int16_t)(y_pos + TRIP_ROW_STEP);

    /* Расстояние (правее - круг стрелки, поле ограничено 16 символами). */
    if (wpt != NULL && nav->valid) {
        char dist[16];

        purrgo_wpt_format_distance(dist, sizeof(dist), nav->nav.distance_to_wp_m);
        purrgo_snprintf(buf, sizeof(buf), "%s%s", dist, nav->nav.is_arrived ? " ARRIVED" : "");
    } else {
        purrgo_snprintf(buf, sizeof(buf), "%s", (wpt != NULL) ? "---" : "");
    }
    pad_to(buf, sizeof(buf), TRIP_VALUE_CHARS - 6);
    gfx_draw_string(gfx, val_x, y_pos, buf);

    /*
     * Подпись опоры стрелки слева от круга: CRS - вверх это направление движения,
     * N - вверх это север. При "ARRIVED" строка занята текстом, подписи нет.
     */
    if (nav_ok && !nav->nav.is_arrived) {
        gfx_draw_string(gfx, 116, y_pos, course_up ? "CRS" : "N  ");
    }
    y_pos = (int16_t)(y_pos + TRIP_ROW_STEP);

    /* Пеленг (истинный, от севера). */
    if (wpt != NULL && nav->valid) {
        purrgo_snprintf(buf, sizeof(buf), "%03d deg %s",
                        (int)nav->nav.bearing_to_wp_deg,
                        purrgo_wpt_cardinal(nav->nav.bearing_to_wp_deg));
    } else {
        purrgo_snprintf(buf, sizeof(buf), "%s", (wpt != NULL) ? "---" : "");
    }
    pad_to(buf, sizeof(buf), TRIP_VALUE_CHARS - 6);
    gfx_draw_string(gfx, val_x, y_pos, buf);

    /* Круг компаса: рисуется, пока цель задана; стрелка - когда есть навигационные данные. */
    if (wpt != NULL) {
        gfx_draw_circle(gfx, TRIP_ARROW_CX, TRIP_ARROW_CY, TRIP_ARROW_RING);
        gfx_draw_circle(gfx, TRIP_ARROW_CX, TRIP_ARROW_CY, (int16_t)(TRIP_ARROW_RING - 1));
    }

    /* Стрелка: вверх - направление движения (если курс надёжен), иначе север. */
    if (nav_ok) {
        purrgo_wpt_pt_t pts[4];

        purrgo_wpt_arrow_points(TRIP_ARROW_CX, TRIP_ARROW_CY, TRIP_ARROW_R, angle, pts);
        gfx_draw_thick_line(gfx, pts[0].x, pts[0].y, pts[1].x, pts[1].y, 2);
        gfx_draw_thick_line(gfx, pts[1].x, pts[1].y, pts[2].x, pts[2].y, 2);
        gfx_draw_thick_line(gfx, pts[1].x, pts[1].y, pts[3].x, pts[3].y, 2);
    }
}

void ui_trip_render_grid(gfx_context_t* gfx) {
    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);

    // Подписи строк: LOC, UTC, SPD, CRS, WPT, DST, BRG, FIX/SAT, LAT, LON, ALT.
    // Блок SUN / TO SUNSET / DAY целиком рисуется в ui_trip_render_values().
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_LOC), "LOC:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_UTC), "UTC:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_SPD), "SPD:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_CRS), "CRS:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_WPT), "WPT:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_WPT + 1), "DST:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_WPT + 2), "BRG:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_FIX), "FIX:");
    gfx_draw_string(gfx, 70, TRIP_ROW_Y(TRIP_ROW_FIX), "SAT:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_LAT), "LAT:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_LON), "LON:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_ALT), "ALT:");
}

void ui_trip_render_values(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, const purrgo_sun_info_t* sun) {
    char buf[64];
    gfx_set_color(gfx, 0, 3);

    const int16_t val_x = 40; // x offset for values after 4-char label like "UTC: " (10 + 4 * 6 + 6)
    const int16_t lat_lon_x = 40;

    // LOC
    int32_t total_mins = (int32_t)gnss->hours * 60 + (int32_t)gnss->minutes + app_config.tz_offset_minutes;
    while (total_mins < 0) total_mins += 1440;
    while (total_mins >= 1440) total_mins -= 1440;
    uint8_t loc_hours = (uint8_t)(total_mins / 60);
    uint8_t loc_minutes = (uint8_t)(total_mins % 60);
    purrgo_snprintf(buf, sizeof(buf), "%02d:%02d", loc_hours, loc_minutes);
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_LOC), buf);

    // UTC
    purrgo_snprintf(buf, sizeof(buf), "%02d:%02d", gnss->hours, gnss->minutes);
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_UTC), buf);

    // SPD
    int speed_kmh = (gnss->speed_knots * 1852) / 100000;
    purrgo_snprintf(buf, sizeof(buf), "%d km/h     ", speed_kmh);
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_SPD), buf);

    // CRS
    if (gnss->course_valid) {
        int course_deg = gnss->course_deg_100 / 100;
        int course_frac = (gnss->course_deg_100 > 0 ? gnss->course_deg_100 : -gnss->course_deg_100) % 100;
        purrgo_snprintf(buf, sizeof(buf), "%d.%02d     ", course_deg, course_frac);
    } else {
        purrgo_snprintf(buf, sizeof(buf), "N/A         ");
    }
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_CRS), buf);

    // WPT / DST / BRG и стрелка на цель (три строки)
    ui_trip_render_wpt(gfx, gnss, TRIP_ROW_Y(TRIP_ROW_WPT));

    // FIX & SAT
    purrgo_snprintf(buf, sizeof(buf), "%-3s", gnss->valid ? "3D" : "NO");
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_FIX), buf);

    purrgo_snprintf(buf, sizeof(buf), "%-2d", gnss->satellites_tracked);
    gfx_draw_string(gfx, 100, TRIP_ROW_Y(TRIP_ROW_FIX), buf); // "SAT: " is at 70, len 4 is 24px -> 94 + 6px space = 100

    // LAT
    int lat_deg = gnss->lat_1e7 / 10000000;
    int lat_frac = (gnss->lat_1e7 > 0 ? gnss->lat_1e7 : -gnss->lat_1e7) % 10000000;
    purrgo_snprintf(buf, sizeof(buf), "%d.%07d   ", lat_deg, lat_frac);
    gfx_draw_string(gfx, lat_lon_x, TRIP_ROW_Y(TRIP_ROW_LAT), buf);

    // LON
    int lon_deg = gnss->lon_1e7 / 10000000;
    int lon_frac = (gnss->lon_1e7 > 0 ? gnss->lon_1e7 : -gnss->lon_1e7) % 10000000;
    purrgo_snprintf(buf, sizeof(buf), "%d.%07d   ", lon_deg, lon_frac);
    gfx_draw_string(gfx, lat_lon_x, TRIP_ROW_Y(TRIP_ROW_LON), buf);

    // ALT
    purrgo_snprintf(buf, sizeof(buf), "%d m        ", (int)gnss->alt_m);
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_ALT), buf);

    int16_t y_pos = TRIP_ROW_Y(TRIP_ROW_SUN);

    // SUN INFO (No grid text split here, as this part varies completely based on state.
    // Usually, we would split it, but it's easier to just draw it whole here, overwriting
    // the area. To make sure it clears properly, we'll pad with spaces.)
    if (sun != NULL) {
        // Clear remaining lines
        for (int i = 0; i < 4; i++) {
             gfx_draw_string(gfx, 10, (int16_t)(y_pos + i*12), "                              ");
        }

        if (sun->status == SUN_STATUS_NORMAL) {
            purrgo_snprintf(buf, sizeof(buf), "SUN: %02d:%02d-%02d:%02d",
                sun->sunrise_hour, sun->sunrise_minute,
                sun->sunset_hour, sun->sunset_minute);
            gfx_draw_string(gfx, 10, y_pos, buf);
            y_pos = (int16_t)(y_pos + 12);

            int remain_h = sun->time_to_event_min / 60;
            int remain_m = sun->time_to_event_min % 60;
            if (sun->is_daytime) {
                purrgo_snprintf(buf, sizeof(buf), "TO SUNSET: %02dh %02dm", remain_h, remain_m);
            } else {
                purrgo_snprintf(buf, sizeof(buf), "TO SUNRISE: %02dh %02dm", remain_h, remain_m);
            }
            gfx_draw_string(gfx, 10, y_pos, buf);
            y_pos = (int16_t)(y_pos + 12);

            int start_min = sun->sunrise_hour * 60 + sun->sunrise_minute;
            int end_min = sun->sunset_hour * 60 + sun->sunset_minute;
            int total_day_min = end_min - start_min;
            if (total_day_min < 0) total_day_min += 1440;

            purrgo_snprintf(buf, sizeof(buf), "DAY: %02dh %02dm", total_day_min / 60, total_day_min % 60);
            gfx_draw_string(gfx, 10, y_pos, buf);
            y_pos = (int16_t)(y_pos + 12);

        } else if (sun->status == SUN_STATUS_POLAR_DAY) {
            gfx_draw_string(gfx, 10, y_pos, "POLAR DAY");
            y_pos = (int16_t)(y_pos + 12);
            gfx_draw_string(gfx, 10, y_pos, "NO SUNSET DAY: 24h 00m");
            y_pos = (int16_t)(y_pos + 12);
        } else if (sun->status == SUN_STATUS_POLAR_NIGHT) {
            gfx_draw_string(gfx, 10, y_pos, "POLAR NIGHT");
            y_pos = (int16_t)(y_pos + 12);
            gfx_draw_string(gfx, 10, y_pos, "NO SUNRISE DAY: 00h 00m");
            y_pos = (int16_t)(y_pos + 12);
        }
    }
}
