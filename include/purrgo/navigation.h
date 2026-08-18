#ifndef PURRGO_NAVIGATION_H
#define PURRGO_NAVIGATION_H

#include <stdbool.h>
#include <stdint.h>
#include "purrgo/types.h"

typedef struct {
    bool valid;
    purrgo_coord_t position;
    purrgo_motion_t motion;
} purrgo_nav_state_t;

void purrgo_nav_init(purrgo_nav_state_t *state);

#endif /* PURRGO_NAVIGATION_H */
