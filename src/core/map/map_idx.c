#include "map_idx.h"
#include "map_culling.h"
#include "map_render.h"
#include "purrgo/logger.h"
#include "purrgo/render_prof.h"
#include "purrgo/map_style.h"
#include "purrgo/gfx_circle.h"
#include <stddef.h>
#include <string.h>
#include "purrgo/gfx_text.h"
#include "map_projection.h"
#include "purrgo/purrgo_poi_icons.h"
#include "purrgo/gfx_icon.h"

/*
 * Обход индекса карты PurrGO V4 (docs/map-format-v4.md, разделы 5-7).
 *
 * Дерево лежит блоками по 512 байт: все дети навигационного узла - один блок, адрес которого
 * (child_block) абсолютен. Блок читается одним запросом, после чего bbox всех его записей
 * уже в ОЗУ; поддерево вне камеры пропускается тем, что его блок не читается.
 */

/*
 * Радиусы POI.
 */
#define PURRGO_POI_BIG_RADIUS   6
#define PURRGO_POI_SMALL_RADIUS 3

/*
 * Буфер читаемого блока. Отрисовка однопоточная; блок нужен только пока разбирается его
 * содержимое: перед спуском в поддерево навигационный блок сворачивается в короткий список
 * ссылок на кадре стека, поэтому буфер на все уровни дерева один.
 */
static uint8_t s_blk[PURRGO_MAP_BLOCK_SIZE];

/* Буфер подписи (общий: отрисовка однопоточная). 22 символа + завершающий ноль. */
static char name_buf[PURRGO_MAP_LABEL_MAX + 1u];

/* Контекст обхода дерева одной секции LOD (общий для отрисовки и поиска). */
typedef struct {
    purrgo_file_t *idx;
    const map_section_t *sec;
    const purrgo_bbox_t *cam;
    map_idx_leaf_fn leaf;      /* вызывается для каждого узла данных листового блока */
    void *leaf_ctx;
} map_tree_t;

/* Контекст отрисовки: листовой обработчик map_idx_draw_leaf() получает его как leaf_ctx. */
typedef struct {
    purrgo_file_t *mlp;
    purrgo_file_t *db;
    const purrgo_bbox_t *cam;
    const purrgo_viewport_t *vp;
    gfx_context_t *gfx;
    purrgo_map_layer_t layer_type;
    const purrgo_map_render_params_t *params;
} map_draw_t;

/*
 * Подпись для карты из поля названия записи .db (раздел 10.6.1): до 22 символов; длиннее -
 * первые 20 без хвостовых "_" и "..", как делал компилятор V3. "_" при выводе - пробел.
 * field - прочитанное начало поля (не больше PURRGO_MAP_LABEL_MAX + 1 байт: лишний байт нужен,
 * чтобы отличить название из 22 символов от более длинного), n - сколько байт прочитано.
 * Возвращает длину подписи; out - строка с завершающим нулём, размер не меньше
 * PURRGO_MAP_LABEL_MAX + 1.
 */
size_t map_label_format(const uint8_t *field, size_t n, char *out)
{
    size_t len = 0u;
    while (len < n && field[len] != 0u) {
        len++;
    }

    size_t keep = len;
    bool cut = false;
    if (len > PURRGO_MAP_LABEL_MAX) {
        keep = PURRGO_MAP_LABEL_CUT;
        while (keep > 0u && field[keep - 1u] == (uint8_t)'_') {
            keep--;
        }
        cut = true;
    }

    for (size_t i = 0; i < keep; i++) {
        out[i] = (field[i] == (uint8_t)'_') ? ' ' : (char)field[i];
    }
    if (cut) {
        out[keep++] = '.';
        out[keep++] = '.';
    }
    out[keep] = '\0';
    return keep;
}

/*
 * Чтение подписи из .db файла.
 * Функция учитывает различие между слоями: в обычных слоях v2=1 это dummy-запись,
 * а в слое POI dummy-записи нет (раздел 10); v2 = 0 - записи нет.
 */
static bool map_db_read_name_impl(purrgo_file_t *db_file, uint32_t v2, bool is_poi, char *out_buffer) {
    if (!db_file || v2 == 0 || out_buffer == NULL) return false;

    // В стандартных слоях v2=1 зарезервировано под dummy record, имена начинаются с 2
    if (!is_poi && v2 == 1) return false;

    // Смещение (раздел 10): 32 (PGO Header) + 129 (DBF Header) + Index * 117 (запись) + 17 (поле названия)
    uint64_t name_offset = 32u + 129u + (uint64_t)(v2 - 1u) * 117u + 17u;
    if (name_offset > UINT32_MAX) return false;

    if (!purrgo_fs_seek(db_file, (uint32_t)name_offset)) {
        return false;
    }

    /*
     * Поле названия - 100 байт, но на карте помещается не больше 22 символов. Читаем 23 байта:
     * лишний нужен, чтобы отличить название ровно из 22 символов от более длинного.
     */
    uint8_t raw_name[PURRGO_MAP_LABEL_MAX + 1u];
    uint32_t bytes_read = purrgo_fs_read(db_file, raw_name, (uint32_t)sizeof(raw_name));
    if (bytes_read == 0 || bytes_read > sizeof(raw_name)) return false;

    return map_label_format(raw_name, bytes_read, out_buffer) > 0u;
}

/* Обёртка для профилировки: время одного чтения имени (seek + read) из .db. */
static bool map_db_read_name(purrgo_file_t *db_file, uint32_t v2, bool is_poi, char *out_buffer) {
    PURRGO_PROF_BEGIN(prof_name);
    const bool ok = map_db_read_name_impl(db_file, v2, is_poi, out_buffer);
    PURRGO_PROF_END(prof_name, PURRGO_PROF_NAME);
    return ok;
}

bool map_label_read(purrgo_file_t *db_file, uint32_t v2, bool is_poi, char *out)
{
    return map_db_read_name(db_file, v2, is_poi, out);
}

/*
 * Узел данных (25 байт, раздел 8): bbox, тип, v1 (геометрия в .mlp), v2 (запись .db).
 * Проверяет пересечение с камерой и рисует объект.
 */
static void map_idx_draw_data_node(const map_draw_t *w, const uint8_t *n)
{
    purrgo_file_t *mlp_file = w->mlp;
    purrgo_file_t *db_file = w->db;
    const purrgo_bbox_t *cam = w->cam;
    const purrgo_viewport_t *vp = w->vp;
    gfx_context_t *gfx = w->gfx;
    const purrgo_map_layer_t layer_type = w->layer_type;
    const purrgo_map_render_params_t *params = w->params;

    int32_t ymin = unpack_i32_le(&n[4]);
    int32_t ymax = unpack_i32_le(&n[12]);

    bool passes = false;
    int32_t xmin = 0;
    int32_t xmax = 0;

    /*
     * Сначала выполняем дешёвую проверку по Y.
     */
    if (!(ymax < cam->min_y || ymin > cam->max_y)) {
        xmin = unpack_i32_le(&n[0]);
        xmax = unpack_i32_le(&n[8]);
        passes = bbox_intersects_camera(xmin, ymin, xmax, ymax, cam);
    }

    if (!passes) {
        return;
    }

    uint8_t obj_type = n[16];
    uint32_t v1 = unpack_u32_le(&n[17]);
    uint32_t v2 = unpack_u32_le(&n[21]); // Индекс в .db файле


    /*
     * ============================================================
     * POI
     * ============================================================
     */
    if (layer_type == MAP_LAYER_POIS) {
        if (params->poi_mode == PURRGO_POI_MODE_NO) return;

        int16_t sx, sy;

        {
            PURRGO_PROF_BEGIN(prof_proj);
            project_to_screen(xmin, ymin, cam, vp, &sx, &sy);
            PURRGO_PROF_END(prof_proj, PURRGO_PROF_PROJ);
        }

        int16_t radius = 0;
        if (params->poi_mode == PURRGO_POI_MODE_ICONS) {
            radius = PURRGO_POI_ICON_WIDTH / 2;
            gfx_draw_icon_11x11(gfx, sx, sy, purrgo_poi_icons[obj_type]);
        } else if (params->poi_mode == PURRGO_POI_MODE_CIRCLES) {
            if (obj_type == PURRGO_FEATURE_POI_BIG) {
                radius = PURRGO_POI_BIG_RADIUS;
                gfx_draw_poi_circle(gfx, sx, sy, radius);
            } else if (obj_type > PURRGO_FEATURE_POI_BIG) {
                radius = PURRGO_POI_SMALL_RADIUS;
                gfx_draw_poi_circle(gfx, sx, sy, radius);
            }
        }

        /*
         * Подписи POI отключены в режиме POI LABELS OFF.
         * Сам маркер POI при этом продолжает отображаться.
         */
        if (params->layer_poi_labels &&
            v2 > 0 &&
            db_file != NULL) {
            if (map_db_read_name(db_file, v2, true, name_buf)) {
                int len = (int)strlen(name_buf);

                int16_t text_w = (int16_t)(len * 6); // Ширина: 5px символ + 1px промежуток
                int16_t text_h = 8;       // Высота: 8px

                if (obj_type == PURRGO_FEATURE_POI_BIG) {
                    // Для POI_BIG: центрируем текст строго под маркером
                    int16_t text_x = sx - (text_w / 2);
                    int16_t text_y = (int16_t)(sy + radius + 2);

                    if (map_render_try_place_label(text_x, text_y, (uint16_t)text_w, (uint16_t)text_h)) {
                        gfx_draw_string_halo(gfx, text_x, text_y, name_buf);
                    }
                } else {
                    // Для POI_SMALL: Основной вариант размещения справа
                    int16_t text_x = (int16_t)(sx + radius + 2);
                    int16_t text_y = sy - (text_h / 2);

                    if (map_render_try_place_label(text_x, text_y, (uint16_t)text_w, (uint16_t)text_h)) {
                        gfx_draw_string_halo(gfx, text_x, text_y, name_buf);
                    } else {
                        // Коллизия справа: Запасной вариант "правее-ниже"
                        text_y = (int16_t)(sy + radius + 2);
                        if (map_render_try_place_label(text_x, text_y, (uint16_t)text_w, (uint16_t)text_h)) {
                            gfx_draw_string_halo(gfx, text_x, text_y, name_buf);
                        }
                    }
                }
            }
        }

        return;
    }


    /*
     * ============================================================
     * LINE / POLYGON
     * ============================================================
     */

		 purrgo_map_style_t style =
purrgo_map_style_from_feature((uint32_t)obj_type);

if (style == PURRGO_STYLE_NONE) {
return;
}

    if (v1 > 0) {
        bool is_polygon = (layer_type == MAP_LAYER_POLYGONS);

        /*
         * Слишком мелкий для экрана объект не рисуем и, главное, не читаем
         * его геометрию с SD (подпись такого полигона тоже не нужна).
         */
        if (map_feature_too_small(xmin, ymin, xmax, ymax, cam, vp,
                                  is_polygon ? PURRGO_MAP_MIN_PX_POLYGON
                                             : PURRGO_MAP_MIN_PX_LINE)) {
            return;
        }

        map_render_feature(mlp_file, v1, cam, vp, gfx, is_polygon, style);

        /*
         * Подписываем только площади (Landuse/Water).
         * Игнорируем дороги ради производительности и чистоты.
         *
         * При POI LABELS OFF подписи полигонов также
         * полностью отключены. Геометрия полигона при этом
         * продолжает отрисовываться.
         */
        if (is_polygon &&
            params->layer_poi_labels &&
            v2 >= 2 &&
            db_file != NULL) {

            if (map_db_read_name(db_file, v2, false, name_buf)) {
                /*
                 * Вычисляем центр BBox, разделяя слагаемые
                 * для защиты от int32 overflow.
                 */
                int32_t center_x = (xmin / 2) + (xmax / 2);
                int32_t center_y = (ymin / 2) + (ymax / 2);

                int16_t sx, sy;
                {
                    PURRGO_PROF_BEGIN(prof_proj);
                    project_to_screen(center_x, center_y, cam, vp, &sx, &sy);
                    PURRGO_PROF_END(prof_proj, PURRGO_PROF_PROJ);
                }

                int len = (int)strlen(name_buf);

                int16_t text_w = (int16_t)(len * 6);
                int16_t text_h = 8;

                /*
                 * Центрируем текст относительно вычисленного
                 * центра полигона.
                 */
                int16_t text_x = sx - (text_w / 2);
                int16_t text_y = sy - (text_h / 2);

                /*
                 * Не рисуем подпись сейчас.
                 *
                 * Landuse geometry уже будет нарисована, затем
                 * roads будут нарисованы поверх неё. После этого
                 * map.c вызовет map_render_draw_queued_labels().
                 */
                map_render_queue_label(
                    text_x,
                    text_y,
                    (uint16_t)text_w,
                    (uint16_t)text_h,
                    name_buf
                );
            }
        }
    }
}

/* Листовой обработчик отрисовки: узел данных -> объект на экране. */
static void map_idx_draw_leaf(void *ctx, const uint8_t *node)
{
    map_idx_draw_data_node((const map_draw_t *)ctx, node);
}

/* ------------------------------------------------------------------------ обход --- */

/* Ссылка на блок детей, прошедшая отсечение по камере. */
typedef struct {
    uint32_t off;      /* смещение блока в файле */
    uint8_t  v1;       /* уровень узла: 0 - блок узлов данных, иначе навигационный блок уровня v1 - 1 */
    uint8_t  count;    /* число записей в блоке (v2) */
} map_blk_ref_t;

/*
 * Обход блока.
 *   off    - смещение блока (кратно 512, внутри секции);
 *   level  - уровень записей навигационного блока; -1 - листовой блок (узлы данных);
 *   count  - число записей;
 *   depth  - сколько блоков уже пройдено от корня (для предела глубины).
 * false - ошибка чтения или нарушение раздела 7.1: узел отвергается целиком, обход прекращается.
 */
static bool map_idx_walk_block(const map_tree_t *w, uint32_t off, int32_t level, uint32_t count,
                               uint32_t depth)
{
    const map_section_t *sec = w->sec;
    const purrgo_bbox_t *cam = w->cam;

    /* Корень - блок 0, листовой блок дерева глубины D - блок D: допустимо до MAX включительно. */
    if (depth > PURRGO_MAP_MAX_TREE_DEPTH) {
        PURRGO_LOGE("MAP: ERROR tree depth limit exceeded\n\r");
        return false;
    }

    if (!purrgo_fs_seek(w->idx, off) ||
        purrgo_fs_read(w->idx, s_blk, PURRGO_MAP_BLOCK_SIZE) != PURRGO_MAP_BLOCK_SIZE) {
        return false;
    }

    /* Листовой блок: узлы данных идут подряд с нулевого байта. */
    if (level < 0) {
        for (uint32_t i = 0; i < count; i++) {
            w->leaf(w->leaf_ctx, &s_blk[i * PURRGO_MAP_DATA_NODE_SIZE]);
        }
        return true;
    }

    /*
     * Навигационный блок. Все узлы проверяются (дёшево, из ОЗУ), по bbox отбираются те,
     * чьи блоки детей нужно читать. Сам буфер при спуске переиспользуется, поэтому прошедшие
     * отсечение узлы копируются в список на стеке (не более 18 записей).
     */
    map_blk_ref_t refs[PURRGO_MAP_NAV_FANOUT];
    uint32_t nrefs = 0u;
    const uint32_t child_limit = sec->start + sec->size - PURRGO_MAP_BLOCK_SIZE;   /* последний блок секции */

    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *nn = &s_blk[i * PURRGO_MAP_NAV_NODE_SIZE];
        const uint32_t child_block = unpack_u32_le(&nn[0]);
        const uint32_t v1 = unpack_u32_le(&nn[20]);
        const uint32_t v2 = unpack_u32_le(&nn[24]);

        /* Раздел 7.1 и 6.3. child_block > off делает дерево ациклическим. */
        const uint32_t max_children = (v1 == 0u) ? sec->leaf_fanout : sec->nav_fanout;
        if ((child_block % PURRGO_MAP_BLOCK_SIZE) != 0u ||
            child_block < sec->start + 2u * PURRGO_MAP_BLOCK_SIZE ||
            child_block > child_limit ||
            child_block <= off ||
            v1 != (uint32_t)level ||
            v2 < 1u || v2 > max_children) {
            PURRGO_LOGE("MAP: ERROR bad index node\n\r");
            return false;
        }

        const int32_t c_ymin = unpack_i32_le(&nn[8]);
        const int32_t c_ymax = unpack_i32_le(&nn[16]);
        if (c_ymax < cam->min_y || c_ymin > cam->max_y) {
            continue;
        }
        if (!bbox_intersects_camera(unpack_i32_le(&nn[4]), c_ymin,
                                    unpack_i32_le(&nn[12]), c_ymax, cam)) {
            continue;
        }

        refs[nrefs].off = child_block;
        refs[nrefs].v1 = (uint8_t)v1;
        refs[nrefs].count = (uint8_t)v2;
        nrefs++;
    }

    for (uint32_t i = 0; i < nrefs; i++) {
        /* v1 == 0: блок детей листовой; иначе навигационный с узлами уровня v1 - 1. */
        const int32_t child_level = (refs[i].v1 == 0u) ? -1 : (int32_t)refs[i].v1 - 1;
        if (!map_idx_walk_block(w, refs[i].off, child_level, refs[i].count, depth + 1u)) {
            return false;
        }
    }
    return true;
}

bool map_idx_visit_section(
    purrgo_file_t *idx_file,
    const map_section_t *section,
    const purrgo_bbox_t *cam,
    map_idx_leaf_fn leaf,
    void *leaf_ctx
)
{
    if (idx_file == NULL || section == NULL || cam == NULL || leaf == NULL) {
        return false;
    }
    if (section->depth == 0u) {
        return true;   /* пустой LOD: корневого блока нет */
    }

    const map_tree_t w = { idx_file, section, cam, leaf, leaf_ctx };

    /* Корень: навигационный блок с root_count узлами уровня depth - 1 (раздел 5.3). */
    return map_idx_walk_block(&w, section->start + PURRGO_MAP_BLOCK_SIZE,
                              (int32_t)section->depth - 1, section->root_count, 0u);
}

bool map_idx_walk_section(
    purrgo_file_t *idx_file,
    purrgo_file_t *mlp_file,
    purrgo_file_t *db_file,
    const map_section_t *section,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    gfx_context_t *gfx,
    purrgo_map_layer_t layer_type,
    const purrgo_map_render_params_t *params
)
{
    map_draw_t d = { mlp_file, db_file, cam, vp, gfx, layer_type, params };

    return map_idx_visit_section(idx_file, section, cam, map_idx_draw_leaf, &d);
}
