#include <stdio.h>
#include <assert.h>
#include "purrgo/app_ui.h"
#include "purrgo/app_fsm.h"

// Mock state
static purrgo_state_t mock_state = APP_STATE_MAP;

purrgo_state_t purrgo_app_get_state(void) {
    return mock_state;
}

// Call counters for the mocked functions
int call_ui_render_menu_config = 0;
int call_ui_trip_render_grid = 0;
int call_ui_trip_render_values = 0;
int call_ui_render_map = 0;
int call_ui_render_menu_dir_select = 0;
int call_ui_render_menu_map_layers = 0;

void reset_call_counters() {
    call_ui_render_menu_config = 0;
    call_ui_trip_render_grid = 0;
    call_ui_trip_render_values = 0;
    call_ui_render_map = 0;
    call_ui_render_menu_dir_select = 0;
    call_ui_render_menu_map_layers = 0;
}

void ui_render_menu_config(gfx_context_t* gfx) {
    call_ui_render_menu_config++;
}

void ui_trip_render_grid(gfx_context_t* gfx) {
    call_ui_trip_render_grid++;
}

void ui_trip_render_values(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, const purrgo_sun_info_t* sun) {
    call_ui_trip_render_values++;
}

void ui_render_map(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, const purrgo_sun_info_t* sun) {
    call_ui_render_map++;
}

void ui_render_menu_dir_select(gfx_context_t* gfx) {
    call_ui_render_menu_dir_select++;
}

void ui_render_menu_map_layers(gfx_context_t* gfx) {
    call_ui_render_menu_map_layers++;
}

void test_app_ui_render_map() {
    gfx_context_t gfx = {0};
    purrgo_gnss_solution_t gnss = {0};
    purrgo_sun_info_t sun = {0};

    reset_call_counters();

    // Test that APP_STATE_MAP calls ui_render_map
    mock_state = APP_STATE_MAP;
    purrgo_app_ui_render(&gfx, &gnss, &sun);

    assert(call_ui_render_map == 1);
    assert(call_ui_render_menu_config == 0);
    assert(call_ui_trip_render_grid == 0);
    assert(call_ui_trip_render_values == 0);
    assert(call_ui_render_menu_dir_select == 0);
}

void test_app_ui_render_trip_computer() {
    gfx_context_t gfx = {0};
    purrgo_gnss_solution_t gnss = {0};
    purrgo_sun_info_t sun = {0};

    reset_call_counters();

    // Initial transition to APP_STATE_TRIP_COMPUTER should call both grid and values renderers
    // The previous state was APP_STATE_MAP from the previous test or default init.
    mock_state = APP_STATE_TRIP_COMPUTER;
    purrgo_app_ui_render(&gfx, &gnss, &sun);

    assert(call_ui_trip_render_grid == 1);
    assert(call_ui_trip_render_values == 1);

    reset_call_counters();

    // Subsequent calls in the same state should not call grid renderer again
    purrgo_app_ui_render(&gfx, &gnss, &sun);

    assert(call_ui_trip_render_grid == 0);
    assert(call_ui_trip_render_values == 1);

    assert(call_ui_render_map == 0);
    assert(call_ui_render_menu_config == 0);
    assert(call_ui_render_menu_dir_select == 0);
}

void test_app_ui_render_menu_config() {
    gfx_context_t gfx = {0};
    purrgo_gnss_solution_t gnss = {0};
    purrgo_sun_info_t sun = {0};

    reset_call_counters();

    // Test that APP_STATE_MENU_CONFIG calls ui_render_menu_config
    mock_state = APP_STATE_MENU_CONFIG;
    purrgo_app_ui_render(&gfx, &gnss, &sun);

    assert(call_ui_render_menu_config == 1);
    assert(call_ui_render_map == 0);
    assert(call_ui_trip_render_grid == 0);
    assert(call_ui_trip_render_values == 0);
    assert(call_ui_render_menu_dir_select == 0);
}

void test_app_ui_render_menu_dir_select() {
    gfx_context_t gfx = {0};
    purrgo_gnss_solution_t gnss = {0};
    purrgo_sun_info_t sun = {0};

    reset_call_counters();

    // Test that APP_STATE_MENU_DIR_SELECT calls ui_render_menu_dir_select
    mock_state = APP_STATE_MENU_DIR_SELECT;
    purrgo_app_ui_render(&gfx, &gnss, &sun);

    assert(call_ui_render_menu_dir_select == 1);
    assert(call_ui_render_map == 0);
    assert(call_ui_trip_render_grid == 0);
    assert(call_ui_trip_render_values == 0);
    assert(call_ui_render_menu_config == 0);
    assert(call_ui_render_menu_map_layers == 0);
}

void test_app_ui_render_menu_map_layers() {
    gfx_context_t gfx = {0};
    purrgo_gnss_solution_t gnss = {0};
    purrgo_sun_info_t sun = {0};

    reset_call_counters();

    // Test that APP_STATE_MENU_MAP_LAYERS calls ui_render_menu_map_layers
    mock_state = APP_STATE_MENU_MAP_LAYERS;
    purrgo_app_ui_render(&gfx, &gnss, &sun);

    assert(call_ui_render_menu_map_layers == 1);
    assert(call_ui_render_menu_dir_select == 0);
    assert(call_ui_render_map == 0);
    assert(call_ui_trip_render_grid == 0);
    assert(call_ui_trip_render_values == 0);
    assert(call_ui_render_menu_config == 0);
}

void test_app_ui_render_unknown_state() {
    gfx_context_t gfx = {0};
    purrgo_gnss_solution_t gnss = {0};
    purrgo_sun_info_t sun = {0};

    reset_call_counters();

    // Test that an unknown state does not call any UI rendering function
    mock_state = (purrgo_state_t)999; // Arbitrary unknown state
    purrgo_app_ui_render(&gfx, &gnss, &sun);

    assert(call_ui_render_map == 0);
    assert(call_ui_trip_render_grid == 0);
    assert(call_ui_trip_render_values == 0);
    assert(call_ui_render_menu_config == 0);
    assert(call_ui_render_menu_dir_select == 0);
}

int main() {
    // Run tests
    // Note: the order matters because `purrgo_app_ui_render` has static internal state `prev_state`
    test_app_ui_render_map(); // This transitions prev_state to APP_STATE_MAP
    test_app_ui_render_trip_computer(); // This transitions from MAP to TRIP_COMPUTER, testing the `prev_state != APP_STATE_TRIP_COMPUTER` logic
    test_app_ui_render_menu_config();
    test_app_ui_render_menu_dir_select();
    test_app_ui_render_menu_map_layers();
    test_app_ui_render_unknown_state();

    printf("App UI tests passed!\n");
    return 0;
}
