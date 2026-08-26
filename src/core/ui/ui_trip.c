#include "ui_trip.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_text.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/config.h"
#include <stdio.h>

void ui_render_trip_computer(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, const purrgo_sun_info_t* sun) {
    char buf[64];
    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);

    int y_pos = 10;

    snprintf(
        buf,
        sizeof(buf),
        "UTC: %02d:%02d",
        gnss->hours,
        gnss->minutes
    );

    gfx_set_color(gfx, 0, 3);
    y_pos += 12;

    int32_t total_mins =
        (int32_t)gnss->hours * 60 +
        (int32_t)gnss->minutes +
        app_config.tz_offset_minutes;

    while (total_mins < 0) total_mins += 1440;
    while (total_mins >= 1440) total_mins -= 1440;

    uint8_t loc_hours = (uint8_t)(total_mins / 60);
    uint8_t loc_minutes = (uint8_t)(total_mins % 60);

    gfx_draw_string(gfx, 10, y_pos, buf);

    snprintf(
        buf,
        sizeof(buf),
        "LOC: %02d:%02d",
        loc_hours,
        loc_minutes
    );

    gfx_set_color(gfx, 0, 3);
    gfx_draw_string(gfx, 10, y_pos, buf);
    y_pos += 12;

    snprintf(
        buf,
        sizeof(buf),
        "FIX: %s   SAT: %d",
        gnss->valid ? "3D" : "NO",
        gnss->satellites_tracked
    );

    gfx_draw_string(gfx, 10, y_pos, buf);
    y_pos += 12;

    int lat_deg = gnss->lat_1e7 / 10000000;
    int lat_frac = (gnss->lat_1e7 > 0 ? gnss->lat_1e7 : -gnss->lat_1e7) % 10000000;

    snprintf(
        buf,
        sizeof(buf),
        "LAT: %d.%07d",
        lat_deg,
        lat_frac
    );

    gfx_draw_string(gfx, 10, y_pos, buf);
    y_pos += 12;

    int lon_deg = gnss->lon_1e7 / 10000000;
    int lon_frac = (gnss->lon_1e7 > 0 ? gnss->lon_1e7 : -gnss->lon_1e7) % 10000000;

    snprintf(
        buf,
        sizeof(buf),
        "LON: %d.%07d",
        lon_deg,
        lon_frac
    );

    gfx_draw_string(gfx, 10, y_pos, buf);
    y_pos += 12;

    snprintf(
        buf,
        sizeof(buf),
        "ALT: %d m",
        gnss->alt_m
    );

    gfx_draw_string(gfx, 10, y_pos, buf);
    y_pos += 12;

    int speed_kmh = (gnss->speed_knots * 1852) / 100000;

    snprintf(
        buf,
        sizeof(buf),
        "SPD: %d km/h",
        speed_kmh
    );

    gfx_draw_string(gfx, 10, y_pos, buf);
    y_pos += 12;

    if (gnss->course_valid) {
        int course_deg = gnss->course_deg_100 / 100;
        int course_frac = (gnss->course_deg_100 > 0 ? gnss->course_deg_100 : -gnss->course_deg_100) % 100;
        snprintf(
            buf,
            sizeof(buf),
            "CRS: %d.%02d",
            course_deg,
            course_frac
        );
    } else {
        snprintf(
            buf,
            sizeof(buf),
            "CRS: N/A"
        );
    }

    gfx_draw_string(gfx, 10, y_pos, buf);
    y_pos += 12;

    if (sun != NULL) {
        if (sun->status == SUN_STATUS_NORMAL) {
            snprintf(
                buf,
                sizeof(buf),
                "SUN %02d:%02d-%02d:%02d",
                sun->sunrise_hour,
                sun->sunrise_minute,
                sun->sunset_hour,
                sun->sunset_minute
            );

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

            snprintf(
                buf,
                sizeof(buf),
                "DAY: %02dh %02dm",
                total_day_min / 60,
                total_day_min % 60
            );

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
