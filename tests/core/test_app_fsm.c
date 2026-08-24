#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include <stdio.h>
#include <assert.h>

void setup_test_state(int32_t lat, int32_t lon, purrgo_map_scale_t scale) {
    purrgo_app_init();

    // Simulate setting internal state (not directly exposed via public API setters for these tests,
    // but we can manipulate config to some extent and rely on initial values if needed,
    // or just let the button handlers move it and verify).
    // Let's use GPS fix update to set position initially.
    purrgo_gnss_solution_t fix = {0};
    fix.valid = true;
    fix.lat_1e7 = lat;
    fix.lon_1e7 = lon;

    // To set scale, we can just press MINUS or PLUS from default scale.
    // Default is 500m (index 5)

    purrgo_app_update(&fix);

    // adjust scale
    purrgo_map_scale_t curr_scale = purrgo_app_get_map_zoom_level();
    while (curr_scale > scale) {
        purrgo_app_handle_button(PURRGO_BTN_PLUS);
        curr_scale = purrgo_app_get_map_zoom_level();
    }
    while (curr_scale < scale) {
        purrgo_app_handle_button(PURRGO_BTN_MINUS);
        curr_scale = purrgo_app_get_map_zoom_level();
    }
}

void test_pan_small_scale() {
    setup_test_state(0, 0, PURRGO_MAP_SCALE_10M);
    int32_t initial_lat = purrgo_app_get_map_center_lat();
    int32_t initial_lon = purrgo_app_get_map_center_lon();

    purrgo_app_handle_button(PURRGO_BTN_UP);
    purrgo_app_handle_button(PURRGO_BTN_RIGHT);

    int32_t new_lat = purrgo_app_get_map_center_lat();
    int32_t new_lon = purrgo_app_get_map_center_lon();

    assert(new_lat > initial_lat);
    assert(new_lon > initial_lon);

    // 10m scale, width 10m, step_m = 2.
    // step_y = 2 * 90 = 180
    assert(new_lat - initial_lat == 180);
    assert(new_lon - initial_lon == 180);
}

void test_pan_large_scale_high_latitude() {
    // We use a high, valid latitude (e.g., 89 degrees N: 890000000) instead of right on the pole.
    // This allows us to push the boundaries of INT32_MAX clamping logic on step_x without testing
    // out-of-bounds geographic coordinates natively on the Y axis.
    setup_test_state(890000000, 0, PURRGO_MAP_SCALE_10000KM);
    int32_t initial_lat = purrgo_app_get_map_center_lat();
    int32_t initial_lon = purrgo_app_get_map_center_lon();

    // Move slightly right and up to test step arithmetic bounds.
    purrgo_app_handle_button(PURRGO_BTN_UP);
    purrgo_app_handle_button(PURRGO_BTN_RIGHT);

    int32_t new_lat = purrgo_app_get_map_center_lat();
    int32_t new_lon = purrgo_app_get_map_center_lon();

    // Width = 10000000m. step_m = 2500000m.
    // step_y_64 = 225,000,000.
    // initial_lat + step_y_64 = 890,000,000 + 225,000,000 = 1,115,000,000.
    // This is within INT32_MAX (2,147,483,647), so no INT32_MAX cap on lat.
    assert(new_lat == 1115000000);

    // cos(89) is very small. Clamped to 100 min internally.
    // step_x_64 = (225000000 * 10000) / 100 = 22,500,000,000.
    // 22.5 billion exceeds INT32_MAX (2.14B). It must be clamped.
    assert(new_lon == 2147483647);
}

void test_pan_coordinate_bounds_clamping() {
    // To ensure next_lat and next_lon clamp safely at INT32_MAX / INT32_MIN boundaries,
    // we set the initial state very close to INT32_MAX and attempt to move beyond it.
    setup_test_state(2000000000, 2000000000, PURRGO_MAP_SCALE_10000KM);

    purrgo_app_handle_button(PURRGO_BTN_UP);
    int32_t new_lat = purrgo_app_get_map_center_lat();

    // 2000000000 + 225000000 = 2225000000, which is > INT32_MAX (2147483647).
    // The coordinate clamp logic should catch it.
    assert(new_lat == 2147483647);

    purrgo_app_handle_button(PURRGO_BTN_RIGHT);
    int32_t new_lon = purrgo_app_get_map_center_lon();
    // step_x clamped to INT32_MAX already, so 2B + 2.14B > INT32_MAX -> Clamped.
    assert(new_lon == 2147483647);

    // Same for negative bounds (INT32_MIN)
    setup_test_state(-2000000000, -2000000000, PURRGO_MAP_SCALE_10000KM);
    purrgo_app_handle_button(PURRGO_BTN_DOWN);
    int32_t current_lat = purrgo_app_get_map_center_lat();
    // Comparing with INT32_MIN macro directly
    assert(current_lat == INT32_MIN);

    // Restore the map state again for LON, because sometimes the button down state modifies step behavior unexpectedly
    setup_test_state(-2000000000, -2000000000, PURRGO_MAP_SCALE_10000KM);

    // Move LEFT directly.
    // step_x at -2B lat is large, so -2000000000 - large > INT32_MIN -> Clamps to INT32_MIN.
    purrgo_app_handle_button(PURRGO_BTN_LEFT);

    int32_t current_lon = purrgo_app_get_map_center_lon();

    // In some compiler variations/C standard versions INT32_MIN is evaluated strangely when typed explicitly in assert
    // We enforce it by comparing manually initialized values.
    int32_t my_min = -2147483647 - 1;
    assert(current_lon == my_min);
}

int main() {
    // Setup basic mock or rely on defaults since config_init logic is needed.
    // config load creates PURRGO.CFG

    test_pan_small_scale();
    test_pan_large_scale_high_latitude();
    test_pan_coordinate_bounds_clamping();

    printf("App FSM tests passed!\n");
    return 0;
}

// Mock filesystem dependencies required by purrgo_core
purrgo_dir_t* purrgo_fs_opendir(const char* path) { return NULL; }
bool purrgo_fs_readdir(purrgo_dir_t* dir, purrgo_fs_dirent_t* entry) { return false; }
void purrgo_fs_closedir(purrgo_dir_t* dir) {}
purrgo_file_t* purrgo_fs_open(const char* path, fs_mode_t mode) { return NULL; }
uint32_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, uint32_t size) { return 0; }
uint32_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* buffer, uint32_t size) { return size; }
void purrgo_fs_close(purrgo_file_t* file) {}
void purrgo_fs_sync(purrgo_file_t* file) {}
