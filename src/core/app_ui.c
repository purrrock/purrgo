// file: src/core/app_ui.c
#include <purrgo/app_ui.h>
#include <purrgo/app_fsm.h>
#include <purrgo/gfx_text.h>
#include <purrgo/map.h>
#include <purrgo/fs_hal.h>
#include <purrgo/config.h>
extern uint32_t emu_fs_read(void* handle, void* buffer, uint32_t size);
extern bool emu_fs_seek(void* handle, uint32_t offset);
#include <stdio.h>
#include <string.h>

void purrgo_app_ui_render(
    gfx_context_t* gfx,
    const purrgo_gnss_solution_t* gnss,
    const purrgo_sun_info_t* sun
) {
    purrgo_bbox_t fixed_cam = {
        .min_x = 283706420,
        .min_y = 535010200,
        .max_x = 284463180,
        .max_y = 535460080
    };

    purrgo_viewport_t map_vp = {
        .width = 128 - 10,
        .height = 296 - 30,
        .offset_x = 5,
        .offset_y = 15
    };

    char buf[64];
    purrgo_gnss_solution_t gnss_solution = *gnss;
    purrgo_sun_info_t sun_info;
    if (sun != NULL) sun_info = *sun;
    gfx_context_t global_gfx_ctx = *gfx;

    switch (purrgo_app_get_state()) {
        case APP_STATE_MENU_CONFIG: {
            gfx_set_color(
                gfx,
                0,
                3
            );

            int16_t draft_tz =
                purrgo_app_get_draft_tz_offset();

            gfx_draw_string(
                gfx,
                10,
                10,
                "=== CONFIG ==="
            );

            char sign =
                (draft_tz >= 0) ? '+' : '-';

            int16_t abs_tz =
                (draft_tz >= 0)
                    ? draft_tz
                    : -draft_tz;

            int hours = abs_tz / 60;
            int mins = abs_tz % 60;

            snprintf(
                buf,
                sizeof(buf),
                "TZ: UTC%c%02d:%02d",
                sign,
                hours,
                mins
            );

            gfx_set_color(
                gfx,
                3,
                0
            );

            gfx_draw_string(
                gfx,
                10,
                25,
                buf
            );

            gfx_set_color(
                gfx,
                0,
                3
            );

            gfx_draw_string(
                gfx,
                10,
                45,
                "+/- : Change"
            );

            gfx_draw_string(
                gfx,
                10,
                60,
                "OK  : Save"
            );

            gfx_draw_string(
                gfx,
                10,
                75,
                "MENU: Cancel"
            );

            break;
        }

        case APP_STATE_TRIP_COMPUTER: {
            int y_pos = 10;

            snprintf(
                buf,
                sizeof(buf),
                "UTC: %02d:%02d:%02d",
                gnss->hours,
                gnss->minutes,
                gnss->seconds
            );

            gfx_set_color(
                gfx,
                0,
                3
            );

            y_pos += 12;

            // Расчет локального времени на основе UTC и смещения из конфигурации
            int32_t total_mins =
                (int32_t)gnss->hours * 60 +
                (int32_t)gnss->minutes +
                app_config.tz_offset_minutes;

            // Коррекция отрицательного времени при переходе через полночь назад
            while (total_mins < 0)
                total_mins += 1440;

            // Коррекция времени больше 24 часов при переходе через полночь вперед
            while (total_mins >= 1440)
                total_mins -= 1440;

            uint8_t loc_hours =
                (uint8_t)(total_mins / 60);

            uint8_t loc_minutes =
                (uint8_t)(total_mins % 60);

            gfx_draw_string(
                gfx,
                10,
                y_pos,
                buf
            );

            snprintf(
                buf,
                sizeof(buf),
                "LOC: %02d:%02d:%02d",
                loc_hours,
                loc_minutes,
                gnss->seconds
            );

            gfx_set_color(
                gfx,
                0,
                3
            );

            gfx_draw_string(
                gfx,
                10,
                y_pos,
                buf
            );

            y_pos += 12;

            snprintf(
                buf,
                sizeof(buf),
                "FIX: %s   SAT: %d",
                gnss->valid
                    ? "3D"
                    : "NO",
                gnss->satellites_tracked
            );

            gfx_draw_string(
                gfx,
                10,
                y_pos,
                buf
            );

            y_pos += 12;

            // Выделение градусов из формата 1e7 (умноженного на 10^7)
            int lat_deg =
                gnss->lat_1e7 / 10000000;

            // Получение дробной части (без знака)
            int lat_frac =
                (gnss->lat_1e7 > 0
                    ? gnss->lat_1e7
                    : -gnss->lat_1e7)
                % 10000000;

            snprintf(
                buf,
                sizeof(buf),
                "LAT: %d.%07d",
                lat_deg,
                lat_frac
            );

            gfx_draw_string(
                gfx,
                10,
                y_pos,
                buf
            );

            y_pos += 12;

            int lon_deg =
                gnss->lon_1e7 / 10000000;

            int lon_frac =
                (gnss->lon_1e7 > 0
                    ? gnss->lon_1e7
                    : -gnss->lon_1e7)
                % 10000000;

            snprintf(
                buf,
                sizeof(buf),
                "LON: %d.%07d",
                lon_deg,
                lon_frac
            );

            gfx_draw_string(
                gfx,
                10,
                y_pos,
                buf
            );

            y_pos += 12;

            snprintf(
                buf,
                sizeof(buf),
                "ALT: %d m",
                gnss->alt_m
            );

            gfx_draw_string(
                gfx,
                10,
                y_pos,
                buf
            );

            y_pos += 12;

            // Перевод узлов (knots) в км/ч. 1 узел = 1.852 км/ч.
            // Используется целочисленная арифметика с фиксированной точкой (knots * 1.852 * 1000)
            int speed_kmh =
                (gnss->speed_knots * 1852)
                / 100000;

            snprintf(
                buf,
                sizeof(buf),
                "SPD: %d km/h",
                speed_kmh
            );

            gfx_draw_string(
                gfx,
                10,
                y_pos,
                buf
            );

            y_pos += 12;

            if (sun != NULL) {
                if (
                    sun->status ==
                    SUN_STATUS_NORMAL
                ) {
                    snprintf(
                        buf,
                        sizeof(buf),
                        "SUN %02d:%02d-%02d:%02d",
                        sun->sunrise_hour,
                        sun->sunrise_minute,
                        sun->sunset_hour,
                        sun->sunset_minute
                    );

                    gfx_draw_string(
                        gfx,
                        10,
                        y_pos,
                        buf
                    );

                    y_pos += 12;

                    int remain_h =
                        sun->time_to_event_min / 60;

                    int remain_m =
                        sun->time_to_event_min % 60;

                    if (sun->is_daytime) {
                        snprintf(
                            buf,
                            sizeof(buf),
                            "TO SUNSET: %02dh %02dm",
                            remain_h,
                            remain_m
                        );
                    } else {
                        snprintf(
                            buf,
                            sizeof(buf),
                            "TO SUNRISE: %02dh %02dm",
                            remain_h,
                            remain_m
                        );
                    }

                    gfx_draw_string(
                        gfx,
                        10,
                        y_pos,
                        buf
                    );

                    y_pos += 12;

                    int start_min =
                        sun->sunrise_hour * 60 +
                        sun->sunrise_minute;

                    int end_min =
                        sun->sunset_hour * 60 +
                        sun->sunset_minute;

                    int total_day_min =
                        end_min - start_min;

                    // Если закат переходит через полночь
                    if (total_day_min < 0)
                        total_day_min += 1440;

                    snprintf(
                        buf,
                        sizeof(buf),
                        "DAY: %02dh %02dm",
                        total_day_min / 60,
                        total_day_min % 60
                    );

                    gfx_draw_string(
                        gfx,
                        10,
                        y_pos,
                        buf
                    );

                    y_pos += 12;
                } else if (
                    sun->status ==
                    SUN_STATUS_POLAR_DAY
                ) {
                    gfx_draw_string(
                        gfx,
                        10,
                        y_pos,
                        "POLAR DAY"
                    );

                    y_pos += 12;

                    gfx_draw_string(
                        gfx,
                        10,
                        y_pos,
                        "NO SUNSET DAY: 24h 00m"
                    );

                    y_pos += 12;
                } else if (
                    sun->status ==
                    SUN_STATUS_POLAR_NIGHT
                ) {
                    gfx_draw_string(
                        gfx,
                        10,
                        y_pos,
                        "POLAR NIGHT"
                    );

                    y_pos += 12;

                    gfx_draw_string(
                        gfx,
                        10,
                        y_pos,
                        "NO SUNRISE DAY: 00h 00m"
                    );

                    y_pos += 12;
                }
            }

            break;
        }

        case APP_STATE_MAP: {
            static bool map_screen_logged = false;

            if (!map_screen_logged) {
                fprintf(
                    stderr,
                    "EMU: APP_STATE_MAP rendering started\n"
                );
                fflush(stderr);

                map_screen_logged = true;
            }

            /*
             * Верхняя служебная строка.
             */
            gfx_set_color(
                gfx,
                0,
                3
            );

            gfx_draw_string(
                gfx,
                5,
                5,
                "TOP STATUS AREA"
            );

            const char* idx_path =
                "../../../tests/data/maps/roads.idx";

            const char* mlp_path =
                "../../../tests/data/maps/roads.mlp";

            const char* name_path =
                "../../../tests/data/maps/map.name";

            purrgo_file_t* idx_file = purrgo_fs_open(idx_path, FS_READ);
            purrgo_file_t* mlp_file = purrgo_fs_open(mlp_path, FS_READ);

            if (idx_file && mlp_file) {
                purrgo_fs_t idx_fs = {
                    .handle = idx_file,
                    .read = emu_fs_read,
                    .seek = emu_fs_seek
                };

                purrgo_fs_t mlp_fs = {
                    .handle = mlp_file,
                    .read = emu_fs_read,
                    .seek = emu_fs_seek
                };

                gfx_set_color(
                    gfx,
                    0,
                    3
                );

                purrgo_map_render_layer(
                    &idx_fs,
                    &mlp_fs,
                    gfx,
                    &fixed_cam,
                    &map_vp
                );

                purrgo_fs_close(idx_file);
                purrgo_fs_close(mlp_file);
            } else {
                if (idx_file) purrgo_fs_close(idx_file);
                if (mlp_file) purrgo_fs_close(mlp_file);
            }

            purrgo_file_t* name_file = purrgo_fs_open(name_path, FS_READ);

            if (name_file) {
                char name_buf[65];

                uint32_t n =
                    purrgo_fs_read(
                        name_file,
                        (uint8_t*)name_buf,
                        64
                    );

                if (n > 64)
                    n = 64;

                name_buf[n] = '\0';

                purrgo_fs_close(name_file);
            }

            gfx_set_color(
                gfx,
                0,
                3
            );

            break;
        }

        default:
            break;
    }
}