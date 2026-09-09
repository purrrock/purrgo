#include "purrgo/map_controller.h"
#include "purrgo/config.h"
#include <assert.h>
#include <stdio.h>

// Mock app config as map_controller references it
purrgo_config_t app_config;

void test_initialization() {
    // Setup initial config state
    app_config.last_lat_1e7 = 123456789;
    app_config.last_lon_1e7 = 987654321;

    // Call initialization, which reads from app_config
    purrgo_map_controller_init();

    // Verify it initialized with app_config values
    assert(map_app_get_map_center_lat() == 123456789);
    assert(map_app_get_map_center_lon() == 987654321);
    assert(map_app_get_map_zoom_level() == PURRGO_MAP_SCALE_500M);
    assert(map_app_is_manual_pan_active() == false);
    assert(map_app_map_is_dirty() == true);
    assert(purrgo_map_controller_is_track_dirty() == false);

    // Update config to something else to verify it's not hardcoded
    app_config.last_lat_1e7 = 0;
    app_config.last_lon_1e7 = 0;

    purrgo_map_controller_init();

    assert(map_app_get_map_center_lat() == 0);
    assert(map_app_get_map_center_lon() == 0);
}

void test_map_dirty_state() {
    purrgo_map_controller_init();

    // Test clear
    map_app_map_clear_dirty();
    assert(map_app_map_is_dirty() == false);

    // Test mark
    map_app_map_mark_dirty();
    assert(map_app_map_is_dirty() == true);
}

void test_track_dirty_state() {
    purrgo_map_controller_init();

    // track_dirty is false after init because purrgo_map_controller_init() does not set it, but it should be false by default if it's static
    purrgo_map_controller_clear_track_dirty(); // Make sure it's false
    assert(purrgo_map_controller_is_track_dirty() == false);

    purrgo_map_controller_mark_track_dirty();
    assert(purrgo_map_controller_is_track_dirty() == true);

    purrgo_map_controller_clear_track_dirty();
    assert(purrgo_map_controller_is_track_dirty() == false);
}

void test_getters() {
    purrgo_map_controller_init();

    uint32_t width_m = map_app_get_map_scale_width_m();
    // PURRGO_MAP_SCALE_500M corresponds to 500m (index 5)
    assert(width_m == 500);

    const char* label = map_app_get_map_scale_label();
    // Verify label for 500M
    assert(label != NULL);
    assert(label[0] == '5' && label[1] == '0' && label[2] == '0' && label[3] == 'M' && label[4] == '\0');
}

int main() {
    test_initialization();
    test_map_dirty_state();
    test_track_dirty_state();
    test_getters();

    printf("Map controller tests passed!\n");
    return 0;
}
