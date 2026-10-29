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

/*
 * true, если объект с bbox (xmin..xmax, ymin..ymax) на экране вьюпорта vp при
 * камере cam занимает меньше min_px пикселей по обеим осям (его можно не
 * рисовать). min_px <= 0 или пустой вьюпорт/камера: всегда false.
 */
bool map_feature_too_small(
    int32_t xmin,
    int32_t ymin,
    int32_t xmax,
    int32_t ymax,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    int32_t min_px
);

#endif // PURRGO_MAP_CULLING_H
