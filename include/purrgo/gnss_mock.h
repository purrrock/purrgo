#ifndef PURRGO_GNSS_MOCK_H
#define PURRGO_GNSS_MOCK_H

#include <purrgo/gnss_types.h>

/**
 * Initializes the GNSS mock solution state with base coordinates and values.
 * Starting coordinates are strictly 53.7135, 28.4199.
 *
 * @param state Pointer to the GNSS solution structure to initialize.
 */
void purrgo_gnss_mock_init(purrgo_gnss_solution_t* state);

/**
 * Updates the GNSS mock solution state, simulating movement and time progression.
 * Increments seconds (handling rollover) and modifies coordinates slightly.
 *
 * @param state Pointer to the GNSS solution structure to update.
 */
void purrgo_gnss_mock_update(purrgo_gnss_solution_t* state);

#endif // PURRGO_GNSS_MOCK_H
