// file: src/core/app_fsm.c
#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include "purrgo/purrgo_time.h"
#include "purrgo/geo.h"
#include <stdio.h>
#include "purrgo/hardware_config.h"
#include "map_projection.h"

#define AUTO_FOLLOW_EDGE_MARGIN_PX 16
#define AUTO_FOLLOW_STOP_MARGIN_DIV 16

// Глобальный экземпляр конфигурации устройства
// purrgo_config_t app_config; // declared in config.c

// Текущее состояние конечного автомата
static purrgo_state_t current_state;

// Состояние окна просмотра карты (Map Viewport)
static int32_t map_center_lat_1e7;
static int32_t map_center_lon_1e7;
static purrgo_map_scale_t map_zoom_level;
static bool manual_pan_active;
static bool map_dirty = true;
static bool ui_dirty = true;

// Значения физической ширины BBox в метрах для расчета координат.
static const uint32_t scale_widths_m[PURRGO_MAP_SCALE_COUNT] = {
    10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000,
    20000, 50000, 100000, 200000, 500000, 1000000, 2000000, 5000000, 10000000
};

// Строковые метки для UI.
static purrgo_gnss_solution_t prev_fix = {0};
static void apply_auto_follow(const purrgo_gnss_solution_t* fix);

static const char* scale_labels[PURRGO_MAP_SCALE_COUNT] = {
    "10M", "20M", "50M", "100M", "200M", "500M", "1KM", "2KM", "5KM", "10KM",
    "20KM", "50KM", "100KM", "200KM", "500KM", "1000KM", "2000KM", "5000KM", "10000KM"
};

// Черновые (несохраненные) настройки времени для режима редактирования в меню
static int16_t draft_tz_offset_minutes;
// Индекс курсора в меню настроек (0 - Timezone, 1 - Map Dir)
static int config_cursor_idx = 0;

// Состояние файлового браузера
#define MAX_DIRS_COUNT 16
static purrgo_fs_dirent_t dir_list[MAX_DIRS_COUNT];
static int dir_list_count = 0;
static int dir_cursor_idx = 0;

static void load_directory_list(void) {
    dir_list_count = 0;
    dir_cursor_idx = 0;

/*
     * PC emulator map data is stored in:
     *     <repository>/tests/data/maps/
     *
     * The emulator executable is normally run from:
     *     <repository>/build/apps/emulator/
     * Therefore the relative path from the emulator working directory
     * to the map root is:
     *     ../../../tests/data/maps
     * Use the same path on Windows and other PC platforms.
     */
    const char* base_path = "../../../tests/data/maps";

     purrgo_dir_t* dir = purrgo_fs_opendir(base_path);
     if (!dir) return;

    purrgo_fs_dirent_t entry;
    while (purrgo_fs_readdir(dir, &entry) && dir_list_count < MAX_DIRS_COUNT) {
        if (entry.is_directory) {
            // Пропускаем "." и ".."
            if (entry.name[0] == '.' && (entry.name[1] == '\0' || (entry.name[1] == '.' && entry.name[2] == '\0'))) {
                continue;
            }
            dir_list[dir_list_count] = entry;
            dir_list_count++;
        }
    }
    purrgo_fs_closedir(dir);
}

// void purrgo_config_init(void) is in config.c

void purrgo_app_init(void) {
    /*
     * First try to load the persistent configuration.
     *
     * purrgo_config_load() initializes default values and creates
     * PURRGO.CFG when the file does not exist.
     *
     * Therefore the application should NOT call purrgo_config_init()
     * unconditionally here: doing so would overwrite the values
     * loaded from PURRGO.CFG.
     */
    purrgo_config_load();

    current_state = APP_STATE_MAP;
    draft_tz_offset_minutes = app_config.tz_offset_minutes;

    // Инициализация состояния карты
    map_center_lat_1e7 = app_config.last_lat_1e7;
    map_center_lon_1e7 = app_config.last_lon_1e7;
    map_zoom_level = PURRGO_MAP_SCALE_500M; // Начальный зум
    manual_pan_active = false;
    map_dirty = true;
}

void purrgo_app_ui_mark_dirty(void) {
    ui_dirty = true;
}

bool purrgo_app_ui_is_dirty(void) {
    return ui_dirty;
}

void purrgo_app_ui_clear_dirty(void) {
    ui_dirty = false;
}

void purrgo_app_map_mark_dirty(void) {
    map_dirty = true;
}

bool purrgo_app_map_is_dirty(void) {
    return map_dirty;
}

void purrgo_app_map_clear_dirty(void) {
    map_dirty = false;
}

purrgo_state_t purrgo_app_get_state(void) {
    return current_state;
}

int16_t purrgo_app_get_draft_tz_offset(void) {
    return draft_tz_offset_minutes;
}

int purrgo_app_get_config_cursor(void) {
    return config_cursor_idx;
}

int purrgo_app_get_dir_list(purrgo_fs_dirent_t** list_out) {
    *list_out = dir_list;
    return dir_list_count;
}

int purrgo_app_get_dir_cursor(void) {
    return dir_cursor_idx;
}

int32_t purrgo_app_get_map_center_lat(void) {
    return map_center_lat_1e7;
}

int32_t purrgo_app_get_map_center_lon(void) {
    return map_center_lon_1e7;
}

purrgo_map_scale_t purrgo_app_get_map_zoom_level(void) {
    return map_zoom_level;
}

uint32_t purrgo_app_get_map_scale_width_m(void) {
    return scale_widths_m[map_zoom_level];
}

const char* purrgo_app_get_map_scale_label(void) {
    return scale_labels[map_zoom_level];
}

bool purrgo_app_is_manual_pan_active(void) {
    return manual_pan_active;
}

void purrgo_app_handle_button(purrgo_btn_t button) {
    ui_dirty = true; // Любое нажатие кнопки требует перерисовки UI

    // 1. Обработка ввода в режиме редактирования настроек
    if (current_state == APP_STATE_MENU_CONFIG) {
        switch (button) {
            case PURRGO_BTN_UP:
                if (config_cursor_idx > 0) config_cursor_idx--;
                break;

            case PURRGO_BTN_DOWN:
                if (config_cursor_idx < 1) config_cursor_idx++;
                break;

            case PURRGO_BTN_PLUS:
            case PURRGO_BTN_RIGHT:
                if (config_cursor_idx == 0) {
                    // Увеличение часового пояса шагом в 15 минут (максимум UTC+14:00 = 840 минут)
                    if (draft_tz_offset_minutes + 15 <= 840) {
                        draft_tz_offset_minutes += 15;
                    }
                }
                break;

            case PURRGO_BTN_MINUS:
            case PURRGO_BTN_LEFT:
                if (config_cursor_idx == 0) {
                    // Уменьшение часового пояса шагом в 15 минут (минимум UTC-12:00 = -720 минут)
                    if (draft_tz_offset_minutes - 15 >= -720) {
                        draft_tz_offset_minutes -= 15;
                    }
                }
                break;

	case PURRGO_BTN_OK:
    if (config_cursor_idx == 0) {
        /*
         * Commit the edited timezone to the active configuration.
         */
        app_config.tz_offset_minutes = draft_tz_offset_minutes;

        /*
         * Persist the complete configuration to PURRGO.CFG.
         *
         * This is required because changing the timezone must survive
         * the next application restart.
         */
        purrgo_config_save();

        current_state = APP_STATE_MAP;
        map_dirty = true;
    } else if (config_cursor_idx == 1) {
        current_state = APP_STATE_MENU_DIR_SELECT;
        load_directory_list();
    }
    break;

            case PURRGO_BTN_MENU:
                // Отмена изменений и возврат на главный экран по зацикливанию
                current_state = APP_STATE_MAP;
                map_dirty = true;
                break;

            default:
                break;
        }
        return;
    }

    // 2. Обработка ввода в подменю выбора директории карт
    if (current_state == APP_STATE_MENU_DIR_SELECT) {
        switch (button) {
            case PURRGO_BTN_UP:
                if (dir_cursor_idx > 0) dir_cursor_idx--;
                break;
            case PURRGO_BTN_DOWN:
                if (dir_cursor_idx < dir_list_count - 1) dir_cursor_idx++;
                break;
            case PURRGO_BTN_OK:
                if (dir_list_count > 0) {
                    /*
                     * Save the path relative to the emulator working
                     * directory.
                     *
                     * Example:
                     *
                     *     ../../../tests/data/maps/roads
                     *
                     * This is the same path used by load_directory_list()
                     * and therefore points to the actual test map data.
                     */
                    snprintf(
                        app_config.map_dir,
                        sizeof(app_config.map_dir),
                        "../../../tests/data/maps/%s",
                        dir_list[dir_cursor_idx].name
                    );

                     purrgo_config_save();
                     current_state = APP_STATE_MENU_CONFIG;
                 }
                break;
            case PURRGO_BTN_MENU:
                // Кнопка MENU выполняет роль "Назад" для возврата в меню настроек
                current_state = APP_STATE_MENU_CONFIG;
                break;
            default:
                break;
        }
        return;
    }

    // Обработка ввода на карте
    if (current_state == APP_STATE_MAP) {
        if (button == PURRGO_BTN_PLUS) {
            if (map_zoom_level > 0) {
                map_zoom_level--;
                map_dirty = true;
            }
            return;
        }
        if (button == PURRGO_BTN_MINUS) {
            if (map_zoom_level < PURRGO_MAP_SCALE_COUNT - 1) {
                map_zoom_level++;
                map_dirty = true;
            }
            return;
        }

// Получаем ширину экрана в метрах для текущего масштаба.
// Шаг панорамирования - 1/4 экрана.
uint32_t width_m = purrgo_app_get_map_scale_width_m();
uint32_t step_m = width_m / 4;
if (step_m == 0) step_m = 1;

// Перевод метров в градусы (10^7).
// 1 градус широты ≈ 111195 метров. Следовательно, 1 метр ≈ 10^7 / 111195 ≈ 90 единиц 1e7.
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
            return;
        }
        if (button == PURRGO_BTN_DOWN) {
            int64_t next_lat = (int64_t)map_center_lat_1e7 - step_y;
            if (next_lat > INT32_MAX) next_lat = INT32_MAX;
            if (next_lat < INT32_MIN) next_lat = INT32_MIN;
            map_center_lat_1e7 = (int32_t)next_lat;
            manual_pan_active = true;
            map_dirty = true;
            return;
        }
        if (button == PURRGO_BTN_RIGHT) {
            int64_t next_lon = (int64_t)map_center_lon_1e7 + step_x;
            if (next_lon > INT32_MAX) next_lon = INT32_MAX;
            if (next_lon < INT32_MIN) next_lon = INT32_MIN;
            map_center_lon_1e7 = (int32_t)next_lon;
            manual_pan_active = true;
            map_dirty = true;
            return;
        }
        if (button == PURRGO_BTN_LEFT) {
            int64_t next_lon = (int64_t)map_center_lon_1e7 - step_x;
            if (next_lon > INT32_MAX) next_lon = INT32_MAX;
            if (next_lon < INT32_MIN) next_lon = INT32_MIN;
            map_center_lon_1e7 = (int32_t)next_lon;
            manual_pan_active = true;
            map_dirty = true;
            return;
        }

        if (button == PURRGO_BTN_OK) {
            if (manual_pan_active) {
                manual_pan_active = false;
                apply_auto_follow(&prev_fix);
            }
            return;
        }
    }

    // 3. Циклическое переключение основных экранов (Garmin eTrex Page Loop)
    if (button == PURRGO_BTN_MENU) {
        switch (current_state) {
            case APP_STATE_MAP:
                current_state = APP_STATE_TRIP_COMPUTER;
                break;

            case APP_STATE_TRIP_COMPUTER:
                current_state = APP_STATE_MENU_CONFIG;
                // При входе в меню сбрасываем черновик на текущее сохраненное значение и курсор
                draft_tz_offset_minutes = app_config.tz_offset_minutes;
                config_cursor_idx = 0;
                break;

            default:
                current_state = APP_STATE_MAP;
                map_dirty = true;
                break;
        }
    }
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
        purrgo_app_get_map_scale_width_m(),
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
            purrgo_app_get_map_scale_width_m(),
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
            purrgo_app_get_map_scale_width_m(),
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

void purrgo_app_update(const purrgo_gnss_solution_t* current_fix) {
    // Check if relevant navigation data has changed to trigger a UI redraw
    if (current_fix->valid != prev_fix.valid ||
        current_fix->minutes != prev_fix.minutes ||
        current_fix->hours != prev_fix.hours ||
        current_fix->lat_1e7 != prev_fix.lat_1e7 ||
        current_fix->lon_1e7 != prev_fix.lon_1e7 ||
        current_fix->alt_m != prev_fix.alt_m ||
        current_fix->speed_knots != prev_fix.speed_knots ||
        current_fix->satellites_tracked != prev_fix.satellites_tracked) {

        ui_dirty = true;
        prev_fix = *current_fix;
    }

    purrgo_gnss_solution_t display_fix;

    // Пересчет UTC времени в локальное с использованием специализированного модуля purrgo_time
    if (!purrgo_time_apply_timezone(current_fix, &display_fix, app_config.tz_offset_minutes)) {
        display_fix = *current_fix; // fallback to UTC if conversion fails (e.g. out of bounds)
        display_fix.valid = false;
    }

    // Диспетчеризация фоновой логики приложения в зависимости от активного экрана
    switch (current_state) {
        case APP_STATE_MAP:
            // Фоновые расчеты для карты (панорамирование, проверке BBox, подгрузка кластеров)
            apply_auto_follow(current_fix);
            break;

        case APP_STATE_TRIP_COMPUTER:
            // Расчет агрегированных данных одометра и маршрута
            break;

        case APP_STATE_MENU_CONFIG:
            break;

        case APP_STATE_MENU_DIR_SELECT:
            break;
    }
}
