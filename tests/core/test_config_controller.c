#include "purrgo/config_controller.h"
#include "purrgo/config.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_config_controller_init(void) {
    app_config.tz_offset_minutes = 120;
    app_config.poi_mode = PURRGO_POI_MODE_ICONS;
    app_config.poi_label_mode = PURRGO_POI_LABELS_IMPORTANT;

    purrgo_config_controller_init();

    assert(config_app_get_draft_tz_offset() == 120);
    assert(config_app_get_draft_poi_mode() == PURRGO_POI_MODE_ICONS);
    assert(config_app_get_draft_poi_label_mode() == PURRGO_POI_LABELS_IMPORTANT);
    assert(config_app_get_config_cursor() == 0); // CONFIG_CURSOR_TZ is 0
}

void test_config_controller_menu_navigation(void) {
    app_config.poi_mode = PURRGO_POI_MODE_ICONS;
    purrgo_config_controller_init();
    purrgo_config_controller_on_enter(APP_STATE_MENU_CONFIG);

    purrgo_state_t next_state = APP_STATE_MENU_CONFIG;

    // Initially at cursor 0 (CONFIG_CURSOR_TZ)
    assert(config_app_get_config_cursor() == 0);

    // Test DOWN
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 1); // CONFIG_CURSOR_DIR

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 2); // CONFIG_CURSOR_POI

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 3); // CONFIG_CURSOR_POI_LABELS

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 4); // CONFIG_CURSOR_LOG_MODE

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 5); // CONFIG_CURSOR_TRACK_DISPLAY

    // Test bounds (Should not go past 5)
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 5);

    // Test UP
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_UP, &next_state);
    assert(config_app_get_config_cursor() == 4);

    // Now disable POI (it will collapse the menu)
    // First let's go to POI setting
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_UP, &next_state);
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_UP, &next_state);
    assert(config_app_get_config_cursor() == 2);

    // Toggle POI mode to PURRGO_POI_MODE_NO using LEFT
    // Icons (2) -> Circles (1) -> No (0)
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_LEFT, &next_state);
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_LEFT, &next_state);
    assert(config_app_get_draft_poi_mode() == PURRGO_POI_MODE_NO);

    // Now test bounds with POI=NO (Last cursor should be 4, as POI_LABELS is skipped)
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 3); // CONFIG_CURSOR_LOG_MODE is now at index 3

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 4); // CONFIG_CURSOR_TRACK_DISPLAY is now at index 4

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 4); // Bounds check
}

void test_config_controller_tz_editing(void) {
    app_config.tz_offset_minutes = 0;
    purrgo_config_controller_init();
    purrgo_config_controller_on_enter(APP_STATE_MENU_CONFIG);

    purrgo_state_t next_state = APP_STATE_MENU_CONFIG;

    assert(config_app_get_draft_tz_offset() == 0);
    assert(config_app_get_config_cursor() == 0);

    // Increment timezone (Right)
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_RIGHT, &next_state);
    assert(config_app_get_draft_tz_offset() == 15);

    // Decrement timezone (Left)
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_LEFT, &next_state);
    assert(config_app_get_draft_tz_offset() == 0);

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_BTN_LEFT, &next_state);
    assert(config_app_get_draft_tz_offset() == -15);
}

int main(void) {
    test_config_controller_init();
    test_config_controller_menu_navigation();
    test_config_controller_tz_editing();
    printf("Config controller tests passed!\n");
    return 0;
}

// Mock dependencies
void purrgo_logger_log(const char* level, const char* file, int line, const char* format, ...) {}

purrgo_dir_t* purrgo_fs_opendir(const char* path) { return NULL; }
bool purrgo_fs_readdir(purrgo_dir_t* dir, purrgo_fs_dirent_t* entry) { return false; }
void purrgo_fs_closedir(purrgo_dir_t* dir) {}
purrgo_file_t* purrgo_fs_open(const char* path, fs_mode_t mode) { return NULL; }
uint32_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, uint32_t size) { return 0; }
uint32_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* buffer, uint32_t size) { return size; }
void purrgo_fs_close(purrgo_file_t* file) {}
void purrgo_fs_sync(purrgo_file_t* file) {}
bool purrgo_fs_seek(purrgo_file_t* file, uint32_t offset) { return false; }
void purrgo_app_map_mark_dirty(void) {}
