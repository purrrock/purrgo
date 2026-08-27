#include "purrgo/trip_computer.h"

void purrgo_trip_computer_init(void) {
    // No state to initialize yet
}

void purrgo_trip_computer_update(const purrgo_gnss_solution_t* current_fix) {
    // No background update yet
}

bool purrgo_trip_computer_handle_button(purrgo_btn_t button) {
    return false; // Trip computer does not handle any specific buttons right now
}
