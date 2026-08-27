#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "src/core/purrgo/map.h"
#include "src/core/purrgo/geo.h"

// we need to write a standalone test to see why purrgo_app_get_map_center_lon() changed from 0
// It seems update(&fix) where fix is inside FOLLOW_STOP might be modifying the camera?
// Let's check apply_auto_follow bounds logic.
