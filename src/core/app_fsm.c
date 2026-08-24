// file: src/core/app_fsm.c
#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include "purrgo/purrgo_time.h"
#include "purrgo/geo.h"
#include <stdio.h>
#include "purrgo/hardware_config.h"

// Глобальный экземпляр конфигурации устройства
// purrgo_config_t app_config; // declared in config.c

// Текущее состояние конечного автомата
static purrgo_state_t current_state;

// Состояние окна просмотра карты (Map Viewport)
static int32_t map_center_lat_1e7;
static int32_t map_center_lon_1e7;
static purrgo_map_scale_t map_zoom_level;
static bool manual_pan_active;

// Значения физической ширины BBox в метрах для расчета координат.
static const uint32_t scale_widths_m[PURRGO_MAP_SCALE_COUNT] = {
    10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000,
    20000, 50000, 100000, 200000, 500000, 1000000, 2000000, 5000000, 10000000
};

// Строковые метки для UI.
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
    } else if (config_cursor_idx == 1) {
        current_state = APP_STATE_MENU_DIR_SELECT;
        load_directory_list();
    }
    break;

            case PURRGO_BTN_MENU:
                // Отмена изменений и возврат на главный экран по зацикливанию
                current_state = APP_STATE_MAP;
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
            if (map_zoom_level > 0) map_zoom_level--;
            return;
        }
        if (button == PURRGO_BTN_MINUS) {
            if (map_zoom_level < PURRGO_MAP_SCALE_COUNT - 1) map_zoom_level++;
            return;
        }

// Получаем ширину экрана в метрах для текущего масштаба.
// Шаг панорамирования - 1/4 экрана.
uint32_t width_m = purrgo_app_get_map_scale_width_m();
uint32_t step_m = width_m / 4;
if (step_m == 0) step_m = 1;

// Перевод метров в градусы (10^7).
// 1 градус широты ≈ 111195 метров. Следовательно, 1 метр ≈ 10^7 / 111195 ≈ 90 единиц 1e7.
int64_t step_y_64 = (int64_t)step_m * 90;

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
            return;
        }
        if (button == PURRGO_BTN_DOWN) {
            int64_t next_lat = (int64_t)map_center_lat_1e7 - step_y;
            if (next_lat > INT32_MAX) next_lat = INT32_MAX;
            if (next_lat < INT32_MIN) next_lat = INT32_MIN;
            map_center_lat_1e7 = (int32_t)next_lat;
            manual_pan_active = true;
            return;
        }
        if (button == PURRGO_BTN_RIGHT) {
            int64_t next_lon = (int64_t)map_center_lon_1e7 + step_x;
            if (next_lon > INT32_MAX) next_lon = INT32_MAX;
            if (next_lon < INT32_MIN) next_lon = INT32_MIN;
            map_center_lon_1e7 = (int32_t)next_lon;
            manual_pan_active = true;
            return;
        }
        if (button == PURRGO_BTN_LEFT) {
            int64_t next_lon = (int64_t)map_center_lon_1e7 - step_x;
            if (next_lon > INT32_MAX) next_lon = INT32_MAX;
            if (next_lon < INT32_MIN) next_lon = INT32_MIN;
            map_center_lon_1e7 = (int32_t)next_lon;
            manual_pan_active = true;
            return;
        }

        if (button == PURRGO_BTN_OK) {
            manual_pan_active = false;
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
                break;
        }
    }
}

void purrgo_app_update(const purrgo_gnss_solution_t* current_fix) {
    purrgo_gnss_solution_t display_fix;

    // Пересчет UTC времени в локальное с использованием специализированного модуля purrgo_time
    purrgo_time_apply_timezone(current_fix, &display_fix, app_config.tz_offset_minutes);

    // Диспетчеризация фоновой логики приложения в зависимости от активного экрана
    switch (current_state) {
        case APP_STATE_MAP:
            // Фоновые расчеты для карты (панорамирование, проверке BBox, подгрузка кластеров)
            if (current_fix->valid && !manual_pan_active) {
                map_center_lat_1e7 = current_fix->lat_1e7;
                map_center_lon_1e7 = current_fix->lon_1e7;
            }
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