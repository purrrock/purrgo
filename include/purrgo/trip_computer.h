#ifndef PURRGO_TRIP_COMPUTER_H
#define PURRGO_TRIP_COMPUTER_H

#include "purrgo/gnss_types.h"
#include "purrgo/app_fsm.h"
#include <stdbool.h>

void purrgo_trip_computer_init(void);
void purrgo_trip_computer_update(const purrgo_gnss_solution_t* current_fix);
bool purrgo_trip_computer_handle_button(purrgo_btn_t button);

#endif // PURRGO_TRIP_COMPUTER_H
