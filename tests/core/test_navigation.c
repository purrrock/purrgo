#include "purrgo/navigation.h"
#include <assert.h>
#include <stdio.h>

static void test_navigation_invalid_fix(void) {
    purrgo_gnss_solution_t fix = {0};
    fix.valid = false; // The fix is invalid
    fix.lat_1e7 = 10000000;
    fix.lon_1e7 = 20000000;

    purrgo_waypoint_t wp = {0};
    wp.lat_1e7 = 10000000;
    wp.lon_1e7 = 20000000;

    purrgo_nav_status_t status = {0};
    status.distance_to_wp_m = 999;
    status.bearing_to_wp_deg = 999;
    status.is_arrived = false;

    // Call the function
    purrgo_nav_update(&fix, &wp, 15, &status);

    // Status should not be updated because fix is invalid
    assert(status.distance_to_wp_m == 999);
    assert(status.bearing_to_wp_deg == 999);
}

static void test_navigation_null_pointers(void) {
    purrgo_gnss_solution_t fix = {0};
    fix.valid = true;

    purrgo_waypoint_t wp = {0};

    purrgo_nav_status_t status = {0};
    status.distance_to_wp_m = 999;

    // Null current_fix
    purrgo_nav_update(NULL, &wp, 15, &status);
    assert(status.distance_to_wp_m == 999);

    // Null target_wp
    purrgo_nav_update(&fix, NULL, 15, &status);
    assert(status.distance_to_wp_m == 999);

    // Null status
    purrgo_nav_update(&fix, &wp, 15, NULL); // Should not crash
}

int main(void) {
    test_navigation_invalid_fix();
    test_navigation_null_pointers();

    printf("All navigation tests passed.\n");
    return 0;
}
