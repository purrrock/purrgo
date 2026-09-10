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
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 1); // CONFIG_CURSOR_DIR

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 2); // CONFIG_CURSOR_POI

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 3); // CONFIG_CURSOR_POI_LABELS

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 4); // CONFIG_CURSOR_LOG_MODE

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 5); // CONFIG_CURSOR_TRACK_DISPLAY

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 6); // CONFIG_CURSOR_MAP_LAYERS

    // Test bounds (Should not go past 6)
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 6);

    // Test UP
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_UP, &next_state);
    assert(config_app_get_config_cursor() == 5);

    // Now disable POI (it will collapse the menu)
    // First let's go to POI setting
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_UP, &next_state);
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_UP, &next_state);
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_UP, &next_state);
    assert(config_app_get_config_cursor() == 2);

    // Toggle POI mode to PURRGO_POI_MODE_NO using LEFT
    // Icons (2) -> Circles (1) -> No (0)
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_LEFT, &next_state);
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_LEFT, &next_state);
    assert(config_app_get_draft_poi_mode() == PURRGO_POI_MODE_NO);

    // Now test bounds with POI=NO (Last cursor should be 4, as POI_LABELS is skipped)
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 3); // CONFIG_CURSOR_LOG_MODE is now at index 3

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 4); // CONFIG_CURSOR_TRACK_DISPLAY is now at index 4

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 5); // CONFIG_CURSOR_MAP_LAYERS is now at index 5

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_DOWN, &next_state);
    assert(config_app_get_config_cursor() == 5); // Bounds check
}

void test_config_controller_tz_editing(void) {
    app_config.tz_offset_minutes = 0;
    purrgo_config_controller_init();
    purrgo_config_controller_on_enter(APP_STATE_MENU_CONFIG);

    purrgo_state_t next_state = APP_STATE_MENU_CONFIG;

    assert(config_app_get_draft_tz_offset() == 0);
    assert(config_app_get_config_cursor() == 0);

    // Increment timezone (Right)
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_RIGHT, &next_state);
    assert(config_app_get_draft_tz_offset() == 15);

    // Decrement timezone (Left)
    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_LEFT, &next_state);
    assert(config_app_get_draft_tz_offset() == 0);

    purrgo_config_controller_handle_button(APP_STATE_MENU_CONFIG, PURRGO_ACTION_LEFT, &next_state);
    assert(config_app_get_draft_tz_offset() == -15);
}

void test_map_layers_navigation_and_toggles(void) {
    purrgo_config_init(); // Set defaults
    app_config.layer_landuse = true;

    purrgo_config_controller_on_enter(APP_STATE_MENU_MAP_LAYERS);
    purrgo_state_t next_state = APP_STATE_MENU_MAP_LAYERS;

    // Test Initial State
    assert(config_app_get_map_layers_cursor() == 0);
    assert(config_app_get_draft_layer_landuse() == true);

    // Test UP/DOWN and Wrap Around
    purrgo_config_controller_handle_button(APP_STATE_MENU_MAP_LAYERS, PURRGO_ACTION_UP, &next_state);
    assert(config_app_get_map_layers_cursor() == 8); // Should wrap to Track

    purrgo_config_controller_handle_button(APP_STATE_MENU_MAP_LAYERS, PURRGO_ACTION_DOWN, &next_state);
    assert(config_app_get_map_layers_cursor() == 0); // Wrap back to Landuse

    purrgo_config_controller_handle_button(APP_STATE_MENU_MAP_LAYERS, PURRGO_ACTION_DOWN, &next_state);
    assert(config_app_get_map_layers_cursor() == 1); // Water

    // Toggle state
    bool initial_water = config_app_get_draft_layer_water();
    purrgo_config_controller_handle_button(APP_STATE_MENU_MAP_LAYERS, PURRGO_ACTION_LEFT, &next_state);
    assert(config_app_get_draft_layer_water() == !initial_water);
    purrgo_config_controller_handle_button(APP_STATE_MENU_MAP_LAYERS, PURRGO_ACTION_RIGHT, &next_state);
    assert(config_app_get_draft_layer_water() == initial_water);

    // Test Cancel (MENU)
    purrgo_config_controller_handle_button(APP_STATE_MENU_MAP_LAYERS, PURRGO_ACTION_MENU, &next_state);
    assert(next_state == APP_STATE_MENU_CONFIG);

    // Re-enter and Test OK (Save)
    purrgo_config_controller_on_enter(APP_STATE_MENU_MAP_LAYERS);
    next_state = APP_STATE_MENU_MAP_LAYERS;

    // Toggle cursor 0
    assert(config_app_get_map_layers_cursor() == 0);
    bool initial_landuse = config_app_get_draft_layer_landuse();
    purrgo_config_controller_handle_button(APP_STATE_MENU_MAP_LAYERS, PURRGO_ACTION_RIGHT, &next_state);
    assert(config_app_get_draft_layer_landuse() == !initial_landuse);

    purrgo_config_controller_handle_button(APP_STATE_MENU_MAP_LAYERS, PURRGO_ACTION_OK, &next_state);
    assert(next_state == APP_STATE_MENU_CONFIG);
    assert(app_config.layer_landuse == !initial_landuse); // Should be saved
}

int main(void) {
    test_config_controller_init();
    test_config_controller_menu_navigation();
    test_config_controller_tz_editing();
    test_map_layers_navigation_and_toggles();
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
