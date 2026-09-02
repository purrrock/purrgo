#include "purrgo/track_renderer.h"
#include "purrgo/fs_hal.h"
#include "purrgo/track_logger.h"
#include "map_projection.h"
#include "purrgo/gfx_line.h"
#include <string.h>

static purrgo_bbox_t s_prev_camera = {0};
static purrgo_viewport_t s_prev_viewport = {0};
static uint32_t s_file_offset = 0;
static int16_t s_prev_sx = 0;
static int16_t s_prev_sy = 0;
static bool s_has_prev_point = false;

// Парсинг строки с плавающей точкой в формат 1e7 без использования float
static int32_t parse_coord_1e7(const char* str) {
    int32_t result = 0;
    int32_t sign = 1;

    while (*str == ' ') str++; // Пропуск пробелов

    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') { str++; }

    // Парсинг целой части
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }

    result *= 10000000; // Сдвиг порядка

    // Парсинг дробной части (до 7 знаков)
    if (*str == '.') {
        str++;
        int32_t frac = 0;
        int32_t multiplier = 1000000;
        while (*str >= '0' && *str <= '9' && multiplier > 0) {
            frac += (*str - '0') * multiplier;
            multiplier /= 10;
            str++;
        }
        result += frac;
    }
    return result * sign;
}

void purrgo_track_render(
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *vp,
    const char *gpx_filepath)
{
    if (!gfx || !camera || !vp || !gpx_filepath) return;

    const char* active_track = purrgo_logger_get_active_filename();
    if (active_track && strcmp(gpx_filepath, active_track) == 0) {
        static track_point_t track_points[TRACK_RAM_MAX_POINTS];
        size_t num_points = purrgo_logger_get_track_points(track_points, TRACK_RAM_MAX_POINTS);

        if (num_points == 0) {
            return;
        }

        gfx_set_color(gfx, BLACK, gfx->color_bg);

        bool has_prev = false;
        int16_t prev_sx = 0, prev_sy = 0;

        for (size_t i = 0; i < num_points; i++) {
            int16_t sx, sy;
            project_to_screen(track_points[i].lon_1e7, track_points[i].lat_1e7, camera, vp, &sx, &sy);

            if (has_prev) {
                gfx_draw_line(gfx, prev_sx, prev_sy, sx, sy);
            }
            prev_sx = sx;
            prev_sy = sy;
            has_prev = true;
        }

        return;
    }

    // Check if camera or viewport changed
    bool changed = false;
    if (camera->min_x != s_prev_camera.min_x || camera->min_y != s_prev_camera.min_y ||
        camera->max_x != s_prev_camera.max_x || camera->max_y != s_prev_camera.max_y ||
        vp->width != s_prev_viewport.width || vp->height != s_prev_viewport.height ||
        vp->offset_x != s_prev_viewport.offset_x || vp->offset_y != s_prev_viewport.offset_y) {
        changed = true;
    }

    if (changed) {
        s_prev_camera = *camera;
        s_prev_viewport = *vp;
        s_file_offset = 0;
        s_has_prev_point = false;
    }

    purrgo_file_t* file = purrgo_fs_open(gpx_filepath, FS_READ);
    if (!file) return;

    if (s_file_offset > 0) {
        if (!purrgo_fs_seek(file, s_file_offset)) {
            // Seek failed, maybe file was truncated or replaced? Reset to 0.
            s_file_offset = 0;
            s_has_prev_point = false;
            purrgo_fs_seek(file, 0);
        }
    }

    // Oтрисовывать трек нужно тонкой черной линией.
    gfx_set_color(gfx, BLACK, gfx->color_bg);

    char buf[256];
    uint32_t bytes_read;
    uint32_t current_offset = s_file_offset;
    uint32_t safe_offset = s_file_offset;

    bool inside_tag = false;
    char tag_buffer[128];
    uint32_t tag_len = 0;

    while ((bytes_read = purrgo_fs_read(file, (uint8_t*)buf, sizeof(buf))) > 0) {
        for (uint32_t i = 0; i < bytes_read; i++) {
            char c = buf[i];
            current_offset++;

            if (c == '<') {
                inside_tag = true;
                tag_len = 0;
                tag_buffer[0] = '\0';
            } else if (c == '>') {
                inside_tag = false;
                safe_offset = current_offset;

                tag_buffer[tag_len] = '\0';

                // Process the tag
                if (strncmp(tag_buffer, "trkpt ", 6) == 0) {
                    char *lat_ptr = strstr(tag_buffer, "lat=\"");
                    char *lon_ptr = strstr(tag_buffer, "lon=\"");

                    if (lat_ptr && lon_ptr) {
                        int32_t lat = parse_coord_1e7(lat_ptr + 5);
                        int32_t lon = parse_coord_1e7(lon_ptr + 5);

                        int16_t sx, sy;
                        project_to_screen(lon, lat, camera, vp, &sx, &sy);

                        if (s_has_prev_point) {
                            gfx_draw_line(gfx, s_prev_sx, s_prev_sy, sx, sy);
                        }

                        s_prev_sx = sx;
                        s_prev_sy = sy;
                        s_has_prev_point = true;
                    }
                }
            } else if (inside_tag) {
                if (tag_len < sizeof(tag_buffer) - 1) {
                    tag_buffer[tag_len++] = c;
                }
            } else {
                safe_offset = current_offset;
            }
        }
    }

    // Save the offset of the last safely processed byte
    s_file_offset = safe_offset;

    purrgo_fs_close(file);
}
