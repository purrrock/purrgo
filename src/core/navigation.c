#include "purrgo/navigation.h"

void purrgo_nav_init(purrgo_nav_state_t *state)
{
    state->valid = false;
    state->position.latitude_e7 = 0;
    state->position.longitude_e7 = 0;
    state->motion.altitude_mm = 0;
    state->motion.speed_mm_s = 0U;
    state->motion.course_cdeg = 0U;
}
