#include "purrgo/map.h"
#include "purrgo/logger.h"
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

    /*
     * Файл слоя не открылся: не пробуем снова на каждом кадре (каждая попытка -
     * обращение к SD и строка в UART). Флаг снимается при смене каталога,
     * выключении слоя в конфиге и purrgo_map_shutdown().
     */
    bool landuse_missing;
    bool landuse_labels_missing;
    bool roads_missing;
    bool poi_missing;
    bool poi_labels_missing;

    /* Путь map_dir не помещается в буфер (с запасом под имя файла слоя). */
    bool dir_invalid;
} map_context_t;

/* Самое длинное имя файла слоя - "landuse.idx" (11 символов) + '/' + '\0'. */
#define MAP_DIR_MAX_LEN (PURRGO_FS_MAX_PATH - 16)

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

    s_map_context.landuse_missing = false;
    s_map_context.landuse_labels_missing = false;
    s_map_context.roads_missing = false;
    s_map_context.poi_missing = false;
    s_map_context.poi_labels_missing = false;
    s_map_context.dir_invalid = false;

    s_map_context.current_map_dir[0] = '\0';
}

/* Открыть <map_dir>/<name>. NULL - файла нет или путь не поместился в буфер. */
static purrgo_file_t* map_open_file(const char* map_dir, const char* name)
{
    char path_buf[PURRGO_FS_MAX_PATH];
    int n = purrgo_snprintf(path_buf, sizeof(path_buf), "%s/%s", map_dir, name);

    if (n < 0 || (size_t)n >= sizeof(path_buf)) {
        return NULL;
    }
    return purrgo_fs_open(path_buf, FS_READ);
}

/* Открыть пару файлов слоя; при неудаче не оставляет открытым ничего. */
static bool map_open_pair(const char* map_dir, const char* name_a, const char* name_b,
                          purrgo_file_t** out_a, purrgo_file_t** out_b)
{
    purrgo_file_t* a = map_open_file(map_dir, name_a);
    purrgo_file_t* b = map_open_file(map_dir, name_b);

    if (a && b) {
        *out_a = a;
        *out_b = b;
        return true;
    }
    if (a) purrgo_fs_close(a);
    if (b) purrgo_fs_close(b);
    PURRGO_LOG("Map: %s or %s not found in %s\r\n", name_a, name_b, map_dir);
    return false;
}

/*
 * Приводит набор открытых файлов в соответствие с каталогом и конфигом.
 * Возвращает false, если путь к каталогу слишком длинный (карта не рисуется).
 */
static bool map_sync_files(const purrgo_map_render_params_t *params)
{
    const char* map_dir = params->map_dir;

    if (strlen(map_dir) > MAP_DIR_MAX_LEN) {
        if (!s_map_context.dir_invalid) {
            map_close_all_files();
            s_map_context.dir_invalid = true;
            PURRGO_LOG("Map: map_dir is too long (max %d)\r\n", (int)MAP_DIR_MAX_LEN);
        }
        return false;
    }

    /* Длина проверена выше, поэтому сравнение и копирование не обрезают путь. */
    if (s_map_context.dir_invalid ||
        strcmp(s_map_context.current_map_dir, map_dir) != 0) {
        map_close_all_files();
        strcpy(s_map_context.current_map_dir, map_dir);
    }

    /* Landuse */
    if (!params->layer_landuse) {
        map_close_file(&s_map_context.landuse_idx_file);
        map_close_file(&s_map_context.landuse_mlp_file);
        map_close_file(&s_map_context.landuse_db_file);
        s_map_context.layer_landuse = false;
        s_map_context.layer_landuse_labels = false;
        s_map_context.landuse_missing = false;
        s_map_context.landuse_labels_missing = false;
    } else if (!s_map_context.layer_landuse && !s_map_context.landuse_missing) {
        if (map_open_pair(map_dir, "landuse.idx", "landuse.mlp",
                          &s_map_context.landuse_idx_file,
                          &s_map_context.landuse_mlp_file)) {
            s_map_context.layer_landuse = true;
        } else {
            s_map_context.landuse_missing = true;
        }
    }

    if (!params->layer_landuse_labels) {
        map_close_file(&s_map_context.landuse_db_file);
        s_map_context.layer_landuse_labels = false;
        s_map_context.landuse_labels_missing = false;
    } else if (s_map_context.layer_landuse &&
               !s_map_context.layer_landuse_labels &&
               !s_map_context.landuse_labels_missing) {
        s_map_context.landuse_db_file = map_open_file(map_dir, "landuse.db");
        if (s_map_context.landuse_db_file) {
            s_map_context.layer_landuse_labels = true;
        } else {
            s_map_context.landuse_labels_missing = true;
            PURRGO_LOG("Map: landuse.db not found in %s\r\n", map_dir);
        }
    }

    /* Roads */
    if (!params->layer_roads) {
        map_close_file(&s_map_context.roads_idx_file);
        map_close_file(&s_map_context.roads_mlp_file);
        s_map_context.layer_roads = false;
        s_map_context.roads_missing = false;
    } else if (!s_map_context.layer_roads && !s_map_context.roads_missing) {
        if (map_open_pair(map_dir, "roads.idx", "roads.mlp",
                          &s_map_context.roads_idx_file,
                          &s_map_context.roads_mlp_file)) {
            s_map_context.layer_roads = true;
        } else {
            s_map_context.roads_missing = true;
        }
    }

    /* POIs */
    if (!params->layer_poi) {
        map_close_file(&s_map_context.poi_idx_file);
        map_close_file(&s_map_context.poi_db_file);
        s_map_context.layer_poi = false;
        s_map_context.layer_poi_labels = false;
        s_map_context.poi_missing = false;
        s_map_context.poi_labels_missing = false;
    } else if (!s_map_context.layer_poi && !s_map_context.poi_missing) {
        s_map_context.poi_idx_file = map_open_file(map_dir, "pois.idx");
        if (s_map_context.poi_idx_file) {
            s_map_context.layer_poi = true;
        } else {
            s_map_context.poi_missing = true;
            PURRGO_LOG("Map: pois.idx not found in %s\r\n", map_dir);
        }
    }

    if (!params->layer_poi_labels) {
        map_close_file(&s_map_context.poi_db_file);
        s_map_context.layer_poi_labels = false;
        s_map_context.poi_labels_missing = false;
    } else if (s_map_context.layer_poi &&
               !s_map_context.layer_poi_labels &&
               !s_map_context.poi_labels_missing) {
        s_map_context.poi_db_file = map_open_file(map_dir, "pois.db");
        if (s_map_context.poi_db_file) {
            s_map_context.layer_poi_labels = true;
        } else {
            s_map_context.poi_labels_missing = true;
            PURRGO_LOG("Map: pois.db not found in %s\r\n", map_dir);
        }
    }

    return true;
}

void purrgo_map_shutdown(void)
{
    map_close_all_files();
    PURRGO_LOG("Map files closed\r\n");
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

static bool map_parse_pgo_header(purrgo_file_t *file, pgo_header_info_t *info)
{
    uint8_t pgo_header[32];

    /*
     * Because map file handles are now persistent across renders,
     * we cannot assume they are at offset 0.
     * We must explicitly seek to 0 before reading the header.
     */
    if (!purrgo_fs_seek(file, 0)) {
        return false;
    }

    if (purrgo_fs_read(file, pgo_header, sizeof(pgo_header)) != sizeof(pgo_header)) {
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

bool purrgo_map_render_layer(
    purrgo_file_t *idx_file,
    purrgo_file_t *mlp_file,
    purrgo_file_t *db_file,
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *viewport,
    const purrgo_map_render_params_t *params,
    purrgo_map_layer_t layer_type
)
{
    if (idx_file == NULL ||
        params == NULL ||
        gfx == NULL ||
        camera == NULL ||
        viewport == NULL) {
        return false;
    }

    uint32_t current_idx_offset = 0;

    pgo_header_info_t idx_header = {0};

    if (!map_parse_pgo_header(idx_file, &idx_header)) {
        return false;
    }

    if (idx_header.file_type != 1) {
        return false;
    }

    if (layer_type != MAP_LAYER_POIS) {
        if (mlp_file == NULL) {
            return false;
        }

        pgo_header_info_t mlp_header = {0};

        if (!map_parse_pgo_header(mlp_file, &mlp_header)) {
            return false;
        }

        if (mlp_header.file_type != 2) {
            return false;
        }
    }

    int target_lod = get_target_lod(params->scale, params->details);

    uint32_t target_lod_offset = idx_header.lod_offset[target_lod];
    uint32_t lod_end = 0;

    if (target_lod == 0) {
        lod_end = idx_header.lod_offset[1];
    } else if (target_lod == 1) {
        lod_end = idx_header.lod_offset[2];
    } else {
        lod_end = 32 + idx_header.payload_size;
    }

    if (!purrgo_fs_seek(idx_file, target_lod_offset)) {
        return false;
    }

    current_idx_offset = target_lod_offset;

    if (current_idx_offset + 16 > lod_end) {
        return false;
    }

    uint8_t sqt_header[16];

    if (purrgo_fs_read(idx_file, sqt_header, sizeof(sqt_header)) != sizeof(sqt_header)) {
        return false;
    }

    current_idx_offset += 16;

    if (sqt_header[0] != 'S' ||
        sqt_header[1] != 'Q' ||
        sqt_header[2] != 'T' ||
        sqt_header[3] != 0x01) {
        return false;
    }

    uint32_t mode = unpack_u32_le(&sqt_header[8]);
    uint32_t count = unpack_u32_le(&sqt_header[12]);

    if (count > 0) {
        bool is_nav = (mode > 0);

        for (uint32_t i = 0; i < count; i++) {
            if (!map_idx_parse_node(
                    idx_file,
                    &current_idx_offset,
                    mlp_file,
                    db_file,
                    is_nav,
                    camera,
                    viewport,
                    gfx,
                    layer_type,
                    params,
                    lod_end)) {
                return false;
            }
        }
    }

    return true;
}

bool purrgo_map_render_viewport(
    gfx_context_t *gfx,
    const purrgo_viewport_t *viewport,
    const purrgo_bbox_t *camera,
    const purrgo_map_render_params_t *params
)
{
    if (params == NULL || params->map_dir == NULL) {
        return false;
    }

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
    if (!map_sync_files(params)) {
        return false;
    }

    /*
     * Результат кадра: false, если хотя бы один открытый слой не отрисован
     * (повреждён заголовок/индекс, ошибка чтения SD). Слой, чьих файлов нет,
     * ошибкой не считается: повтор не поможет, а каждый повтор - полный refresh
     * e-ink. Об отсутствии файла один раз сообщает map_sync_files().
     */
    bool ok = true;

    /* ------------------- LANDUSE ------------------- */

    if (s_map_context.landuse_idx_file && s_map_context.landuse_mlp_file) {

        gfx_set_color(gfx, 2, 3);

        ok = purrgo_map_render_layer(
            s_map_context.landuse_idx_file,
            s_map_context.landuse_mlp_file,
            s_map_context.landuse_db_file,
            gfx,
            camera,
            viewport,
            params,
            MAP_LAYER_POLYGONS
        ) && ok;
    }

    /* ------------------- ROADS ------------------- */

    if (s_map_context.roads_idx_file && s_map_context.roads_mlp_file) {

        gfx_set_color(gfx, 1, 3);

        /*
         * Дороги не подписываем —
         * передаём NULL вместо db_file.
         */
        ok = purrgo_map_render_layer(
            s_map_context.roads_idx_file,
            s_map_context.roads_mlp_file,
            NULL,
            gfx,
            camera,
            viewport,
            params,
            MAP_LAYER_LINES
        ) && ok;
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

    if (s_map_context.poi_idx_file) {

        ok = purrgo_map_render_layer(
            s_map_context.poi_idx_file,
            NULL,
            s_map_context.poi_db_file,
            gfx,
            camera,
            viewport,
            params,
            MAP_LAYER_POIS
        ) && ok;
    }

    return ok;
}