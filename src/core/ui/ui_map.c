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
static void ui_render_marker(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, const purrgo_viewport_t* map_vp, const purrgo_bbox_t* dynamic_cam) {
    int32_t lat, lon;
    if (gnss->valid) {
        lat = gnss->lat_1e7;
        lon = gnss->lon_1e7;
    } else {
        lat = app_config.last_lat_1e7;
        lon = app_config.last_lon_1e7;
    }

    int16_t cx, cy;
    project_to_screen(lon, lat, dynamic_cam, map_vp, &cx, &cy);

    int32_t size = PURRGO_MAP_POS_MARK_SIZE_PX;
    int32_t top_y = -(size / 2);
    int32_t bot_y = size + top_y;
    gfx_point_t pts[3];

    if (gnss->valid && gnss->course_valid) {
        // Isosceles triangle, pointing up in local coordinates
        int32_t hw = size / 2;
        int32_t p0x = 0, p0y = top_y;
        int32_t p1x = -hw, p1y = bot_y;
        int32_t p2x = hw, p2y = bot_y;

        int16_t course_deg = (gnss->course_deg_100 / 100) % 360;
        int32_t s = get_sin_10k(course_deg);
        int32_t c = get_cos_10k(course_deg);

        // Rotate by course_deg
        pts[0].x = (int16_t)(cx + (p0x * c - p0y * s) / 10000);
        pts[0].y = (int16_t)(cy + (p0x * s + p0y * c) / 10000);

        pts[1].x = (int16_t)(cx + (p1x * c - p1y * s) / 10000);
        pts[1].y = (int16_t)(cy + (p1x * s + p1y * c) / 10000);

        pts[2].x = (int16_t)(cx + (p2x * c - p2y * s) / 10000);
        pts[2].y = (int16_t)(cy + (p2x * s + p2y * c) / 10000);
    } else {
        // Equilateral triangle, fixed orientation (pointing up)
        int32_t w = (size * 5774 + 5000) / 10000; // tan(30) = 0.57735...

        pts[0].x = (int16_t)cx;
        pts[0].y = (int16_t)(cy + top_y);

        pts[1].x = (int16_t)(cx - w);
        pts[1].y = (int16_t)(cy + bot_y);

        pts[2].x = (int16_t)(cx + w);
        pts[2].y = (int16_t)(cy + bot_y);
    }

    int16_t min_x = pts[0].x, max_x = pts[0].x;
    int16_t min_y = pts[0].y, max_y = pts[0].y;

    for (int i = 1; i < 3; i++) {
        if (pts[i].x < min_x) min_x = pts[i].x;
        if (pts[i].x > max_x) max_x = pts[i].x;
        if (pts[i].y < min_y) min_y = pts[i].y;
        if (pts[i].y > max_y) max_y = pts[i].y;
    }

    // Render ONLY if the COMPLETE triangle bounding box fits inside the map viewport
    if (min_x >= map_vp->offset_x && max_x < map_vp->offset_x + map_vp->width &&
        min_y >= map_vp->offset_y && max_y < map_vp->offset_y + map_vp->height) {

        gfx_color_t old_fg = gfx->color_fg;
        gfx_set_color(gfx, BLACK, gfx->color_bg);

        if (gnss->valid) {
            gfx_fill_polygon(gfx, pts, 3);
        } else {
            gfx_draw_polygon(gfx, pts, 3);
        }

        gfx_set_color(gfx, old_fg, gfx->color_bg);
    }
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
