#ifndef PURRGO_MAP_INTERNAL_H
#define PURRGO_MAP_INTERNAL_H

#include <stdint.h>

/*
 * Shared map subsystem constants and diagnostics.
 */

#define PURRGO_MAP_MAX_PARTS 32
#define PURRGO_MAP_MAX_POINTS 512
#define PURRGO_MAP_READ_CHUNK_SIZE 64
#define PURRGO_MAP_POINTS_PER_CHUNK (PURRGO_MAP_READ_CHUNK_SIZE / 8)

typedef struct {
    uint8_t  file_type;
    uint32_t payload_size;
    uint32_t lod_offset[3];
} pgo_header_info_t;

typedef struct {
    uint32_t sqt_blocks;

    uint32_t nav_visited;

    uint32_t data_visited;
    uint32_t data_passed;
    uint32_t data_culled;

    uint32_t lines_drawn;

    uint32_t polygons_filled;
    uint32_t polygons_skipped;

    uint32_t styles_unknown;
    uint32_t style_none;
} map_diag_t;

/* Little-endian helpers */
static inline int32_t unpack_i32_le(const uint8_t *buf)
{
    return (int32_t)(
        (uint32_t)buf[0] |
        ((uint32_t)buf[1] << 8) |
        ((uint32_t)buf[2] << 16) |
        ((uint32_t)buf[3] << 24)
    );
}

static inline uint32_t unpack_u32_le(const uint8_t *buf)
{
    return
        (uint32_t)buf[0] |
        ((uint32_t)buf[1] << 8) |
        ((uint32_t)buf[2] << 16) |
        ((uint32_t)buf[3] << 24);
}

#endif // PURRGO_MAP_INTERNAL_H
