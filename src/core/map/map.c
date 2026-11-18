#include "purrgo/map.h"
#include "purrgo/logger.h"
#include "map_internal.h"
#include "map_idx.h"
#include "map_render.h"
#include "purrgo/fs_hal.h"
#include "purrgo/render_prof.h"

#include <stdbool.h>
#include <stddef.h>
#include "purrgo/purrgo_format.h"
#include <string.h>

/*
 * Таблица слоёв карты.
 *
 * Всё, что относится к слою, описано одной строкой k_layers: имена файлов, переключатели
 * в конфиге, правила открытия и параметры отрисовки. Порядок строк - порядок отрисовки
 * снизу вверх; открывается и закрывается всё одним циклом. Новый слой - новая строка
 * (плюс поля в purrgo_map_render_params_t) и строка в docs/architecture.md, раздел
 * "Layer drawing order". Состояние слоя (открытые файлы, флаги "файла нет") хранится в
 * s_map_context.layer[] под тем же индексом.
 */
typedef struct {
    const char* idx_name;        /* индекс: всегда есть */
    const char* mlp_name;        /* геометрия; NULL - слой без геометрии (POI) */
    const char* db_name;         /* подписи; NULL - слой без подписей */

    size_t enabled_off;          /* offsetof(params, <переключатель слоя>) */
    size_t labels_off;           /* offsetof(params, <переключатель подписей>); при db_name == NULL не используется */

    bool optional;               /* слоя может не быть в карте: отсутствие файлов не ошибка */
    bool labels_optional;        /* то же для файла подписей */

    purrgo_map_layer_t type;     /* как рисовать слой */
    int fg, bg;                  /* цвета gfx_set_color перед слоем; fg < 0 - не менять */
    bool after_labels;           /* рисовать после очереди подписей полигонов (поверх дорог) */
    bool (*want_draw)(const purrgo_map_render_params_t* params);   /* NULL - всегда */
} map_layer_desc_t;

#define PARAM_OFF(field) offsetof(purrgo_map_render_params_t, field)

/* При poi_mode == NO обход pois.idx ничего не рисует, но читает индекс с SD: слой пропускаем. */
static bool map_want_poi(const purrgo_map_render_params_t* params)
{
    return params->poi_mode != PURRGO_POI_MODE_NO;
}

static const map_layer_desc_t k_layers[] = {
    /* Landuse: полигоны; подписи уходят в очередь и рисуются поверх дорог. */
    { "landuse.idx", "landuse.mlp", "landuse.db",
      PARAM_OFF(layer_landuse), PARAM_OFF(layer_landuse_labels),
      false, false, MAP_LAYER_POLYGONS, 2, 3, false, NULL },

    /* Water: полигоны водоёмов; water.db нет, если в слое нет именованных объектов. */
    { "water.idx", "water.mlp", "water.db",
      PARAM_OFF(layer_water), PARAM_OFF(layer_water_labels),
      true, true, MAP_LAYER_POLYGONS, 0, 3, false, NULL },

    /* Waterways: линии рек и ручьёв, включаются переключателем water, подписей нет. */
    { "waterways.idx", "waterways.mlp", NULL,
      PARAM_OFF(layer_water), 0,
      true, false, MAP_LAYER_LINES, 0, 3, false, NULL },

    /* Roads: линии, подписей нет. */
    { "roads.idx", "roads.mlp", NULL,
      PARAM_OFF(layer_roads), 0,
      false, false, MAP_LAYER_LINES, 1, 3, false, NULL },

    /* POI: только индекс и подписи; цвета задаёт сама отрисовка маркеров. */
    { "pois.idx", NULL, "pois.db",
      PARAM_OFF(layer_poi), PARAM_OFF(layer_poi_labels),
      false, false, MAP_LAYER_POIS, -1, -1, true, map_want_poi },
};

#define MAP_LAYER_COUNT (sizeof k_layers / sizeof k_layers[0])

/*
 * Persistent map file context
 * We maintain these files open to preserve the LRU cache benefits across renders.
 */
typedef struct {
    purrgo_file_t* idx;
    purrgo_file_t* mlp;
    purrgo_file_t* db;

    /*
     * Файлы слоя не открылись: не пробуем снова на каждом кадре (каждая попытка -
     * обращение к SD и строка в UART). Флаги снимаются при смене каталога,
     * выключении слоя (подписей) в конфиге и purrgo_map_shutdown().
     * Слой открыт, пока открыт его idx (пара idx/mlp открывается целиком или никак).
     */
    bool missing;
    bool labels_missing;
} map_layer_state_t;

typedef struct {
    char current_map_dir[PURRGO_FS_MAX_PATH];
    map_layer_state_t layer[MAP_LAYER_COUNT];

    /* Путь map_dir не помещается в буфер (с запасом под имя файла слоя). */
    bool dir_invalid;
} map_context_t;

/* Самое длинное имя файла слоя - "landuse.idx" (11 символов) + '/' + '\0'. */
#define MAP_DIR_MAX_LEN (PURRGO_FS_MAX_PATH - 16)

static map_context_t s_map_context = {0};

static bool map_param_flag(const purrgo_map_render_params_t* params, size_t offset)
{
    return *(const bool*)((const char*)params + offset);
}

static void map_close_file(purrgo_file_t **file)
{
    if (*file != NULL) { purrgo_fs_close(*file); *file = NULL; }
}

/* Закрыть файлы слоя и забыть, что их не удалось открыть. */
static void map_layer_reset(map_layer_state_t* st)
{
    map_close_file(&st->idx);
    map_close_file(&st->mlp);
    map_close_file(&st->db);
    st->missing = false;
    st->labels_missing = false;
}

static void map_close_all_files(void)
{
    for (size_t i = 0; i < MAP_LAYER_COUNT; i++) {
        map_layer_reset(&s_map_context.layer[i]);
    }
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
    purrgo_file_t* file = purrgo_fs_open(path_buf, FS_READ);

    /* Индекс обходится то прыжками, то подряд: адаптивный read-ahead (см. map_internal.h). */
    const size_t name_len = strlen(name);
    if (file != NULL && name_len > 4u && strcmp(name + name_len - 4u, ".idx") == 0) {
        purrgo_fs_set_adaptive_read_ahead(file, (uint8_t)PURRGO_MAP_IDX_READ_AHEAD_MAX_SECTORS);
    }
    return file;
}

/*
 * Открыть пару файлов слоя; при неудаче не оставляет открытым ничего.
 * optional: слой может отсутствовать в карте (water, waterways) - об этом не
 * сообщаем как об ошибке.
 */
static bool map_open_pair(const char* map_dir, const char* name_a, const char* name_b,
                          purrgo_file_t** out_a, purrgo_file_t** out_b, bool optional)
{
    purrgo_file_t* a = map_open_file(map_dir, name_a);
    purrgo_file_t* b = map_open_file(map_dir, name_b);

    if (a && b) {
        /* b - всегда .mlp: включаем read-ahead (адаптивный или фиксированный, см. map_internal.h). */
#if PURRGO_MAP_MLP_ADAPTIVE_RA
        purrgo_fs_set_adaptive_read_ahead(b, (uint8_t)PURRGO_MAP_MLP_READ_AHEAD_SECTORS);
#else
        purrgo_fs_set_read_ahead(b, (uint8_t)PURRGO_MAP_MLP_READ_AHEAD_SECTORS);
#endif
        *out_a = a;
        *out_b = b;
        return true;
    }
    if (a) purrgo_fs_close(a);
    if (b) purrgo_fs_close(b);
    if (!optional) {
        PURRGO_LOGE("Map: %s or %s not found in %s\r\n", name_a, name_b, map_dir);
    }
    return false;
}

/* Открыть основные файлы слоя (пару idx/mlp или один idx). */
static void map_layer_open(const map_layer_desc_t* d, map_layer_state_t* st, const char* map_dir)
{
    bool ok;

    if (d->mlp_name != NULL) {
        ok = map_open_pair(map_dir, d->idx_name, d->mlp_name, &st->idx, &st->mlp, d->optional);
    } else {
        st->idx = map_open_file(map_dir, d->idx_name);
        ok = (st->idx != NULL);
        if (!ok && !d->optional) {
            PURRGO_LOGE("Map: %s not found in %s\r\n", d->idx_name, map_dir);
        }
    }
    st->missing = !ok;
}

/* Открыть файл подписей слоя. */
static void map_layer_open_labels(const map_layer_desc_t* d, map_layer_state_t* st, const char* map_dir)
{
    st->db = map_open_file(map_dir, d->db_name);
    if (st->db == NULL) {
        st->labels_missing = true;
        if (!d->labels_optional) {
            PURRGO_LOGE("Map: %s not found in %s\r\n", d->db_name, map_dir);
        }
    }
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
            PURRGO_LOGE("Map: map_dir is too long (max %d)\r\n", (int)MAP_DIR_MAX_LEN);
        }
        return false;
    }

    /* Длина проверена выше, поэтому сравнение и копирование не обрезают путь. */
    if (s_map_context.dir_invalid ||
        strcmp(s_map_context.current_map_dir, map_dir) != 0) {
        map_close_all_files();
        strcpy(s_map_context.current_map_dir, map_dir);
    }

    for (size_t i = 0; i < MAP_LAYER_COUNT; i++) {
        const map_layer_desc_t* d = &k_layers[i];
        map_layer_state_t* st = &s_map_context.layer[i];

        if (!map_param_flag(params, d->enabled_off)) {
            map_layer_reset(st);
        } else if (st->idx == NULL && !st->missing) {
            map_layer_open(d, st, map_dir);
        }

        if (d->db_name == NULL) {
            continue;
        }

        /* Подписи открываются только у открытого слоя. */
        if (!map_param_flag(params, d->labels_off)) {
            map_close_file(&st->db);
            st->labels_missing = false;
        } else if (st->idx != NULL && st->db == NULL && !st->labels_missing) {
            map_layer_open_labels(d, st, map_dir);
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

    map_ram_enter();

#if PURRGO_RENDER_PROF
    purrgo_render_prof_begin_frame();
#endif

    /*
     * Sync map files context with current map directory and layer states.
     * This opens necessary files and closes unused ones.
     */
    PURRGO_PROF_BEGIN(prof_sync);
    const bool sync_ok = map_sync_files(params);
    PURRGO_PROF_END(prof_sync, PURRGO_PROF_SYNC);

    if (!sync_ok) {
#if PURRGO_RENDER_PROF
        purrgo_render_prof_abort_frame();
#endif
        return false;
    }

    /*
     * Результат кадра: false, если хотя бы один открытый слой не отрисован
     * (повреждён заголовок/индекс, ошибка чтения SD). Слой, чьих файлов нет,
     * ошибкой не считается: повтор не поможет, а каждый повтор - полный refresh
     * e-ink. Об отсутствии файла один раз сообщает map_sync_files().
     */
    bool ok = true;

    /*
     * Слои сверху вниз в порядке k_layers. Слой, чьи файлы не открыты, пропускается.
     *
     * Перед слоями с after_labels (POI) выводятся отложенные подписи полигонов. К этому
     * моменту landuse, water и roads уже нарисованы, поэтому подписи ложатся поверх дорог.
     * map_render_draw_queued_labels() проверяет коллизии и резервирует BBox каждой
     * успешно размещённой подписи.
     */
    bool labels_drawn = false;

    for (size_t i = 0; i <= MAP_LAYER_COUNT; i++) {
        const bool end = (i == MAP_LAYER_COUNT);

        if (!labels_drawn && (end || k_layers[i].after_labels)) {
            PURRGO_PROF_BEGIN(prof_lbl);
            map_render_draw_queued_labels(gfx);
            PURRGO_PROF_END(prof_lbl, PURRGO_PROF_LABELS);
            labels_drawn = true;
        }
        if (end) {
            break;
        }

        const map_layer_desc_t* d = &k_layers[i];
        const map_layer_state_t* st = &s_map_context.layer[i];

        if (st->idx == NULL || (d->mlp_name != NULL && st->mlp == NULL)) {
            continue;
        }
        if (d->want_draw != NULL && !d->want_draw(params)) {
            continue;
        }
        if (d->fg >= 0) {
            gfx_set_color(gfx, (gfx_color_t)d->fg, (gfx_color_t)d->bg);
        }

        /* db у слоёв без подписей всегда NULL: файл не открывается. */
        ok = purrgo_map_render_layer(
            st->idx,
            st->mlp,
            st->db,
            gfx,
            camera,
            viewport,
            params,
            d->type
        ) && ok;
    }

#if PURRGO_RENDER_PROF
    purrgo_render_prof_end_frame((int)params->scale, ok ? 1 : 0);
#endif

    return ok;
}