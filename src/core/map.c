#include "purrgo/map.h"
#include "purrgo/logger.h"
#include "purrgo/app_fsm.h"
#include "map_internal.h"
#include "map_idx.h"
#include "purrgo/fs_hal.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Локальные обертки для согласования сигнатур fs_hal.h и purrgo_fs_t */
static uint32_t core_fs_read_wrapper(void* handle, void* buffer, uint32_t size) {
    return (uint32_t)purrgo_fs_read((purrgo_file_t*)handle, (uint8_t*)buffer, (size_t)size);
}

static bool core_fs_seek_wrapper(void* handle, uint32_t offset) {
    return purrgo_fs_seek((purrgo_file_t*)handle, offset);
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

void purrgo_map_render_layer(
    purrgo_fs_t *idx_fs,
    purrgo_fs_t *mlp_fs,
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *viewport,
    bool is_polygon_layer
) {
    if (
        idx_fs == NULL ||
        mlp_fs == NULL ||
        gfx == NULL ||
        camera == NULL ||
        viewport == NULL
    ) {
        return;
    }

    PURRGO_LOG("MAP: IDX opened\n");

    PURRGO_LOG(
        "MAP: CAMERA "
        "min=(%ld,%ld) "
        "max=(%ld,%ld)\n",
        (long)camera->min_x,
        (long)camera->min_y,
        (long)camera->max_x,
        (long)camera->max_y
    );

    PURRGO_LOG(
        "MAP: VIEWPORT "
        "offset=(%d,%d) "
        "size=(%u,%u)\n",
        (int)viewport->offset_x,
        (int)viewport->offset_y,
        (unsigned)viewport->width,
        (unsigned)viewport->height
    );

    PURRGO_LOG(
        "MAP: GEOMETRY TYPE=%s\n",
        is_polygon_layer
            ? "POLYGON"
            : "LINE"
    );

    map_diag_t diag = {0};

    uint32_t current_idx_offset = 0;

    /* ---------------------------------------------------------------------- */
    /* YZL header                                                              */
    /* ---------------------------------------------------------------------- */

    uint8_t yzl_header[32];

    if (
        idx_fs->read(
            idx_fs->handle,
            yzl_header,
            sizeof(yzl_header)
        ) != sizeof(yzl_header)
    ) {
        PURRGO_LOG("MAP: ERROR reading YZL header\n");
        return;
    }

    current_idx_offset += 32;

    if (
        yzl_header[0] != 'Y' ||
        yzl_header[1] != 'Z' ||
        yzl_header[2] != 'L'
    ) {
        PURRGO_LOG("MAP: ERROR invalid YZL header\n");
        return;
    }

    uint32_t payload_size = unpack_u32_le(&yzl_header[4]);
    uint32_t max_idx_offset = 32 + payload_size;

    /* ---------------------------------------------------------------------- */
    /* SQT sections                                                            */
    /* ---------------------------------------------------------------------- */

    purrgo_map_scale_t current_scale = purrgo_app_get_map_zoom_level();
    int target_lod = 0;

    if (current_scale <= PURRGO_MAP_SCALE_500M) {
        target_lod = 0;
    } else if (current_scale <= PURRGO_MAP_SCALE_5KM) {
        target_lod = 1;
    } else {
        target_lod = 2;
    }

    int current_lod = 0;

    while (true) {
        if (current_lod < target_lod) {
            if (!map_idx_skip_sqt_block(idx_fs, &current_idx_offset, max_idx_offset)) {
                break;
            }
            current_lod++;
            continue;
        }

        uint8_t sqt_header[16];

        if (
            idx_fs->read(
                idx_fs->handle,
                sqt_header,
                sizeof(sqt_header)
            ) != sizeof(sqt_header)
        ) {
            break;
        }

        current_idx_offset += 16;

        if (
            sqt_header[0] != 'S' ||
            sqt_header[1] != 'Q' ||
            sqt_header[2] != 'T' ||
            sqt_header[3] != 0x01
        ) {
            PURRGO_LOG("MAP: ERROR invalid SQT header\n");
            break;
        }

        diag.sqt_blocks++;

        uint32_t mode = unpack_u32_le(&sqt_header[8]);
        uint32_t count = unpack_u32_le(&sqt_header[12]);

        if (count > 0) {
            bool is_nav = (mode > 0);

            for (uint32_t i = 0; i < count; i++) {
                map_idx_parse_node(
                    idx_fs,
                    &current_idx_offset,
                    mlp_fs,
                    is_nav,
                    camera,
                    viewport,
                    gfx,
                    is_polygon_layer,
                    &diag
                );
            }
        }

        // Target LOD block has been processed. Break immediately to avoid reading next LOD blocks.
        break;
    }

    /* ---------------------------------------------------------------------- */
    /* Diagnostics                                                             */
    /* ---------------------------------------------------------------------- */

    PURRGO_LOG(
        "MAP: "
        "SQT=%u "
        "NAV=%u "
        "DATA=%u "
        "PASS=%u "
        "CULL=%u "
        "LINES=%u "
        "POLYGONS=%u "
        "SKIPPED=%u\n",

        (unsigned)diag.sqt_blocks,
        (unsigned)diag.nav_visited,
        (unsigned)diag.data_visited,
        (unsigned)diag.data_passed,
        (unsigned)diag.data_culled,
        (unsigned)diag.lines_drawn,
        (unsigned)diag.polygons_filled,
        (unsigned)diag.polygons_skipped
    );
}

bool purrgo_map_render_viewport(
    gfx_context_t *gfx,
    const purrgo_viewport_t *viewport,
    const purrgo_bbox_t *camera,
    const char *map_dir
) {
    char landuse_idx_path[PURRGO_FS_MAX_PATH];
    char landuse_mlp_path[PURRGO_FS_MAX_PATH];
    char idx_path[PURRGO_FS_MAX_PATH];
    char mlp_path[PURRGO_FS_MAX_PATH];

    snprintf(landuse_idx_path, sizeof(landuse_idx_path), "%s/landuse.idx", map_dir);
    snprintf(landuse_mlp_path, sizeof(landuse_mlp_path), "%s/landuse.mlp", map_dir);
    snprintf(idx_path, sizeof(idx_path), "%s/roads.idx", map_dir);
    snprintf(mlp_path, sizeof(mlp_path), "%s/roads.mlp", map_dir);

    purrgo_file_t* landuse_idx_file = purrgo_fs_open(landuse_idx_path, FS_READ);
    purrgo_file_t* landuse_mlp_file = purrgo_fs_open(landuse_mlp_path, FS_READ);

    bool landuse_success = false;
    if (landuse_idx_file && landuse_mlp_file) {
        purrgo_fs_t landuse_idx_fs = {
            .handle = landuse_idx_file,
            .read = core_fs_read_wrapper,
            .seek = core_fs_seek_wrapper
        };

        purrgo_fs_t landuse_mlp_fs = {
            .handle = landuse_mlp_file,
            .read = core_fs_read_wrapper,
            .seek = core_fs_seek_wrapper
        };

        gfx_set_color(gfx, 2, 3);
        purrgo_map_render_layer(&landuse_idx_fs, &landuse_mlp_fs, gfx, camera, viewport, true);
        landuse_success = true;
    }

    if (landuse_idx_file) purrgo_fs_close(landuse_idx_file);
    if (landuse_mlp_file) purrgo_fs_close(landuse_mlp_file);

    purrgo_file_t* idx_file = purrgo_fs_open(idx_path, FS_READ);
    purrgo_file_t* mlp_file = purrgo_fs_open(mlp_path, FS_READ);

    bool roads_success = false;
    if (idx_file && mlp_file) {
        purrgo_fs_t idx_fs = {
            .handle = idx_file,
            .read = core_fs_read_wrapper,
            .seek = core_fs_seek_wrapper
        };

        purrgo_fs_t mlp_fs = {
            .handle = mlp_file,
            .read = core_fs_read_wrapper,
            .seek = core_fs_seek_wrapper
        };

        gfx_set_color(gfx, 1, 3);
        purrgo_map_render_layer(&idx_fs, &mlp_fs, gfx, camera, viewport, false);
        roads_success = true;
    }

    if (idx_file) purrgo_fs_close(idx_file);
    if (mlp_file) purrgo_fs_close(mlp_file);

    return landuse_success && roads_success;
}
