#include "purrgo/map_controller.h"
#include <string.h>
#include "purrgo/config.h"
#include "purrgo/geo.h"
#include "map_projection.h"
#include "purrgo/hardware_config.h"
#include "purrgo/logger.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/ui/ui_dirty.h"

#define AUTO_FOLLOW_EDGE_MARGIN_PX 16
/* Запас от границы зоны срабатывания, на который маркёр ставится после сдвига. */
#define AUTO_FOLLOW_REARM_PX 24

// Состояние окна просмотра карты (Map Viewport)
static int32_t map_center_lat_1e7;
static int32_t map_center_lon_1e7;
static purrgo_map_scale_t map_zoom_level;
static bool manual_pan_active;
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
    purrgo_ui_dirty_clear(UI_DIRTY_TRACK);
    memset(&prev_fix_for_map, 0, sizeof prev_fix_for_map);
    purrgo_ui_dirty_mark(UI_DIRTY_MAP);
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

/*
 * Автоследование: камера сдвигается, когда маркёр подошёл к краю окна карты
 * ближе AUTO_FOLLOW_EDGE_MARGIN_PX. Каждая ось обрабатывается независимо, и
 * сдвигается только та ось, по которой маркёр дошёл до края: раньше при выходе
 * к краю по одной оси вторая тоже отражалась относительно центра.
 *
 * После сдвига маркёр ставится на противоположную сторону, но не ближе
 * AUTO_FOLLOW_REARM_PX к границе зоны срабатывания. Этот гистерезис нужен,
 * чтобы маркёр, оказавшийся в углу, не попадал сразу после сдвига на ту же
 * границу с другой стороны: иначе шум GNSS в 1 px запускал обратный сдвиг, и
 * карта дёргалась туда-сюда.
 */
static int32_t follow_axis_target(int32_t d, int32_t start) {
    int32_t abs_d = d < 0 ? -d : d;
    if (abs_d < start) {
        return d;   /* ось не сработала: маркёр остаётся где был */
    }
    int32_t limit = start - AUTO_FOLLOW_REARM_PX;
    if (limit < 0) limit = 0;
    int32_t target = -d;
    if (target > limit) target = limit;
    if (target < -limit) target = -limit;
    return target;
}

static void apply_auto_follow(const purrgo_gnss_solution_t* fix) {
    if (!fix->valid || manual_pan_active) {
        return;
    }

    purrgo_viewport_t map_vp = {
        .width = PURRGO_HW_DISPLAY_WIDTH_PX,
        .height = PURRGO_MAP_VIEWPORT_HEIGHT_PX,
        .offset_x = 0,
        .offset_y = PURRGO_MAP_STATUS_BAR_H_PX
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
    project_to_screen(fix->lon_1e7, fix->lat_1e7, &dynamic_cam, &map_vp, &sx, &sy);

    const int32_t center_x = map_vp.offset_x + map_vp.width / 2;
    const int32_t center_y = map_vp.offset_y + map_vp.height / 2;
    const int32_t dx = (int32_t)sx - center_x;
    const int32_t dy = (int32_t)sy - center_y;

    const int32_t follow_start_x = (int32_t)(map_vp.width / 2) - AUTO_FOLLOW_EDGE_MARGIN_PX;
    const int32_t follow_start_y = (int32_t)(map_vp.height / 2) - AUTO_FOLLOW_EDGE_MARGIN_PX;

    const int32_t target_dx = follow_axis_target(dx, follow_start_x);
    const int32_t target_dy = follow_axis_target(dy, follow_start_y);
    const bool move_x = (target_dx != dx);
    const bool move_y = (target_dy != dy);
    if (!move_x && !move_y) {
        return;
    }

    int32_t new_lat = map_center_lat_1e7;
    int32_t new_lon = map_center_lon_1e7;

    /* Точная обратная проекция: центр камеры, при котором маркёр попадёт в target. */
    if (move_y) {
        int64_t geo_height = (int64_t)dynamic_cam.max_y - (int64_t)dynamic_cam.min_y;
        int64_t rad_y = geo_height / 2;
        int64_t y_term = (int64_t)map_vp.height - (center_y + target_dy) + map_vp.offset_y;
        int64_t candidate_lat = (int64_t)fix->lat_1e7 -
                                (y_term * geo_height + map_vp.height / 2) / map_vp.height + rad_y;
        if (candidate_lat > 900000000LL) candidate_lat = 900000000LL;
        if (candidate_lat < -900000000LL) candidate_lat = -900000000LL;
        new_lat = (int32_t)candidate_lat;
    }

    if (move_x) {
        /* Ширина охвата в градусах зависит от широты нового центра. */
        purrgo_bbox_t temp_cam;
        purrgo_geo_bbox_from_center(new_lat, map_center_lon_1e7,
                                    map_app_get_map_scale_width_m(), &map_vp, &temp_cam);
        int64_t geo_width = camera_span_x(&temp_cam);
        int64_t rad_x = geo_width / 2;
        int64_t x_term = (int64_t)(center_x + target_dx) - map_vp.offset_x;
        int64_t candidate_lon = (int64_t)fix->lon_1e7 -
                                (x_term * geo_width + map_vp.width / 2) / map_vp.width + rad_x;
        new_lon = purrgo_geo_wrap_lon(candidate_lon);
    }

    if (new_lat != map_center_lat_1e7 || new_lon != map_center_lon_1e7) {
        map_center_lat_1e7 = new_lat;
        map_center_lon_1e7 = new_lon;
        purrgo_ui_dirty_mark(UI_DIRTY_MAP);
    }
}

/* Центрирование карты на маркёре GNSS: маркёр в центре окна карты. */
static void center_map_on_fix(const purrgo_gnss_solution_t* fix) {
    if (!fix->valid) {
        return;
    }
    if (map_center_lat_1e7 != fix->lat_1e7 || map_center_lon_1e7 != fix->lon_1e7) {
        map_center_lat_1e7 = fix->lat_1e7;
        map_center_lon_1e7 = fix->lon_1e7;
        purrgo_ui_dirty_mark(UI_DIRTY_MAP);
    }
}

void purrgo_map_controller_update(const purrgo_gnss_solution_t* current_fix) {
    apply_auto_follow(current_fix);
    prev_fix_for_map = *current_fix;
}

bool purrgo_map_controller_handle_button(purrgo_action_t button) {
    if (button == PURRGO_ACTION_PLUS) {
        if (map_zoom_level > 0) {
            map_zoom_level--;
            purrgo_ui_dirty_mark(UI_DIRTY_MAP);
        }
        return true;
    }
    if (button == PURRGO_ACTION_MINUS) {
        if (map_zoom_level < PURRGO_MAP_SCALE_COUNT - 1) {
            map_zoom_level++;
            purrgo_ui_dirty_mark(UI_DIRTY_MAP);
        }
        return true;
    }

    const uint32_t width_m = map_app_get_map_scale_width_m();

    /* Высота окна карты в метрах: окно имеет те же метры на пиксель по обеим осям. */
    const uint64_t height_m = ((uint64_t)width_m * PURRGO_MAP_VIEWPORT_HEIGHT_PX) /
                              PURRGO_HW_DISPLAY_WIDTH_PX;

    uint64_t step_x_m = width_m / PURRGO_MAP_PAN_STEP_X_DIV;
    uint64_t step_y_m = height_m / PURRGO_MAP_PAN_STEP_Y_DIV;
    if (step_x_m == 0) step_x_m = 1;
    if (step_y_m == 0) step_y_m = 1;

    int64_t step_y_64 = (int64_t)step_y_m * PURRGO_1E7_PER_METER;

    int32_t cos_val = purrgo_geo_cos_10k(map_center_lat_1e7);
    if (cos_val < 100) cos_val = 100; // prevent division by zero near poles
    int64_t step_x_64 = ((int64_t)step_x_m * PURRGO_1E7_PER_METER * 10000) / cos_val;

    // Safety clamping before casting to int32_t
    if (step_y_64 > INT32_MAX) step_y_64 = INT32_MAX;
    if (step_y_64 < INT32_MIN) step_y_64 = INT32_MIN;
    if (step_x_64 > INT32_MAX) step_x_64 = INT32_MAX;
    if (step_x_64 < INT32_MIN) step_x_64 = INT32_MIN;

    int32_t step_y = (int32_t)step_y_64;
    int32_t step_x = (int32_t)step_x_64;

    if (button == PURRGO_ACTION_UP) {
        int64_t next_lat = (int64_t)map_center_lat_1e7 + step_y;
        if (next_lat > INT32_MAX) next_lat = INT32_MAX;
        if (next_lat < INT32_MIN) next_lat = INT32_MIN;
        map_center_lat_1e7 = (int32_t)next_lat;
        manual_pan_active = true;
        purrgo_ui_dirty_mark(UI_DIRTY_MAP);
        return true;
    }
    if (button == PURRGO_ACTION_DOWN) {
        int64_t next_lat = (int64_t)map_center_lat_1e7 - step_y;
        if (next_lat > INT32_MAX) next_lat = INT32_MAX;
        if (next_lat < INT32_MIN) next_lat = INT32_MIN;
        map_center_lat_1e7 = (int32_t)next_lat;
        manual_pan_active = true;
        purrgo_ui_dirty_mark(UI_DIRTY_MAP);
        return true;
    }
    if (button == PURRGO_ACTION_RIGHT) {
        int64_t next_lon = (int64_t)map_center_lon_1e7 + step_x;
        if (next_lon > INT32_MAX) next_lon = INT32_MAX;
        if (next_lon < INT32_MIN) next_lon = INT32_MIN;
        map_center_lon_1e7 = (int32_t)next_lon;
        manual_pan_active = true;
        purrgo_ui_dirty_mark(UI_DIRTY_MAP);
        return true;
    }
    if (button == PURRGO_ACTION_LEFT) {
        int64_t next_lon = (int64_t)map_center_lon_1e7 - step_x;
        if (next_lon > INT32_MAX) next_lon = INT32_MAX;
        if (next_lon < INT32_MIN) next_lon = INT32_MIN;
        map_center_lon_1e7 = (int32_t)next_lon;
        manual_pan_active = true;
        purrgo_ui_dirty_mark(UI_DIRTY_MAP);
        return true;
    }

    if (button == PURRGO_ACTION_OK) {
        /* Долгое K3: вернуть автоследование и поставить маркёр GNSS в центр карты. */
        const bool was_manual = manual_pan_active;
        manual_pan_active = false;
        center_map_on_fix(&prev_fix_for_map);
        if (was_manual) {
            /* Перекрестие центра карты рисуется только в ручном режиме: убрать его. */
            purrgo_ui_dirty_mark(UI_DIRTY_MAP);
        }
        return true;
    }

    return false;
}
