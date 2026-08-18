#ifndef PURRGO_GEO_H
#define PURRGO_GEO_H

#include <stdint.h>
#include "purrgo/types.h"

/* Portable geodesic helpers. Coordinate units are documented in types.h. */
uint32_t purrgo_distance_m(const purrgo_coord_t *a, const purrgo_coord_t *b);
uint16_t purrgo_bearing_deg(const purrgo_coord_t *a, const purrgo_coord_t *b);

#endif /* PURRGO_GEO_H */
