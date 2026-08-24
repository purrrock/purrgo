#ifndef PURRGO_MAP_CULLING_H
#define PURRGO_MAP_CULLING_H

#include <stdbool.h>
#include <stdint.h>
#include "purrgo/map.h"

bool bbox_intersects_camera(
    int32_t xmin,
    int32_t ymin,
    int32_t xmax,
    int32_t ymax,
    const purrgo_bbox_t *cam
);

#endif // PURRGO_MAP_CULLING_H
