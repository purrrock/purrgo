#ifndef PURRGO_MAP_IDX_H
#define PURRGO_MAP_IDX_H

#include <stdint.h>
#include <stdbool.h>
#include "purrgo/map.h"
#include "map_internal.h"

bool map_idx_parse_node(
    purrgo_file_t *idx_file,
    uint32_t *current_idx_offset,
    purrgo_file_t *mlp_file,
    purrgo_file_t *db_file,
    bool is_nav_node,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    gfx_context_t *gfx,
    purrgo_map_layer_t layer_type,
    const purrgo_map_render_params_t *params,
    uint32_t lod_end
);

#endif // PURRGO_MAP_IDX_H