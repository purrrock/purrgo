#include "purrgo/map_controller.h"
#include "purrgo/config.h"
#include "purrgo/geo.h"
#include "map_projection.h"
#include "purrgo/hardware_config.h"
#include <stdio.h>

#define AUTO_FOLLOW_EDGE_MARGIN_PX 16
#define AUTO_FOLLOW_STOP_MARGIN_DIV 16

// Состояние окна просмотра карты (Map Viewport)
static int32_t map_center_lat_1e7;
static int32_t map_center_lon_1e7;
static purrgo_map_scale_t map_zoom_level;
static bool manual_pan_active;
static bool map_dirty = true;
static purrgo_gnss_solution_t prev_fix_for_map = {0};

// Значения физической ширины BBox в метрах для расчета координат.
static const uint32_t scale_widths_m[PURRGO_MAP_SCALE_COUNT] = {
    10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000,
    20000, 50000, 100000, 200000, 500000, 1000000, 2000000, 5000000, 10000000
};

static const char* scale_labels[PURRGO_MAP_SCALE_COUNT] = {
    "10M", "20M", "50M", "100M", "200M", "500M", "1KM", "2KM", "5KM", "10KM",
    "20KM", "50KM", "100KM", "200KM", "500KM", "1000KM", "2000KM", "5000KM", "10000KM"
};

void purrgo_map_controller_init(void) {
    map_center_lat_1e7 = app_config.last_lat_1e7;
    map_center_lon_1e7 = app_config.last_lon_1e7;
    map_zoom_level = PURRGO_MAP_SCALE_500M;
    manual_pan_active = false;
    map_dirty = true;
}

void map_app_map_mark_dirty(void) {
    map_dirty = true;
}

bool map_app_map_is_dirty(void) {
    return map_dirty;
}

void map_app_map_clear_dirty(void) {
    map_dirty = false;
}

int32_t map_app_get_map_center_lat(void) {
    return map_center_lat_1e7;
}

int32_t map_app_get_map_center_lon(void) {
    return map_center_lon_1e7;
}

purrgo_map_scale_t map_app_get_map_zoom_level(void) {
    return map_zoom_level;
}

uint32_t map_app_get_map_scale_width_m(void) {
    return scale_widths_m[map_zoom_level];
}

const char* map_app_get_map_scale_label(void) {
    return scale_labels[map_zoom_level];
}

bool map_app_is_manual_pan_active(void) {
    return manual_pan_active;
}

static void apply_auto_follow(const purrgo_gnss_solution_t* fix) {
    if (!fix->valid || manual_pan_active) {
        return;
    }

    purrgo_viewport_t map_vp = {
        .width = PURRGO_HW_DISPLAY_WIDTH_PX,
        .height = PURRGO_HW_DISPLAY_HEIGHT_PX - 18,
        .offset_x = 0,
        .offset_y = 9
    };

    purrgo_bbox_t dynamic_cam;
    purrgo_geo_bbox_from_center(
        map_center_lat_1e7,
        map_center_lon_1e7,
        map_app_get_map_scale_width_m(),
        &map_vp,
        &dynamic_cam
    );

    int16_t sx, sy;
    project_to_screen(
        fix->lon_1e7,
        fix->lat_1e7,
        &dynamic_cam,
        &map_vp,
        &sx,
        &sy
    );

    int16_t center_x = map_vp.offset_x + map_vp.width / 2;
    int16_t center_y = map_vp.offset_y + map_vp.height / 2;

    int32_t dx = (int32_t)sx - center_x;
    if (dx < 0) dx = -dx;

    int32_t dy = (int32_t)sy - center_y;
    if (dy < 0) dy = -dy;

    int32_t follow_start_x = (map_vp.width / 2) - AUTO_FOLLOW_EDGE_MARGIN_PX;
    int32_t follow_start_y = (map_vp.height / 2) - AUTO_FOLLOW_EDGE_MARGIN_PX;

    int32_t follow_stop_x = map_vp.width / AUTO_FOLLOW_STOP_MARGIN_DIV;
    int32_t follow_stop_y = map_vp.height / AUTO_FOLLOW_STOP_MARGIN_DIV;

    if (dx <= follow_stop_x && dy <= follow_stop_y) {
        // Inside FOLLOW_STOP zone: no camera change
        return;
    } else if (dx < follow_start_x && dy < follow_start_y) {
        // Between FOLLOW_STOP and FOLLOW_START zones: no camera change
        return;
    } else {
        // Reached or exceeded FOLLOW_START zone: calculate target opposite position
        int32_t target_x = 2 * center_x - sx;
        int32_t target_y = 2 * center_y - sy;

        // Clamp target_x to safe area (shifted inward by 1 pixel to guarantee boundary adherence against integer rounding)
        int32_t min_x = center_x - follow_start_x + 1;
        int32_t max_x = center_x + follow_start_x - 1;
        if (target_x < min_x) target_x = min_x;
        if (target_x > max_x) target_x = max_x;

        // Clamp target_y to safe area
        int32_t min_y = center_y - follow_start_y + 1;
        int32_t max_y = center_y + follow_start_y - 1;
        if (target_y < min_y) target_y = min_y;
        if (target_y > max_y) target_y = max_y;

        // Exact inverse projection from GNSS coordinate to find new camera center
        // From project_to_screen:
        // projected_y = vp.height - (lat - min_y) * vp.height / height + vp.offset_y
        // By substituting min_y = candidate_lat - rad_y and solving for candidate_lat:

        int64_t geo_height = (int64_t)dynamic_cam.max_y - (int64_t)dynamic_cam.min_y;
        int64_t rad_y = geo_height / 2;
        int64_t y_term = (int64_t)map_vp.height - target_y + map_vp.offset_y;
        int64_t candidate_lat = (int64_t)fix->lat_1e7 - (y_term * geo_height + map_vp.height / 2) / map_vp.height - rad_y;

        if (candidate_lat > 900000000LL) candidate_lat = 900000000LL;
        if (candidate_lat < -900000000LL) candidate_lat = -900000000LL;

        // Calculate exact width at the new latitude to handle correct longitudinal scaling
        purrgo_bbox_t temp_cam;
        purrgo_geo_bbox_from_center(
            (int32_t)candidate_lat,
            map_center_lon_1e7, // Lon doesn't matter for width calculation
            map_app_get_map_scale_width_m(),
            &map_vp,
            &temp_cam
        );

        int64_t geo_width = camera_span_x(&temp_cam);
        int64_t rad_x = geo_width / 2;
        int64_t x_term = (int64_t)target_x - map_vp.offset_x;
        int64_t candidate_lon = (int64_t)fix->lon_1e7 - (x_term * geo_width + map_vp.width / 2) / map_vp.width + rad_x;

        // Wrap candidate_lon cleanly within WGS84 +/- 180 (1.8e9) if needed, or simply clamp
        // Consistent with manual panning clamping:
        if (candidate_lon > INT32_MAX) candidate_lon = INT32_MAX;
        if (candidate_lon < INT32_MIN) candidate_lon = INT32_MIN;

        purrgo_bbox_t candidate_cam;
        purrgo_geo_bbox_from_center(
            (int32_t)candidate_lat,
            (int32_t)candidate_lon,
            map_app_get_map_scale_width_m(),
            &map_vp,
            &candidate_cam
        );

        int16_t new_sx, new_sy;
        project_to_screen(
            fix->lon_1e7,
            fix->lat_1e7,
            &candidate_cam,
            &map_vp,
            &new_sx,
            &new_sy
        );

        int32_t new_dx = (int32_t)new_sx - center_x;
        if (new_dx < 0) new_dx = -new_dx;

        int32_t new_dy = (int32_t)new_sy - center_y;
        if (new_dy < 0) new_dy = -new_dy;


        if (new_dx <= follow_start_x && new_dy <= follow_start_y) {
            printf("AUTO-FOLLOW: marker=(%d,%d) target=(%d,%d) result=(%d,%d)\n", sx, sy, (int)target_x, (int)target_y, new_sx, new_sy);

            if (map_center_lat_1e7 != (int32_t)candidate_lat || map_center_lon_1e7 != (int32_t)candidate_lon) {
                map_center_lat_1e7 = (int32_t)candidate_lat;
                map_center_lon_1e7 = (int32_t)candidate_lon;
                map_dirty = true;
            }
        }
    }
}

void purrgo_map_controller_update(const purrgo_gnss_solution_t* current_fix) {
    apply_auto_follow(current_fix);
    prev_fix_for_map = *current_fix;
}

bool purrgo_map_controller_handle_button(purrgo_btn_t button) {
    if (button == PURRGO_BTN_PLUS) {
        if (map_zoom_level > 0) {
            map_zoom_level--;
            map_dirty = true;
        }
        return true;
    }
    if (button == PURRGO_BTN_MINUS) {
        if (map_zoom_level < PURRGO_MAP_SCALE_COUNT - 1) {
            map_zoom_level++;
            map_dirty = true;
        }
        return true;
    }

    uint32_t width_m = map_app_get_map_scale_width_m();
    uint32_t step_m = width_m / 4;
    if (step_m == 0) step_m = 1;

    int64_t step_y_64 = (int64_t)step_m * PURRGO_1E7_PER_METER;

    int32_t cos_val = purrgo_geo_cos_10k(map_center_lat_1e7);
    if (cos_val < 100) cos_val = 100; // prevent division by zero near poles
    int64_t step_x_64 = (step_y_64 * 10000) / cos_val;

    // Safety clamping before casting to int32_t
    if (step_y_64 > INT32_MAX) step_y_64 = INT32_MAX;
    if (step_y_64 < INT32_MIN) step_y_64 = INT32_MIN;
    if (step_x_64 > INT32_MAX) step_x_64 = INT32_MAX;
    if (step_x_64 < INT32_MIN) step_x_64 = INT32_MIN;

    int32_t step_y = (int32_t)step_y_64;
    int32_t step_x = (int32_t)step_x_64;

    if (button == PURRGO_BTN_UP) {
        int64_t next_lat = (int64_t)map_center_lat_1e7 + step_y;
        if (next_lat > INT32_MAX) next_lat = INT32_MAX;
        if (next_lat < INT32_MIN) next_lat = INT32_MIN;
        map_center_lat_1e7 = (int32_t)next_lat;
        manual_pan_active = true;
        map_dirty = true;
        return true;
    }
    if (button == PURRGO_BTN_DOWN) {
        int64_t next_lat = (int64_t)map_center_lat_1e7 - step_y;
        if (next_lat > INT32_MAX) next_lat = INT32_MAX;
        if (next_lat < INT32_MIN) next_lat = INT32_MIN;
        map_center_lat_1e7 = (int32_t)next_lat;
        manual_pan_active = true;
        map_dirty = true;
        return true;
    }
    if (button == PURRGO_BTN_RIGHT) {
        int64_t next_lon = (int64_t)map_center_lon_1e7 + step_x;
        if (next_lon > INT32_MAX) next_lon = INT32_MAX;
        if (next_lon < INT32_MIN) next_lon = INT32_MIN;
        map_center_lon_1e7 = (int32_t)next_lon;
        manual_pan_active = true;
        map_dirty = true;
        return true;
    }
    if (button == PURRGO_BTN_LEFT) {
        int64_t next_lon = (int64_t)map_center_lon_1e7 - step_x;
        if (next_lon > INT32_MAX) next_lon = INT32_MAX;
        if (next_lon < INT32_MIN) next_lon = INT32_MIN;
        map_center_lon_1e7 = (int32_t)next_lon;
        manual_pan_active = true;
        map_dirty = true;
        return true;
    }

    if (button == PURRGO_BTN_OK) {
        if (manual_pan_active) {
            manual_pan_active = false;
            apply_auto_follow(&prev_fix_for_map);
        }
        return true;
    }

    return false;
}
