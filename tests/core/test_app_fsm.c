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

void test_pan_large_scale_pole() {
    // Large scale, at pole (latitude near 90, e.g. 900000000)
    setup_test_state(900000000, 0, PURRGO_MAP_SCALE_10000KM);
    int32_t initial_lat = purrgo_app_get_map_center_lat();
    int32_t initial_lon = purrgo_app_get_map_center_lon();

    purrgo_app_handle_button(PURRGO_BTN_UP);
    purrgo_app_handle_button(PURRGO_BTN_RIGHT);

    int32_t new_lat = purrgo_app_get_map_center_lat();
    int32_t new_lon = purrgo_app_get_map_center_lon();

    assert(new_lat > initial_lat);
    assert(new_lon > initial_lon);

    // 10000km scale, width 10000000m, step_m = 2500000.
    // step_y_64 = 2500000 * 90 = 225000000
    // step_x_64 at pole (cos_val clamped to 100):
    // 225000000 * 10000 / 100 = 22500000000
    // Clamped to 2147483647

    assert(new_lat - initial_lat == 225000000);
    assert(new_lon - initial_lon == 2147483647LL);
}

int main() {
    // Setup basic mock or rely on defaults since config_init logic is needed.
    // config load creates PURRGO.CFG

    test_pan_small_scale();
    test_pan_large_scale_pole();

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
