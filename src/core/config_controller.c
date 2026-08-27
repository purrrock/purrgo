#include "purrgo/config_controller.h"
#include "purrgo/config.h"
#include <stdio.h>

#define MAX_DIRS_COUNT 16
static purrgo_fs_dirent_t dir_list[MAX_DIRS_COUNT];
static int dir_list_count = 0;
static int dir_cursor_idx = 0;

static int16_t draft_tz_offset_minutes;
static int config_cursor_idx = 0;

static void load_directory_list(void) {
    dir_list_count = 0;
    dir_cursor_idx = 0;

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

void purrgo_config_controller_init(void) {
    draft_tz_offset_minutes = app_config.tz_offset_minutes;
    config_cursor_idx = 0;
}

void purrgo_config_controller_on_enter(purrgo_state_t state) {
    if (state == APP_STATE_MENU_CONFIG) {
        draft_tz_offset_minutes = app_config.tz_offset_minutes;
        config_cursor_idx = 0;
    } else if (state == APP_STATE_MENU_DIR_SELECT) {
        load_directory_list();
    }
}


void purrgo_config_controller_update(const purrgo_gnss_solution_t* current_fix) {
    // No update logic
}

bool purrgo_config_controller_handle_button(purrgo_state_t current_state, purrgo_btn_t button, purrgo_state_t* next_state_out) {
    *next_state_out = current_state;

    if (current_state == APP_STATE_MENU_CONFIG) {
        switch (button) {
            case PURRGO_BTN_UP:
                if (config_cursor_idx > 0) config_cursor_idx--;
                return true;

            case PURRGO_BTN_DOWN:
                if (config_cursor_idx < 1) config_cursor_idx++;
                return true;

            case PURRGO_BTN_PLUS:
            case PURRGO_BTN_RIGHT:
                if (config_cursor_idx == 0) {
                    if (draft_tz_offset_minutes + 15 <= 840) {
                        draft_tz_offset_minutes += 15;
                    }
                }
                return true;

            case PURRGO_BTN_MINUS:
            case PURRGO_BTN_LEFT:
                if (config_cursor_idx == 0) {
                    if (draft_tz_offset_minutes - 15 >= -720) {
                        draft_tz_offset_minutes -= 15;
                    }
                }
                return true;

            case PURRGO_BTN_OK:
                if (config_cursor_idx == 0) {
                    app_config.tz_offset_minutes = draft_tz_offset_minutes;
                    purrgo_config_save();
                    *next_state_out = APP_STATE_MAP;
                    purrgo_app_map_mark_dirty();
                } else if (config_cursor_idx == 1) {
                    *next_state_out = APP_STATE_MENU_DIR_SELECT;
                    purrgo_config_controller_on_enter(APP_STATE_MENU_DIR_SELECT);
                }
                return true;

            case PURRGO_BTN_MENU:
                *next_state_out = APP_STATE_MAP;
                purrgo_app_map_mark_dirty();
                return true;

            default:
                return false;
        }
    }

    if (current_state == APP_STATE_MENU_DIR_SELECT) {
        switch (button) {
            case PURRGO_BTN_UP:
                if (dir_cursor_idx > 0) dir_cursor_idx--;
                return true;
            case PURRGO_BTN_DOWN:
                if (dir_cursor_idx < dir_list_count - 1) dir_cursor_idx++;
                return true;
            case PURRGO_BTN_OK:
                if (dir_list_count > 0) {
                    snprintf(
                        app_config.map_dir,
                        sizeof(app_config.map_dir),
                        "../../../tests/data/maps/%s",
                        dir_list[dir_cursor_idx].name
                    );

                     purrgo_config_save();
                     *next_state_out = APP_STATE_MENU_CONFIG;
                 }
                return true;
            case PURRGO_BTN_MENU:
                *next_state_out = APP_STATE_MENU_CONFIG;
                return true;
            default:
                return false;
        }
    }

    return false;
}

int16_t config_app_get_draft_tz_offset(void) {
    return draft_tz_offset_minutes;
}

int config_app_get_config_cursor(void) {
    return config_cursor_idx;
}

int config_app_get_dir_list(purrgo_fs_dirent_t** list_out) {
    *list_out = dir_list;
    return dir_list_count;
}

int config_app_get_dir_cursor(void) {
    return dir_cursor_idx;
}
