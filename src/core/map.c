#include "purrgo/map.h"
#include "purrgo/logger.h"
#include "purrgo/app_fsm.h"
#include "purrgo/map_controller.h"
#include "purrgo/config.h"
#include "map_internal.h"
#include "map_idx.h"
#include "map_render.h"
#include "purrgo/fs_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include "purrgo/purrgo_format.h"
#include <string.h>

/*
 * Persistent map file context
 * We maintain these files open to preserve the LRU cache benefits across renders.
 */
typedef struct {
    char current_map_dir[PURRGO_FS_MAX_PATH];

    bool layer_landuse;
    bool layer_landuse_labels;
    bool layer_roads;
    bool layer_poi;
    bool layer_poi_labels;

    purrgo_file_t* landuse_idx_file;
    purrgo_file_t* landuse_mlp_file;
    purrgo_file_t* landuse_db_file;

    purrgo_file_t* roads_idx_file;
    purrgo_file_t* roads_mlp_file;

    purrgo_file_t* poi_idx_file;
    purrgo_file_t* poi_db_file;
} map_context_t;

static map_context_t s_map_context = {0};

static void map_close_file(purrgo_file_t **file)
{
    if (*file != NULL) { purrgo_fs_close(*file); *file = NULL; }
}

static void map_close_all_files(void)
{
    map_close_file(&s_map_context.landuse_idx_file);
    map_close_file(&s_map_context.landuse_mlp_file);
    map_close_file(&s_map_context.landuse_db_file);

    map_close_file(&s_map_context.roads_idx_file);
    map_close_file(&s_map_context.roads_mlp_file);

    map_close_file(&s_map_context.poi_idx_file);
    map_close_file(&s_map_context.poi_db_file);

    s_map_context.layer_landuse = false;
    s_map_context.layer_landuse_labels = false;
    s_map_context.layer_roads = false;
    s_map_context.layer_poi = false;
    s_map_context.layer_poi_labels = false;

    s_map_context.current_map_dir[0] = '\0';
}

static void map_sync_files(const char* map_dir)
{
    bool dir_changed = (strncmp(s_map_context.current_map_dir, map_dir, sizeof(s_map_context.current_map_dir)) != 0);

    if (dir_changed) {
        map_close_all_files();
        strncpy(s_map_context.current_map_dir, map_dir, sizeof(s_map_context.current_map_dir) - 1);
        s_map_context.current_map_dir[sizeof(s_map_context.current_map_dir) - 1] = '\0';
    }

    char path_buf[PURRGO_FS_MAX_PATH];

    /* Landuse */
    if (app_config.layer_landuse != s_map_context.layer_landuse) {
        if (!app_config.layer_landuse) {
            if (s_map_context.landuse_idx_file) { purrgo_fs_close(s_map_context.landuse_idx_file); s_map_context.landuse_idx_file = NULL; }
            if (s_map_context.landuse_mlp_file) { purrgo_fs_close(s_map_context.landuse_mlp_file); s_map_context.landuse_mlp_file = NULL; }
            if (s_map_context.landuse_db_file) { purrgo_fs_close(s_map_context.landuse_db_file); s_map_context.landuse_db_file = NULL; }
            s_map_context.layer_landuse = false;
            s_map_context.layer_landuse_labels = false;
        } else {
            purrgo_snprintf(path_buf, sizeof(path_buf), "%s/landuse.idx", map_dir);
            purrgo_file_t* idx = purrgo_fs_open(path_buf, FS_READ);

            purrgo_snprintf(path_buf, sizeof(path_buf), "%s/landuse.mlp", map_dir);
            purrgo_file_t* mlp = purrgo_fs_open(path_buf, FS_READ);

            if (idx && mlp) {
                s_map_context.landuse_idx_file = idx;
                s_map_context.landuse_mlp_file = mlp;
                s_map_context.layer_landuse = true;
            } else {
                if (idx) purrgo_fs_close(idx);
                if (mlp) purrgo_fs_close(mlp);
                s_map_context.layer_landuse = false;
            }
        }
    }

    if (s_map_context.layer_landuse && (app_config.layer_landuse_labels != s_map_context.layer_landuse_labels)) {
        if (!app_config.layer_landuse_labels) {
            if (s_map_context.landuse_db_file) { purrgo_fs_close(s_map_context.landuse_db_file); s_map_context.landuse_db_file = NULL; }
            s_map_context.layer_landuse_labels = false;
        } else {
            purrgo_snprintf(path_buf, sizeof(path_buf), "%s/landuse.db", map_dir);
            purrgo_file_t* db = purrgo_fs_open(path_buf, FS_READ);
            if (db) {
                s_map_context.landuse_db_file = db;
                s_map_context.layer_landuse_labels = true;
            } else {
                s_map_context.layer_landuse_labels = false;
            }
        }
    }

    /* Roads */
    if (app_config.layer_roads != s_map_context.layer_roads) {
        if (!app_config.layer_roads) {
            if (s_map_context.roads_idx_file) { purrgo_fs_close(s_map_context.roads_idx_file); s_map_context.roads_idx_file = NULL; }
            if (s_map_context.roads_mlp_file) { purrgo_fs_close(s_map_context.roads_mlp_file); s_map_context.roads_mlp_file = NULL; }
            s_map_context.layer_roads = false;
        } else {
            purrgo_snprintf(path_buf, sizeof(path_buf), "%s/roads.idx", map_dir);
            purrgo_file_t* idx = purrgo_fs_open(path_buf, FS_READ);

            purrgo_snprintf(path_buf, sizeof(path_buf), "%s/roads.mlp", map_dir);
            purrgo_file_t* mlp = purrgo_fs_open(path_buf, FS_READ);

            if (idx && mlp) {
                s_map_context.roads_idx_file = idx;
                s_map_context.roads_mlp_file = mlp;
                s_map_context.layer_roads = true;
            } else {
                if (idx) purrgo_fs_close(idx);
                if (mlp) purrgo_fs_close(mlp);
                s_map_context.layer_roads = false;
            }
        }
    }

    /* POIs */
    if (app_config.layer_poi != s_map_context.layer_poi) {
        if (!app_config.layer_poi) {
            if (s_map_context.poi_idx_file) { purrgo_fs_close(s_map_context.poi_idx_file); s_map_context.poi_idx_file = NULL; }
            if (s_map_context.poi_db_file) { purrgo_fs_close(s_map_context.poi_db_file); s_map_context.poi_db_file = NULL; }
            s_map_context.layer_poi = false;
            s_map_context.layer_poi_labels = false;
        } else {
            purrgo_snprintf(path_buf, sizeof(path_buf), "%s/pois.idx", map_dir);
            purrgo_file_t* idx = purrgo_fs_open(path_buf, FS_READ);
            if (idx) {
                s_map_context.poi_idx_file = idx;
                s_map_context.layer_poi = true;
            } else {
                s_map_context.layer_poi = false;
            }
        }
    }

    if (s_map_context.layer_poi && (app_config.layer_poi_labels != s_map_context.layer_poi_labels)) {
        if (!app_config.layer_poi_labels) {
            if (s_map_context.poi_db_file) { purrgo_fs_close(s_map_context.poi_db_file); s_map_context.poi_db_file = NULL; }
            s_map_context.layer_poi_labels = false;
        } else {
            purrgo_snprintf(path_buf, sizeof(path_buf), "%s/pois.db", map_dir);
            purrgo_file_t* db = purrgo_fs_open(path_buf, FS_READ);
            if (db) {
                s_map_context.poi_db_file = db;
                s_map_context.layer_poi_labels = true;
            } else {
                s_map_context.layer_poi_labels = false;
            }
        }
    }
}

void purrgo_map_shutdown(void)
{
    map_close_all_files();
    PURRGO_LOG("Map files closed\r\n");
}

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

static int get_target_lod(purrgo_map_scale_t scale, purrgo_map_details_t details)
{
    if (details == PURRGO_MAP_DETAILS_LOW) {
        if (scale <= PURRGO_MAP_SCALE_100M) {
            return 0;
        } else if (scale <= PURRGO_MAP_SCALE_2KM) {
            return 1;
        } else {
            return 2;
        }
    } else {
        if (scale <= PURRGO_MAP_SCALE_500M) {
            return 0;
        } else if (scale <= PURRGO_MAP_SCALE_5KM) {
            return 1;
        } else {
            return 2;
        }
    }
}

static bool map_parse_pgo_header(purrgo_fs_t *fs, pgo_header_info_t *info)
{
    uint8_t pgo_header[32];

    /*
     * Because map file handles are now persistent across renders,
     * we cannot assume they are at offset 0.
     * We must explicitly seek to 0 before reading the header.
     */
    if (!fs->seek(fs->handle, 0)) {
        return false;
    }

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

    purrgo_map_scale_t current_scale = map_app_get_map_zoom_level();
    int target_lod = get_target_lod(current_scale, app_config.map_details);

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

    /*
     * Sync map files context with current map directory and layer states.
     * This opens necessary files and closes unused ones.
     */
    map_sync_files(map_dir);

    /* ------------------- LANDUSE ------------------- */

    bool landuse_success = !app_config.layer_landuse;

    if (s_map_context.landuse_idx_file && s_map_context.landuse_mlp_file) {

        purrgo_fs_t landuse_idx_fs = {
            .handle = s_map_context.landuse_idx_file,
            .read = core_fs_read_wrapper,
            .seek = core_fs_seek_wrapper
        };

        purrgo_fs_t landuse_mlp_fs = {
            .handle = s_map_context.landuse_mlp_file,
            .read = core_fs_read_wrapper,
            .seek = core_fs_seek_wrapper
        };

        purrgo_fs_t landuse_db_fs = {
            .handle = s_map_context.landuse_db_file,
            .read = core_fs_read_wrapper,
            .seek = core_fs_seek_wrapper
        };

        gfx_set_color(gfx, 2, 3);

        purrgo_map_render_layer(
            &landuse_idx_fs,
            &landuse_mlp_fs,
            s_map_context.landuse_db_file ? &landuse_db_fs : NULL,
            gfx,
            camera,
            viewport,
            MAP_LAYER_POLYGONS
        );

        landuse_success = true;
    }

    /* ------------------- ROADS ------------------- */

    bool roads_success = !app_config.layer_roads;

    if (s_map_context.roads_idx_file && s_map_context.roads_mlp_file) {

        purrgo_fs_t idx_fs = {
            .handle = s_map_context.roads_idx_file,
            .read = core_fs_read_wrapper,
            .seek = core_fs_seek_wrapper
        };

        purrgo_fs_t mlp_fs = {
            .handle = s_map_context.roads_mlp_file,
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

    bool poi_success = true;

    if (s_map_context.poi_idx_file) {

        purrgo_fs_t poi_idx_fs = {
            .handle = s_map_context.poi_idx_file,
            .read = core_fs_read_wrapper,
            .seek = core_fs_seek_wrapper
        };

        purrgo_fs_t poi_db_fs = {
            .handle = s_map_context.poi_db_file,
            .read = core_fs_read_wrapper,
            .seek = core_fs_seek_wrapper
        };

        purrgo_map_render_layer(
            &poi_idx_fs,
            NULL,
            s_map_context.poi_db_file ? &poi_db_fs : NULL,
            gfx,
            camera,
            viewport,
            MAP_LAYER_POIS
        );
    }

    return landuse_success && roads_success && poi_success;
}