#include "purrgo/map.h"
#include "purrgo/logger.h"
#include "map_internal.h"
#include "map_idx.h"
#include <stdbool.h>
#include <stddef.h>

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

    /* ---------------------------------------------------------------------- */
    /* SQT sections                                                            */
    /* ---------------------------------------------------------------------- */

    while (true) {
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

        if (diag.sqt_blocks > 0) {
            break;
        }

        diag.sqt_blocks++;

        uint32_t mode = unpack_u32_le(&sqt_header[8]);
        uint32_t count = unpack_u32_le(&sqt_header[12]);

        if (count == 0) {
            continue;
        }

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
