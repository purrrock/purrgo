#ifndef PURRGO_MAP_PROJECTION_H
#define PURRGO_MAP_PROJECTION_H

#include <stdint.h>
#include "purrgo/map.h"

int64_t camera_span_x(const purrgo_bbox_t *cam);

void project_to_screen(
    int32_t lon,
    int32_t lat,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    int16_t *sx,
    int16_t *sy
);

#endif // PURRGO_MAP_PROJECTION_H
