#ifndef PURRGO_MAP_IDX_H
#define PURRGO_MAP_IDX_H

#include <stdint.h>
#include <stdbool.h>

#include "purrgo/map.h"
#include "map_internal.h"
#include "map_projection.h"

/*
 * Разбирает один Data Node IDX.
 *
 * layer_type определяет, какой тип объектов должен обрабатываться:
 *
 *     MAP_LAYER_LINES
 *     MAP_LAYER_POLYGONS
 *     MAP_LAYER_POIS
 *
 * Для MAP_LAYER_LINES и MAP_LAYER_POLYGONS используется MLP.
 *
 * Для MAP_LAYER_POIS MLP не требуется:
 * координаты точки находятся непосредственно в BBox Data Node.
 */
bool map_idx_parse_node(
    purrgo_fs_t *idx_fs,
    uint32_t *current_idx_offset,
    purrgo_fs_t *mlp_fs,
    purrgo_fs_t *db_fs, // <--- ДОБАВЛЕНО
    bool is_nav_node,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    gfx_context_t *gfx,
    bool is_polygon_layer,
    map_diag_t *diag,
    uint32_t lod_end
);

#endif /* PURRGO_MAP_IDX_H */