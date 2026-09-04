#include "ui_trip.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_text.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/config.h"
#include <stdio.h>

void ui_trip_render_grid(gfx_context_t* gfx) {
    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);

    int y_pos = 10;

    // Draw grid labels
    gfx_draw_string(gfx, 10, y_pos, "UTC:");
    y_pos += 12;
    gfx_draw_string(gfx, 10, y_pos, "LOC:");
    y_pos += 12;
    gfx_draw_string(gfx, 10, y_pos, "FIX:");
    gfx_draw_string(gfx, 50, y_pos, "SAT:");
    y_pos += 12;
    gfx_draw_string(gfx, 10, y_pos, "LAT:");
    y_pos += 12;
    gfx_draw_string(gfx, 10, y_pos, "LON:");
    y_pos += 12;
    gfx_draw_string(gfx, 10, y_pos, "ALT:");
    y_pos += 12;
    gfx_draw_string(gfx, 10, y_pos, "SPD:");
    y_pos += 12;
    gfx_draw_string(gfx, 10, y_pos, "CRS:");
    y_pos += 12;
}

void ui_trip_render_values(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, const purrgo_sun_info_t* sun) {
    char buf[64];
    gfx_set_color(gfx, 0, 3);

    int y_pos = 10;
    int val_x = 40; // standard x offset for values after 4-char label like "UTC: " (10 + 4 * 6 + 6)
    int lat_lon_x = 40; // standard x offset for LAT/LON

    // UTC
    snprintf(buf, sizeof(buf), "%02d:%02d", gnss->hours, gnss->minutes);
    gfx_draw_string(gfx, val_x, y_pos, buf);
    y_pos += 12;

    // LOC
    int32_t total_mins = (int32_t)gnss->hours * 60 + (int32_t)gnss->minutes + app_config.tz_offset_minutes;
    while (total_mins < 0) total_mins += 1440;
    while (total_mins >= 1440) total_mins -= 1440;
    uint8_t loc_hours = (uint8_t)(total_mins / 60);
    uint8_t loc_minutes = (uint8_t)(total_mins % 60);
    snprintf(buf, sizeof(buf), "%02d:%02d", loc_hours, loc_minutes);
    gfx_draw_string(gfx, val_x, y_pos, buf);
    y_pos += 12;

    // FIX & SAT
    snprintf(buf, sizeof(buf), "%-3s", gnss->valid ? "3D" : "NO");
    gfx_draw_string(gfx, val_x, y_pos, buf);

    snprintf(buf, sizeof(buf), "%-2d", gnss->satellites_tracked);
    gfx_draw_string(gfx, 80, y_pos, buf); // "SAT: " is at 50, len 4 is 24px -> 74 + 6px space = 80
    y_pos += 12;

    // LAT
    int lat_deg = gnss->lat_1e7 / 10000000;
    int lat_frac = (gnss->lat_1e7 > 0 ? gnss->lat_1e7 : -gnss->lat_1e7) % 10000000;
    snprintf(buf, sizeof(buf), "%d.%07d   ", lat_deg, lat_frac);
    gfx_draw_string(gfx, lat_lon_x, y_pos, buf);
    y_pos += 12;

    // LON
    int lon_deg = gnss->lon_1e7 / 10000000;
    int lon_frac = (gnss->lon_1e7 > 0 ? gnss->lon_1e7 : -gnss->lon_1e7) % 10000000;
    snprintf(buf, sizeof(buf), "%d.%07d   ", lon_deg, lon_frac);
    gfx_draw_string(gfx, lat_lon_x, y_pos, buf);
    y_pos += 12;

    // ALT
    snprintf(buf, sizeof(buf), "%d m        ", gnss->alt_m);
    gfx_draw_string(gfx, val_x, y_pos, buf);
    y_pos += 12;

    // SPD
    int speed_kmh = (gnss->speed_knots * 1852) / 100000;
    snprintf(buf, sizeof(buf), "%d km/h     ", speed_kmh);
    gfx_draw_string(gfx, val_x, y_pos, buf);
    y_pos += 12;

    // CRS
    if (gnss->course_valid) {
        int course_deg = gnss->course_deg_100 / 100;
        int course_frac = (gnss->course_deg_100 > 0 ? gnss->course_deg_100 : -gnss->course_deg_100) % 100;
        snprintf(buf, sizeof(buf), "%d.%02d     ", course_deg, course_frac);
    } else {
        snprintf(buf, sizeof(buf), "N/A         ");
    }
    gfx_draw_string(gfx, val_x, y_pos, buf);
    y_pos += 12;

    // SUN INFO (No grid text split here, as this part varies completely based on state.
    // Usually, we would split it, but it's easier to just draw it whole here, overwriting
    // the area. To make sure it clears properly, we'll pad with spaces.)
    if (sun != NULL) {
        // Clear remaining lines
        for (int i = 0; i < 4; i++) {
             gfx_draw_string(gfx, 10, y_pos + i*12, "                              ");
        }

        if (sun->status == SUN_STATUS_NORMAL) {
            snprintf(buf, sizeof(buf), "SUN %02d:%02d-%02d:%02d",
                sun->sunrise_hour, sun->sunrise_minute,
                sun->sunset_hour, sun->sunset_minute);
            gfx_draw_string(gfx, 10, y_pos, buf);
            y_pos += 12;

            int remain_h = sun->time_to_event_min / 60;
            int remain_m = sun->time_to_event_min % 60;
            if (sun->is_daytime) {
                snprintf(buf, sizeof(buf), "TO SUNSET: %02dh %02dm", remain_h, remain_m);
            } else {
                snprintf(buf, sizeof(buf), "TO SUNRISE: %02dh %02dm", remain_h, remain_m);
            }
            gfx_draw_string(gfx, 10, y_pos, buf);
            y_pos += 12;

            int start_min = sun->sunrise_hour * 60 + sun->sunrise_minute;
            int end_min = sun->sunset_hour * 60 + sun->sunset_minute;
            int total_day_min = end_min - start_min;
            if (total_day_min < 0) total_day_min += 1440;

            snprintf(buf, sizeof(buf), "DAY: %02dh %02dm", total_day_min / 60, total_day_min % 60);
            gfx_draw_string(gfx, 10, y_pos, buf);
            y_pos += 12;

        } else if (sun->status == SUN_STATUS_POLAR_DAY) {
            gfx_draw_string(gfx, 10, y_pos, "POLAR DAY");
            y_pos += 12;
            gfx_draw_string(gfx, 10, y_pos, "NO SUNSET DAY: 24h 00m");
            y_pos += 12;
        } else if (sun->status == SUN_STATUS_POLAR_NIGHT) {
            gfx_draw_string(gfx, 10, y_pos, "POLAR NIGHT");
            y_pos += 12;
            gfx_draw_string(gfx, 10, y_pos, "NO SUNRISE DAY: 00h 00m");
            y_pos += 12;
        }
    }
}
