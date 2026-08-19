#include <purrgo/gnss_mock.h>

void purrgo_gnss_mock_init(purrgo_gnss_solution_t* state) {
    if (!state) {
        return;
    }

    state->valid = true; // 3D FIX

    // Base coordinates: 53.7135, 28.4199
    // Converted to int32_t * 10^7
    state->lat_1e7 = 537135000;
    state->lon_1e7 = 284199000;

    // Static Mock data
    state->speed_knots = 269; // 5km/h ~ 2.69 knots -> 269
    state->alt_m = 150;
    state->satellites_tracked = 9;

    // Static time and date
    state->hours = 12;
    state->minutes = 34;
    state->seconds = 56;
    state->day = 1;
    state->month = 1;
    state->year = 24;
}

void purrgo_gnss_mock_update(purrgo_gnss_solution_t* state) {
    if (!state) {
        return;
    }

    // Time progression logic
    // Increment seconds by 1
    state->seconds++;

    // If seconds reach 60, reset to 0 and increment minutes
    if (state->seconds >= 60) {
        state->seconds = 0;
        state->minutes++;

        // If minutes reach 60, reset to 0 and increment hours
        if (state->minutes >= 60) {
            state->minutes = 0;
            // Hours wrap around at 24 using modulo operation
            state->hours = (state->hours + 1) % 24;
        }
    }

    // Simulate movement by slightly changing coordinates.
    // Adding 10 to latitude (approx 1 meter)
    state->lat_1e7 += 10;
    // Adding 15 to longitude (approx 1.5 meters)
    state->lon_1e7 += 15;
}
