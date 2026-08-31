#include "purrgo/map.h"
#include "purrgo/logger.h"
#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include "map_internal.h"
#include "map_idx.h"
#include "map_render.h"
#include "purrgo/fs_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static uint32_t core_fs_read_wrapper(void* handle, void* buffer, uint32_t size)
{
    return (uint32_t)purrgo_fs_read(
        (purrgo_file_t*)handle,
        (uint8_t*)buffer,
        (size_t)size
    );
}

static bool core_fs_seek_wrapper(void* handle, uint32_t offset)
{
    return purrgo_fs_seek((purrgo_file_t*)handle, offset);
}

static bool map_parse_pgo_header(purrgo_fs_t *fs, pgo_header_info_t *info)
{
    uint8_t pgo_header[32];

    if (fs->read(fs->handle, pgo_header, sizeof(pgo_header)) != sizeof(pgo_header)) {
        return false;
    }

    if (pgo_header[0] != 'P' ||
        pgo_header[1] != 'G' ||
        pgo_header[2] != 'O') {
        return false;
    }

    info->file_type = pgo_header[3];
    info->payload_size = unpack_u32_le(&pgo_header[4]);
    info->lod_offset[0] = unpack_u32_le(&pgo_header[8]);
    info->lod_offset[1] = unpack_u32_le(&pgo_header[12]);
    info->lod_offset[2] = unpack_u32_le(&pgo_header[16]);

    uint32_t payload_start = 32;

    if (UINT32_MAX - payload_start < info->payload_size) {
        return false;
    }

    uint32_t payload_end = payload_start + info->payload_size;

    if (info->file_type == 1) {
        if (info->lod_offset[0] < payload_start ||
            info->lod_offset[0] >= payload_end) {
            return false;
        }

        if (info->lod_offset[1] < payload_start ||
            info->lod_offset[1] >= payload_end) {
            return false;
        }

        if (info->lod_offset[2] < payload_start ||
            info->lod_offset[2] >= payload_end) {
            return false;
        }

        if (info->lod_offset[0] >= info->lod_offset[1]) {
            return false;
        }

        if (info->lod_offset[1] >= info->lod_offset[2]) {
            return false;
        }
    }

    return true;
}

void purrgo_map_render_layer(
    purrgo_fs_t *idx_fs,
    purrgo_fs_t *mlp_fs,
    purrgo_fs_t *db_fs,
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *viewport,
    purrgo_map_layer_t layer_type
)
{
    if (idx_fs == NULL ||
        gfx == NULL ||
        camera == NULL ||
        viewport == NULL) {
        return;
    }

    map_diag_t diag = {0};
    uint32_t current_idx_offset = 0;

    pgo_header_info_t idx_header = {0};

    if (!map_parse_pgo_header(idx_fs, &idx_header)) {
        return;
    }

    if (idx_header.file_type != 1) {
        return;
    }

    if (layer_type != MAP_LAYER_POIS) {
        if (mlp_fs == NULL) {
            return;
        }

        pgo_header_info_t mlp_header = {0};

        if (!map_parse_pgo_header(mlp_fs, &mlp_header)) {
            return;
        }

        if (mlp_header.file_type != 2) {
            return;
        }
    }

    purrgo_map_scale_t current_scale = purrgo_app_get_map_zoom_level();
    int target_lod = 0;

    if (current_scale <= PURRGO_MAP_SCALE_500M) {
        target_lod = 0;
    } else if (current_scale <= PURRGO_MAP_SCALE_5KM) {
        target_lod = 1;
    } else {
        target_lod = 2;
    }

    uint32_t target_lod_offset = idx_header.lod_offset[target_lod];
    uint32_t lod_end = 0;

    if (target_lod == 0) {
        lod_end = idx_header.lod_offset[1];
    } else if (target_lod == 1) {
        lod_end = idx_header.lod_offset[2];
    } else {
        lod_end = 32 + idx_header.payload_size;
    }

    if (!idx_fs->seek(idx_fs->handle, target_lod_offset)) {
        return;
    }

    current_idx_offset = target_lod_offset;

    if (current_idx_offset + 16 > lod_end) {
        return;
    }

    uint8_t sqt_header[16];

    if (idx_fs->read(
            idx_fs->handle,
            sqt_header,
            sizeof(sqt_header)) != sizeof(sqt_header)) {
        return;
    }

    current_idx_offset += 16;

    if (sqt_header[0] != 'S' ||
        sqt_header[1] != 'Q' ||
        sqt_header[2] != 'T' ||
        sqt_header[3] != 0x01) {
        return;
    }

    diag.sqt_blocks++;

    uint32_t mode = unpack_u32_le(&sqt_header[8]);
    uint32_t count = unpack_u32_le(&sqt_header[12]);

    if (count > 0) {
        bool is_nav = (mode > 0);

        for (uint32_t i = 0; i < count; i++) {
            if (!map_idx_parse_node(
                    idx_fs,
                    &current_idx_offset,
                    mlp_fs,
                    db_fs,
                    is_nav,
                    camera,
                    viewport,
                    gfx,
                    layer_type,
                    &diag,
                    lod_end)) {
                return;
            }
        }
    }
}

bool purrgo_map_render_viewport(
    gfx_context_t *gfx,
    const purrgo_viewport_t *viewport,
    const purrgo_bbox_t *camera,
    const char *map_dir
)
{
    /*
     * Начинаем новый кадр.
     *
     * Очищаем BBox-кэш уже размещённых подписей
     * и очередь отложенных подписей.
     */
    map_render_clear_labels();

    char landuse_idx_path[PURRGO_FS_MAX_PATH];
    char landuse_mlp_path[PURRGO_FS_MAX_PATH];
    char landuse_db_path[PURRGO_FS_MAX_PATH];

    char idx_path[PURRGO_FS_MAX_PATH];
    char mlp_path[PURRGO_FS_MAX_PATH];

    char poi_idx_path[PURRGO_FS_MAX_PATH];
    char poi_db_path[PURRGO_FS_MAX_PATH];

    /* ------------------- LANDUSE ------------------- */

    snprintf(
        landuse_idx_path,
        sizeof(landuse_idx_path),
        "%s/landuse.idx",
        map_dir
    );

    snprintf(
        landuse_mlp_path,
        sizeof(landuse_mlp_path),
        "%s/landuse.mlp",
        map_dir
    );

    snprintf(
        landuse_db_path,
        sizeof(landuse_db_path),
        "%s/landuse.db",
        map_dir
    );

    purrgo_file_t* landuse_idx_file =
        purrgo_fs_open(landuse_idx_path, FS_READ);

    purrgo_file_t* landuse_mlp_file =
        purrgo_fs_open(landuse_mlp_path, FS_READ);

    purrgo_file_t* landuse_db_file =
        purrgo_fs_open(landuse_db_path, FS_READ);

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

        purrgo_fs_t landuse_db_fs = {
            .handle = landuse_db_file,
            .read = core_fs_read_wrapper,
            .seek = core_fs_seek_wrapper
        };

        gfx_set_color(gfx, 2, 3);

        purrgo_map_render_layer(
            &landuse_idx_fs,
            &landuse_mlp_fs,
            landuse_db_file ? &landuse_db_fs : NULL,
            gfx,
            camera,
            viewport,
            MAP_LAYER_POLYGONS
        );

        landuse_success = true;
    }

    if (landuse_idx_file) {
        purrgo_fs_close(landuse_idx_file);
    }

    if (landuse_mlp_file) {
        purrgo_fs_close(landuse_mlp_file);
    }

    if (landuse_db_file) {
        purrgo_fs_close(landuse_db_file);
    }

    /* ------------------- ROADS ------------------- */

    snprintf(
        idx_path,
        sizeof(idx_path),
        "%s/roads.idx",
        map_dir
    );

    snprintf(
        mlp_path,
        sizeof(mlp_path),
        "%s/roads.mlp",
        map_dir
    );

    purrgo_file_t* idx_file =
        purrgo_fs_open(idx_path, FS_READ);

    purrgo_file_t* mlp_file =
        purrgo_fs_open(mlp_path, FS_READ);

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

        /*
         * Дороги не подписываем —
         * передаём NULL вместо db_fs.
         */
        purrgo_map_render_layer(
            &idx_fs,
            &mlp_fs,
            NULL,
            gfx,
            camera,
            viewport,
            MAP_LAYER_LINES
        );

        roads_success = true;
    }

    if (idx_file) {
        purrgo_fs_close(idx_file);
    }

    if (mlp_file) {
        purrgo_fs_close(mlp_file);
    }

    /*
     * ------------------------------------------------
     * LANDUSE LABELS
     * ------------------------------------------------
     *
     * К этому моменту:
     *
     *   1. все landuse-полигоны уже нарисованы;
     *   2. все roads уже нарисованы.
     *
     * Поэтому теперь можно вывести отложенные
     * подписи landuse поверх дорог.
     *
     * map_render_draw_queued_labels() также выполняет
     * проверку коллизий и резервирует BBox каждой
     * успешно размещённой подписи.
     */
    map_render_draw_queued_labels(gfx);

    /* ------------------- POIS ------------------- */

    snprintf(
        poi_idx_path,
        sizeof(poi_idx_path),
        "%s/pois.idx",
        map_dir
    );

    snprintf(
        poi_db_path,
        sizeof(poi_db_path),
        "%s/pois.db",
        map_dir
    );

    purrgo_file_t* poi_idx_file =
        purrgo_fs_open(poi_idx_path, FS_READ);

    purrgo_file_t* poi_db_file =
        purrgo_fs_open(poi_db_path, FS_READ);

    bool poi_success = true;

    if (poi_idx_file) {

        purrgo_fs_t poi_idx_fs = {
            .handle = poi_idx_file,
            .read = core_fs_read_wrapper,
            .seek = core_fs_seek_wrapper
        };

        purrgo_fs_t poi_db_fs = {
            .handle = poi_db_file,
            .read = core_fs_read_wrapper,
            .seek = core_fs_seek_wrapper
        };

        purrgo_map_render_layer(
            &poi_idx_fs,
            NULL,
            poi_db_file ? &poi_db_fs : NULL,
            gfx,
            camera,
            viewport,
            MAP_LAYER_POIS
        );
    }

    if (poi_idx_file) {
        purrgo_fs_close(poi_idx_file);
    }

    if (poi_db_file) {
        purrgo_fs_close(poi_db_file);
    }

    return landuse_success && roads_success && poi_success;
}