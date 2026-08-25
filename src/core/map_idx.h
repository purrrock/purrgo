#ifndef PURRGO_MAP_IDX_H
#define PURRGO_MAP_IDX_H

#include <stdint.h>
#include <stdbool.h>
#include "purrgo/map.h"
#include "map_internal.h"

void map_idx_parse_node(
    purrgo_fs_t *idx_fs,
    uint32_t *current_idx_offset,
    purrgo_fs_t *mlp_fs,
    bool is_nav_node,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    gfx_context_t *gfx,
    bool is_polygon_layer,
    map_diag_t *diag
);

bool map_idx_skip_sqt_block(purrgo_fs_t *idx_fs, uint32_t *current_idx_offset);

#endif // PURRGO_MAP_IDX_H
