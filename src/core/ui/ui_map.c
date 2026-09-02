#include "ui_map.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_text.h"
#include "purrgo/geo.h"
#include "purrgo/map.h"
#include "purrgo/track_renderer.h"
#include "purrgo/track_logger.h"
#include "purrgo/config.h"
#include "purrgo/logger.h"
#include "purrgo/hardware_config.h"
#include "purrgo/display_hal.h"

typedef struct {
    bool rendered;
    int16_t min_x;
    int16_t max_x;
    int16_t min_y;
    int16_t max_y;
    bool gnss_valid;
    bool course_valid;
    int16_t course_deg;
    int32_t lat_1e7;
    int32_t lon_1e7;

    gfx_point_t pts[8];
    uint8_t point_count;
    bool filled;
} marker_state_t;

static marker_state_t prev_marker_state;
static bool prev_marker_state_valid = false;

/*
 * Background cache to avoid re-rendering the whole map when marker changes.
 * Size 24x24 is enough for the marker size + safety margin.
 */
#define MARKER_BG_CACHE_W 16
#define MARKER_BG_CACHE_H 16

static struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    bool valid;
    gfx_color_t pixels[MARKER_BG_CACHE_W * MARKER_BG_CACHE_H];
} marker_bg_cache = {0};

static void log_marker_diagnostic(const char* reason, const marker_state_t* m) {
    if (m->rendered) {
        PURRGO_LOG(
            "%s | valid: %d | pos: %ld,%ld | course_valid: %d | course: %d | bbox: %d,%d -> %d,%d\n",
            reason, m->gnss_valid, (long)m->lat_1e7, (long)m->lon_1e7,
            m->course_valid, m->course_valid ? m->course_deg : 0,
            m->min_x, m->min_y, m->max_x, m->max_y
        );
    } else {
        PURRGO_LOG(
            "%s | valid: %d | pos: %ld,%ld | course_valid: %d | course: %d | NOT RENDERED\n",
            reason, m->gnss_valid, (long)m->lat_1e7, (long)m->lon_1e7,
            m->course_valid, m->course_valid ? m->course_deg : 0
        );
    }
}

#include "purrgo/gfx_polygon.h"
#include "purrgo/sun_tables.h"
#include "../map_projection.h"
#include <stdio.h>
#include <string.h>

#define PURRGO_MAP_POS_MARK_SIZE_PX 10
#define PURRGO_MAP_POS_MARK_DIRECTION_HALF_WIDTH_PX 4
#define PURRGO_MAP_POS_MARK_OCTAGON_CUT_PX           2

extern int dbg_map_render_calls;

// Helper: Get sine * 10000 for angle in degrees
static int32_t get_sin_10k(int16_t deg) {
    deg = deg % 360;
    if (deg < 0) deg += 360;
    if (deg <= 90) return sin_lut[deg];
    if (deg <= 180) return sin_lut[180 - deg];
    if (deg <= 270) return -sin_lut[deg - 180];
    return -sin_lut[360 - deg];
}

// Helper: Get cosine * 10000 for angle in degrees
static int32_t get_cos_10k(int16_t deg) {
    deg = deg % 360;
    if (deg < 0) deg += 360;
    if (deg <= 90) return cos_lut[deg];
    if (deg <= 180) return -cos_lut[180 - deg];
    if (deg <= 270) return -cos_lut[deg - 180];
    return cos_lut[360 - deg];
}

/*
 * Calculates the marker geometry using integer math based on GNSS state,
 * projects the geographical position to screen, checks if the entire shape's
 * bounding box falls inside the viewport, and renders it in black.
 */
/*
 * Render the GNSS position marker.
 *
 * Marker states:
 *   1. Valid position + valid course:
 *      Filled narrow isosceles triangle pointing in the current course.
 *
 *   2. Valid position + invalid course:
 *      Filled black octagon. Its fixed shape indicates that the position
 *      is valid, but the direction of movement is unknown.
 *
 *   3. Invalid position:
 *      Outline equilateral triangle at the configured last position.
 *
 * All geometry is calculated using integer arithmetic.
 *
 * The marker is rendered only when its complete bounding box fits inside
 * the map viewport. gfx clipping is intentionally not used as a substitute
 * for this visibility test.
 */
static void ui_calc_marker_state(
    const purrgo_gnss_solution_t* gnss,
    const purrgo_viewport_t* map_vp,
    const purrgo_bbox_t* dynamic_cam,
    marker_state_t* out_state)
{
    int32_t lat;
    int32_t lon;

    out_state->gnss_valid = gnss->valid;
    out_state->course_valid = gnss->course_valid;
    out_state->course_deg = (int16_t)((gnss->course_deg_100 / 100) % 360);
    out_state->rendered = false;

    /*
     * Use the current GNSS position when a valid fix exists.
     * Otherwise use the persistent configured fallback position.
     */
    if (gnss->valid) {
        lat = gnss->lat_1e7;
        lon = gnss->lon_1e7;
    } else {
        lat = app_config.last_lat_1e7;
        lon = app_config.last_lon_1e7;
    }

    out_state->lat_1e7 = lat;
    out_state->lon_1e7 = lon;

    /*
     * Project the geographic position using the existing integer projection.
     */
    int16_t cx;
    int16_t cy;

    project_to_screen(
        lon,
        lat,
        dynamic_cam,
        map_vp,
        &cx,
        &cy
    );

    const int32_t size = PURRGO_MAP_POS_MARK_SIZE_PX;
    const int32_t half_size = size / 2;

    /*
     * Maximum number of vertices required by any marker shape:
     *   - triangle: 3
     *   - octagon:  8
     */
    if (gnss->valid && gnss->course_valid) {
        /*
         * Valid position and valid course:
         *
         * Local triangle:
         *       (0, -half_size)
         *          /\
         *         /  \
         *        /    \
         *   (-w, +half_size) (+w, +half_size)
         *
         * The marker is intentionally narrower than before:
         * half width = 4 px instead of 6 px.
         */
        const int32_t half_width =
            PURRGO_MAP_POS_MARK_DIRECTION_HALF_WIDTH_PX;

        const int32_t top_y = -half_size;
        const int32_t bottom_y = half_size;

        const int32_t local_x[3] = {
            0,
            -half_width,
            half_width
        };

        const int32_t local_y[3] = {
            top_y,
            bottom_y,
            bottom_y
        };

        /*
         * course_deg_100 is degrees * 100.
         *
         * The marker is intentionally rotated using whole degrees.
         * At a 12 px marker size, the additional 1/100 degree precision
         * has no meaningful effect on the rasterized result.
         */
        int32_t s = get_sin_10k(out_state->course_deg);
        int32_t c = get_cos_10k(out_state->course_deg);

        for (uint8_t i = 0; i < 3; i++) {
            /*
             * Rotate local coordinates around the marker center.
             *
             * sin/cos are represented as integer values scaled by 10000.
             */
            out_state->pts[i].x = (int16_t)(
                cx +
                (local_x[i] * c - local_y[i] * s) / 10000
            );

            out_state->pts[i].y = (int16_t)(
                cy +
                (local_x[i] * s + local_y[i] * c) / 10000
            );
        }

        out_state->point_count = 3;
        out_state->filled = true;

    } else if (gnss->valid) {
        /*
         * Valid position but invalid course:
         *
         * Use a fixed filled octagon. This is intentionally visually
         * distinct from the directional triangle.
         *
         * The octagon occupies approximately the same 12 px envelope
         * as the other marker shapes.
         */
        const int32_t cut =
            PURRGO_MAP_POS_MARK_OCTAGON_CUT_PX;

        const int32_t left = -half_size;
        const int32_t right = half_size;
        const int32_t top = -half_size;
        const int32_t bottom = half_size;

        out_state->pts[0].x = (int16_t)(cx - cut);
        out_state->pts[0].y = (int16_t)(cy + top);

        out_state->pts[1].x = (int16_t)(cx + cut);
        out_state->pts[1].y = (int16_t)(cy + top);

        out_state->pts[2].x = (int16_t)(cx + right);
        out_state->pts[2].y = (int16_t)(cy - cut);

        out_state->pts[3].x = (int16_t)(cx + right);
        out_state->pts[3].y = (int16_t)(cy + cut);

        out_state->pts[4].x = (int16_t)(cx + cut);
        out_state->pts[4].y = (int16_t)(cy + bottom);

        out_state->pts[5].x = (int16_t)(cx - cut);
        out_state->pts[5].y = (int16_t)(cy + bottom);

        out_state->pts[6].x = (int16_t)(cx + left);
        out_state->pts[6].y = (int16_t)(cy + cut);

        out_state->pts[7].x = (int16_t)(cx + left);
        out_state->pts[7].y = (int16_t)(cy - cut);

        out_state->point_count = 8;
        out_state->filled = true;

    } else {
        /*
         * No valid GNSS fix:
         *
         * Show the configured last position using an outline
         * equilateral triangle. No course information is used.
         */
        const int32_t w =
            (size * 5774 + 5000) / 10000;

        const int32_t top_y = -half_size;
        const int32_t bottom_y = half_size;

        out_state->pts[0].x = cx;
        out_state->pts[0].y = (int16_t)(cy + top_y);

        out_state->pts[1].x = (int16_t)(cx - w);
        out_state->pts[1].y = (int16_t)(cy + bottom_y);

        out_state->pts[2].x = (int16_t)(cx + w);
        out_state->pts[2].y = (int16_t)(cy + bottom_y);

        out_state->point_count = 3;
        out_state->filled = false;
    }

    /*
     * Determine the complete marker bounding box.
     */
    int16_t min_x = out_state->pts[0].x;
    int16_t max_x = out_state->pts[0].x;
    int16_t min_y = out_state->pts[0].y;
    int16_t max_y = out_state->pts[0].y;

    for (uint8_t i = 1; i < out_state->point_count; i++) {
        if (out_state->pts[i].x < min_x) {
            min_x = out_state->pts[i].x;
        }

        if (out_state->pts[i].x > max_x) {
            max_x = out_state->pts[i].x;
        }

        if (out_state->pts[i].y < min_y) {
            min_y = out_state->pts[i].y;
        }

        if (out_state->pts[i].y > max_y) {
            max_y = out_state->pts[i].y;
        }
    }

    out_state->min_x = min_x;
    out_state->max_x = max_x;
    out_state->min_y = min_y;
    out_state->max_y = max_y;

    /*
     * Do not render a partially visible marker.
     *
     * Every vertex must fit completely inside the map viewport.
     */
    if (min_x < map_vp->offset_x ||
        max_x >= map_vp->offset_x + map_vp->width ||
        min_y < map_vp->offset_y ||
        max_y >= map_vp->offset_y + map_vp->height) {
        return;
    }

    out_state->rendered = true;
}

static void ui_save_marker_bg(gfx_context_t* gfx, const marker_state_t* state) {
    if (!state->rendered) {
        marker_bg_cache.valid = false;
        return;
    }

    // Safety margin is important to cover anti-aliasing or slight rounding errors
    int margin = 2;
    int16_t x = state->min_x - margin;
    int16_t y = state->min_y - margin;
    int16_t w = (state->max_x - state->min_x + 1) + 2 * margin;
    int16_t h = (state->max_y - state->min_y + 1) + 2 * margin;

    // Clip to screen to prevent buffer overflow
    // Map viewport offset_y is 9 and height is HEIGHT - 18
    // So viewport bounds are y: 9 to HEIGHT - 9.
    int16_t map_vp_offset_y = 9;
    int16_t map_vp_height = PURRGO_HW_DISPLAY_HEIGHT_PX - 18;

    if (x < 0) { w += x; x = 0; }
    if (y < map_vp_offset_y) { h -= (map_vp_offset_y - y); y = map_vp_offset_y; }
    if (x + w > PURRGO_HW_DISPLAY_WIDTH_PX) { w = PURRGO_HW_DISPLAY_WIDTH_PX - x; }
    if (y + h > map_vp_offset_y + map_vp_height) { h = map_vp_offset_y + map_vp_height - y; }

    if (w <= 0 || h <= 0 || w > MARKER_BG_CACHE_W || h > MARKER_BG_CACHE_H) {
        // Can't fit in cache or invalid size, invalidate cache
        marker_bg_cache.valid = false;
        return;
    }

    marker_bg_cache.x = x;
    marker_bg_cache.y = y;
    marker_bg_cache.w = w;
    marker_bg_cache.h = h;
    marker_bg_cache.valid = true;

    int idx = 0;

    // We need to bypass the current clipping area of gfx_context to read background
    // because it might be clipped to the union region during a partial refresh.
    // However, we just clamped the read rectangle to the map viewport, so we are safe
    // to read directly.
    int16_t old_clip_x = gfx->clip_x;
    int16_t old_clip_y = gfx->clip_y;
    int16_t old_clip_w = gfx->clip_w;
    int16_t old_clip_h = gfx->clip_h;
    gfx_reset_clip(gfx);

    for (int16_t cy = 0; cy < h; cy++) {
        for (int16_t cx = 0; cx < w; cx++) {
            marker_bg_cache.pixels[idx++] = gfx_read_pixel(gfx, x + cx, y + cy);
        }
    }

    gfx_set_clip(gfx, old_clip_x, old_clip_y, old_clip_w, old_clip_h);

    PURRGO_LOG("MARKER BG SAVE x=%d y=%d w=%d h=%d\n", x, y, w, h);
}

static void ui_restore_marker_bg(gfx_context_t* gfx) {
    if (!marker_bg_cache.valid) return;

    gfx_color_t old_fg = gfx->color_fg;

    int idx = 0;
    int16_t old_clip_x = gfx->clip_x;
    int16_t old_clip_y = gfx->clip_y;
    int16_t old_clip_w = gfx->clip_w;
    int16_t old_clip_h = gfx->clip_h;
    gfx_reset_clip(gfx);

    for (int16_t cy = 0; cy < marker_bg_cache.h; cy++) {
        for (int16_t cx = 0; cx < marker_bg_cache.w; cx++) {
            gfx->color_fg = marker_bg_cache.pixels[idx++];
            gfx_draw_pixel(gfx, marker_bg_cache.x + cx, marker_bg_cache.y + cy);
        }
    }

    gfx_set_clip(gfx, old_clip_x, old_clip_y, old_clip_w, old_clip_h);

    gfx->color_fg = old_fg;
    PURRGO_LOG("MARKER BG RESTORE x=%d y=%d w=%d h=%d\n", marker_bg_cache.x, marker_bg_cache.y, marker_bg_cache.w, marker_bg_cache.h);
}

static void ui_draw_marker(gfx_context_t* gfx, const marker_state_t* state) {
    if (!state->rendered) return;

    /*
     * Preserve the caller's foreground colour.
     */
    gfx_color_t old_fg = gfx->color_fg;

    gfx_set_color(gfx, BLACK, gfx->color_bg);

    if (state->filled) {
        gfx_fill_polygon(gfx, (gfx_point_t*)state->pts, state->point_count);
    } else {
        gfx_draw_polygon(gfx, (gfx_point_t*)state->pts, state->point_count);
    }

    gfx_set_color(gfx, old_fg, gfx->color_bg);
}
void ui_render_map(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, const purrgo_sun_info_t* sun) {
    (void)sun;  // Currently unused in map view

    purrgo_viewport_t map_vp = {
        .width = PURRGO_HW_DISPLAY_WIDTH_PX,
        .height = PURRGO_HW_DISPLAY_HEIGHT_PX - 18,
        .offset_x = 0,
        .offset_y = 9
    };

    static bool map_screen_logged = false;

    // Clear top status area (0 to offset_y)
    gfx_set_color(gfx, 0, 3);
    gfx_fill_rect(gfx, 0, 0, PURRGO_HW_DISPLAY_WIDTH_PX, map_vp.offset_y);

    // Clear bottom status area
    gfx_fill_rect(gfx, 0, map_vp.offset_y + map_vp.height, PURRGO_HW_DISPLAY_WIDTH_PX, PURRGO_HW_DISPLAY_HEIGHT_PX - (map_vp.offset_y + map_vp.height));

    if (!map_screen_logged) {
        PURRGO_LOG("EMU: APP_STATE_MAP rendering started\n");
        map_screen_logged = true;
    }

    int32_t center_lat = purrgo_app_get_map_center_lat();
    int32_t center_lon = purrgo_app_get_map_center_lon();
    uint32_t width_m = purrgo_app_get_map_scale_width_m();

    purrgo_bbox_t dynamic_cam;
    purrgo_geo_bbox_from_center(center_lat, center_lon, width_m, &map_vp, &dynamic_cam);

    /* Верхняя служебная строка. */
    gfx_set_color(gfx, 0, 3);
    gfx_draw_string(gfx, 5, 1, "TOP STATUS AREA");

    if (purrgo_app_map_is_dirty()) {
        // Clear map viewport
        gfx_set_color(gfx, 0, 3);
        gfx_fill_rect(gfx, map_vp.offset_x, map_vp.offset_y, map_vp.width, map_vp.height);

        // Limit rendering strictly to the map viewport to protect the status bars
        gfx_set_clip(gfx, map_vp.offset_x, map_vp.offset_y, map_vp.width, map_vp.height);

        bool map_success = purrgo_map_render_viewport(gfx, &map_vp, &dynamic_cam, app_config.map_dir);

        const char* active_track_filename = purrgo_logger_get_active_filename();
        if (active_track_filename != NULL && app_config.track_display_enabled) {
            purrgo_track_render(gfx, &dynamic_cam, &map_vp, active_track_filename);
        }

        marker_state_t new_marker_state;
        ui_calc_marker_state(gnss, &map_vp, &dynamic_cam, &new_marker_state);

        if (!prev_marker_state_valid) {
            log_marker_diagnostic("MARKER: initial", &new_marker_state);
            prev_marker_state_valid = true;
        } else {
            bool changed = (new_marker_state.rendered != prev_marker_state.rendered) ||
                           (new_marker_state.gnss_valid != prev_marker_state.gnss_valid) ||
                           (new_marker_state.course_valid != prev_marker_state.course_valid) ||
                           (new_marker_state.course_deg != prev_marker_state.course_deg) ||
                           (new_marker_state.lat_1e7 != prev_marker_state.lat_1e7) ||
                           (new_marker_state.lon_1e7 != prev_marker_state.lon_1e7);

            if (changed) {
                const char* reason = "MARKER: unknown";
                if (new_marker_state.gnss_valid != prev_marker_state.gnss_valid) {
                    reason = "MARKER: validity changed";
                } else if (new_marker_state.lat_1e7 != prev_marker_state.lat_1e7 || new_marker_state.lon_1e7 != prev_marker_state.lon_1e7) {
                    reason = "MARKER: position changed";
                } else if (new_marker_state.course_valid != prev_marker_state.course_valid || new_marker_state.course_deg != prev_marker_state.course_deg) {
                    reason = "MARKER: course changed";
                } else if (new_marker_state.rendered != prev_marker_state.rendered) {
                    reason = "MARKER: visibility changed";
                }
                log_marker_diagnostic(reason, &new_marker_state);
            }
        }

        ui_save_marker_bg(gfx, &new_marker_state);
        ui_draw_marker(gfx, &new_marker_state);
        prev_marker_state = new_marker_state;

        // Restore clipping so status UI can be drawn
        gfx_reset_clip(gfx);

        if (map_success) {
            purrgo_app_map_clear_dirty();
        }
        display_refresh();
        dbg_map_render_calls++;
    } else {
        marker_state_t new_marker_state;
        ui_calc_marker_state(gnss, &map_vp, &dynamic_cam, &new_marker_state);

        bool changed = (new_marker_state.rendered != prev_marker_state.rendered) ||
                       (new_marker_state.gnss_valid != prev_marker_state.gnss_valid) ||
                       (new_marker_state.course_valid != prev_marker_state.course_valid) ||
                       (new_marker_state.course_deg != prev_marker_state.course_deg) ||
                       (new_marker_state.lat_1e7 != prev_marker_state.lat_1e7) ||
                       (new_marker_state.lon_1e7 != prev_marker_state.lon_1e7);

        if (changed) {
            const char* reason = "MARKER: unknown";
            if (new_marker_state.gnss_valid != prev_marker_state.gnss_valid) {
                reason = "MARKER: validity changed";
            } else if (new_marker_state.lat_1e7 != prev_marker_state.lat_1e7 || new_marker_state.lon_1e7 != prev_marker_state.lon_1e7) {
                reason = "MARKER: position changed";
            } else if (new_marker_state.course_valid != prev_marker_state.course_valid || new_marker_state.course_deg != prev_marker_state.course_deg) {
                reason = "MARKER: course changed";
            } else if (new_marker_state.rendered != prev_marker_state.rendered) {
                reason = "MARKER: visibility changed";
            }
            log_marker_diagnostic(reason, &new_marker_state);

            // Restore the old background
            ui_restore_marker_bg(gfx);

            int16_t min_x = 32767;
            int16_t max_x = -32768;
            int16_t min_y = 32767;
            int16_t max_y = -32768;

            if (prev_marker_state.rendered) {
                if (prev_marker_state.min_x < min_x) min_x = prev_marker_state.min_x;
                if (prev_marker_state.max_x > max_x) max_x = prev_marker_state.max_x;
                if (prev_marker_state.min_y < min_y) min_y = prev_marker_state.min_y;
                if (prev_marker_state.max_y > max_y) max_y = prev_marker_state.max_y;
            }

            if (new_marker_state.rendered) {
                if (new_marker_state.min_x < min_x) min_x = new_marker_state.min_x;
                if (new_marker_state.max_x > max_x) max_x = new_marker_state.max_x;
                if (new_marker_state.min_y < min_y) min_y = new_marker_state.min_y;
                if (new_marker_state.max_y > max_y) max_y = new_marker_state.max_y;
            }

            ui_save_marker_bg(gfx, &new_marker_state);
            ui_draw_marker(gfx, &new_marker_state);

            if (min_x <= max_x && min_y <= max_y) {
                // Clamp to viewport
                if (min_x < map_vp.offset_x) min_x = map_vp.offset_x;
                if (max_x >= map_vp.offset_x + map_vp.width) max_x = map_vp.offset_x + map_vp.width - 1;
                if (min_y < map_vp.offset_y) min_y = map_vp.offset_y;
                if (max_y >= map_vp.offset_y + map_vp.height) max_y = map_vp.offset_y + map_vp.height - 1;

                int16_t clip_w = max_x - min_x + 1;
                int16_t clip_h = max_y - min_y + 1;

                if (clip_w > 0 && clip_h > 0) {
                    display_refresh_region(min_x, min_y, clip_w, clip_h);
                }
            }
            prev_marker_state = new_marker_state;
        }
    }

    gfx_set_color(gfx, 0, 3);

    // Отрисовка масштаба в правом нижнем углу
    const char* scale_label = purrgo_app_get_map_scale_label();

    int label_len = 0;
    while(scale_label[label_len] != '\0') label_len++;
    int text_width = label_len * 6;

    int16_t scale_x = map_vp.offset_x + map_vp.width - text_width - 5;
    int16_t scale_y = map_vp.offset_y + map_vp.height + 1;

    gfx_draw_string(gfx, scale_x, scale_y, scale_label);
}
