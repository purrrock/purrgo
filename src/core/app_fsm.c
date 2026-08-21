// file: src/core/app_fsm.c
#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include "purrgo/purrgo_time.h"
#include <stdio.h>

// Глобальный экземпляр конфигурации устройства
// purrgo_config_t app_config; // declared in config.c

// Текущее состояние конечного автомата
static purrgo_state_t current_state;

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

    // Открываем базовую директорию карт (например, /MAPS)
    // В данном случае читаем из корня, либо относительно текущей,
    // но в задании требуется искать папки для map_dir.
    // Будем читать из родительской директории карт, либо из корня приложения.
    // Пусть базовая папка будет "MAPS" или просто читаем тесты.
    // Если app_config.map_dir == "/MAPS/BY", то ищем в "/MAPS"
    // Но для простоты прочитаем "../../../tests/data/maps"
    // или вообще корневую папку "/". Для эмулятора используем "./"
    // В реальном устройстве это будет корень SD карты "/"
#ifdef _WIN32
    const char* base_path = "./";
#else
    const char* base_path = "../../../tests/data/maps"; // for emulator tests, or just "/"
#endif

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
    purrgo_config_init();
    // Стартовый экран согласно новой концепции UI — Карта
    current_state = APP_STATE_MAP;
    draft_tz_offset_minutes = app_config.tz_offset_minutes;
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
                    // Сохранение часового пояса и возврат на главный экран (Карта)
                    app_config.tz_offset_minutes = draft_tz_offset_minutes;
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
                    // Сохраняем выбранную директорию (с формированием полного пути)
                    // Для простоты подставляем выбранную папку
#ifdef _WIN32
                    snprintf(app_config.map_dir, sizeof(app_config.map_dir), "./%s", dir_list[dir_cursor_idx].name);
#else
                    snprintf(app_config.map_dir, sizeof(app_config.map_dir), "../../../tests/data/maps/%s", dir_list[dir_cursor_idx].name);
#endif
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