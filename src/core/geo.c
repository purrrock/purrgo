#include "purrgo/geo.h"

/*
 * Placeholder for the portable geographic algorithms.
 *
 * These functions deliberately contain no MCU-specific code. The exact
 * numerical model will be selected and validated during algorithm development.
 */
uint32_t purrgo_distance_m(const purrgo_coord_t *a, const purrgo_coord_t *b)
{
    (void)a;
    (void)b;
    return 0U;
}

uint16_t purrgo_bearing_deg(const purrgo_coord_t *a, const purrgo_coord_t *b)
{
    (void)a;
    (void)b;
    return 0U;
}
