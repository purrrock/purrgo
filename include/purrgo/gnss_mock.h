#ifndef PURRGO_GNSS_MOCK_H
#define PURRGO_GNSS_MOCK_H

#include <purrgo/gnss_types.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Initializes the GNSS mock solution state with base coordinates and values.
 * Starting coordinates are strictly 53.7135, 28.4199.
 * Also generates the initial NMEA sentences.
 */
void purrgo_gnss_mock_init(void);

/**
 * Updates the GNSS mock solution state, simulating movement and time progression.
 * Increments seconds (handling rollover) and modifies coordinates slightly.
 * Generates the updated NMEA sentences.
 */
void purrgo_gnss_mock_update(void);

/**
 * Reads a byte from the generated MOCK NMEA stream.
 * @param byte Pointer to the byte variable
 * @return true if a byte was read, false otherwise
 */
bool purrgo_gnss_mock_read_byte(uint8_t *byte);

#endif // PURRGO_GNSS_MOCK_H
