#ifndef PURRGO_MAP_MLP_H
#define PURRGO_MAP_MLP_H

#include <stdint.h>
#include <stdbool.h>
#include "purrgo/map.h"
#include "map_internal.h"

typedef struct {
    purrgo_fs_t *mlp_fs;
    uint32_t num_parts;
    uint32_t num_points;
    uint32_t parts[PURRGO_MAP_MAX_PARTS];

    // Iteration state
    uint32_t points_read;
    uint32_t current_part_idx;
    uint32_t next_part_start;

    uint8_t chunk_buf[PURRGO_MAP_READ_CHUNK_SIZE];
    uint32_t chunk_points_read;
    uint32_t points_in_chunk;
} map_mlp_iter_t;

/* Initializes iterator, reads header and parts */
bool map_mlp_iter_init(
    map_mlp_iter_t *iter,
    purrgo_fs_t *mlp_fs,
    uint32_t v1_offset
);

/* Returns next point, setting is_new_part if a new part begins */
bool map_mlp_iter_next(
    map_mlp_iter_t *iter,
    int32_t *raw_x,
    int32_t *raw_y,
    bool *is_new_part
);

#endif // PURRGO_MAP_MLP_H
