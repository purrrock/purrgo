#ifndef PURRGO_MAP_RENDER_H
#define PURRGO_MAP_RENDER_H

#include <stdbool.h>
#include <stdint.h>
#include "purrgo/map.h"
#include "purrgo/map_style.h"
#include "map_internal.h"

void map_render_feature(
    purrgo_fs_t *mlp_fs,
    uint32_t v1_offset,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    gfx_context_t *gfx,
    bool is_polygon_layer,
    purrgo_map_style_t style,
    map_diag_t *diag
);

#endif // PURRGO_MAP_RENDER_H
