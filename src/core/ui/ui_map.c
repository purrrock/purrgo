#include "ui_map.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_text.h"
#include "purrgo/geo.h"
#include "purrgo/map.h"
#include "purrgo/config.h"
#include "purrgo/logger.h"
#include "purrgo/hardware_config.h"
#include "purrgo/gfx_polygon.h"
#include "purrgo/sun_tables.h"
#include "../map_projection.h"
#include <stdio.h>
#include <string.h>

#define PURRGO_MAP_POS_MARK_SIZE_PX 12
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
static void ui_render_marker(
    gfx_context_t* gfx,
    const purrgo_gnss_solution_t* gnss,
    const purrgo_viewport_t* map_vp,
    const purrgo_bbox_t* dynamic_cam)
{
    int32_t lat;
    int32_t lon;

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
    gfx_point_t pts[8];
    uint8_t point_count = 0;
    bool filled = false;

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
        int16_t course_deg =
            (int16_t)((gnss->course_deg_100 / 100) % 360);

        int32_t s = get_sin_10k(course_deg);
        int32_t c = get_cos_10k(course_deg);

        for (uint8_t i = 0; i < 3; i++) {
            /*
             * Rotate local coordinates around the marker center.
             *
             * sin/cos are represented as integer values scaled by 10000.
             */
            pts[i].x = (int16_t)(
                cx +
                (local_x[i] * c - local_y[i] * s) / 10000
            );

            pts[i].y = (int16_t)(
                cy +
                (local_x[i] * s + local_y[i] * c) / 10000
            );
        }

        point_count = 3;
        filled = true;

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

        pts[0].x = (int16_t)(cx - cut);
        pts[0].y = (int16_t)(cy + top);

        pts[1].x = (int16_t)(cx + cut);
        pts[1].y = (int16_t)(cy + top);

        pts[2].x = (int16_t)(cx + right);
        pts[2].y = (int16_t)(cy - cut);

        pts[3].x = (int16_t)(cx + right);
        pts[3].y = (int16_t)(cy + cut);

        pts[4].x = (int16_t)(cx + cut);
        pts[4].y = (int16_t)(cy + bottom);

        pts[5].x = (int16_t)(cx - cut);
        pts[5].y = (int16_t)(cy + bottom);

        pts[6].x = (int16_t)(cx + left);
        pts[6].y = (int16_t)(cy + cut);

        pts[7].x = (int16_t)(cx + left);
        pts[7].y = (int16_t)(cy - cut);

        point_count = 8;
        filled = true;

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

        pts[0].x = cx;
        pts[0].y = (int16_t)(cy + top_y);

        pts[1].x = (int16_t)(cx - w);
        pts[1].y = (int16_t)(cy + bottom_y);

        pts[2].x = (int16_t)(cx + w);
        pts[2].y = (int16_t)(cy + bottom_y);

        point_count = 3;
        filled = false;
    }

    /*
     * Determine the complete marker bounding box.
     */
    int16_t min_x = pts[0].x;
    int16_t max_x = pts[0].x;
    int16_t min_y = pts[0].y;
    int16_t max_y = pts[0].y;

    for (uint8_t i = 1; i < point_count; i++) {
        if (pts[i].x < min_x) {
            min_x = pts[i].x;
        }

        if (pts[i].x > max_x) {
            max_x = pts[i].x;
        }

        if (pts[i].y < min_y) {
            min_y = pts[i].y;
        }

        if (pts[i].y > max_y) {
            max_y = pts[i].y;
        }
    }

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

    /*
     * Preserve the caller's foreground colour.
     */
    gfx_color_t old_fg = gfx->color_fg;

    gfx_set_color(gfx, BLACK, gfx->color_bg);

    if (filled) {
        gfx_fill_polygon(gfx, pts, point_count);
    } else {
        gfx_draw_polygon(gfx, pts, point_count);
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

        ui_render_marker(gfx, gnss, &map_vp, &dynamic_cam);

        // Restore clipping so status UI can be drawn
        gfx_reset_clip(gfx);

        if (map_success) {
            purrgo_app_map_clear_dirty();
        }
        dbg_map_render_calls++;
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
