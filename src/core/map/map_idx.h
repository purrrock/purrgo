#ifndef PURRGO_MAP_IDX_H
#define PURRGO_MAP_IDX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "purrgo/map.h"
#include "map_internal.h"

/*
 * Обойти секцию LOD индекса V4 и нарисовать объекты, попавшие в камеру.
 * section - уже проверенный заголовок секции (map.c). false - ошибка чтения SD или
 * нарушение формата (раздел 7.1); уже нарисованное остаётся на экране.
 */
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
);

/*
 * Обработчик узла данных листового блока: node - 25 байт (раздел 8): bbox (xmin, ymin, xmax, ymax,
 * по 4 байта little-endian), Type, v1, v2. Вызывается для каждого узла листа, попавшего в блок,
 * который пересёк камеру; точную проверку по bbox узла обработчик делает сам. Указатель действителен
 * только на время вызова. Обработчик не должен читать тот же файл индекса.
 */
typedef void (*map_idx_leaf_fn)(void *ctx, const uint8_t *node);

/*
 * Обойти секцию LOD индекса V4 и вызвать leaf для узлов данных листовых блоков, лежащих в
 * поддеревьях, чей bbox пересекает cam. Блоки вне камеры не читаются. Тот же обход использует
 * отрисовка (map_idx_walk_section), поиск POI (poi_search.c) - единственная проверка раздела 7.1.
 * false - ошибка чтения SD или нарушение формата; уже вызванные обработчики остаются в силе.
 */
bool map_idx_visit_section(
    purrgo_file_t *idx_file,
    const map_section_t *section,
    const purrgo_bbox_t *cam,
    map_idx_leaf_fn leaf,
    void *leaf_ctx
);

/*
 * Разобрать заголовок PGO индекса и сектор SQT секции LOD lod (0..PURRGO_MAP_LOD_COUNT - 1).
 * false - не PGO-индекс, версия не V4, нарушение раздела 1.5 или 5.
 */
bool map_idx_open_lod(purrgo_file_t *idx_file, uint32_t lod, map_section_t *section);

/*
 * Подпись для карты из начала поля названия записи .db (раздел 10.6.1).
 * Вынесена для тестов; out - не меньше PURRGO_MAP_LABEL_MAX + 1 байт. Возвращает длину.
 */
size_t map_label_format(const uint8_t *field, size_t n, char *out);

/*
 * Прочитать подпись записи v2 из .db (раздел 10): тот же путь, что у подписей на карте
 * (PurrGO-256, "_" -> пробел, не больше 22 символов, длиннее - ".."). is_poi - слой POI
 * (в нём записи нумеруются с 1 и dummy-записи нет). out - не меньше PURRGO_MAP_LABEL_MAX + 1 байт.
 * false - записи нет, имя пустое или ошибка чтения.
 */
bool map_label_read(purrgo_file_t *db_file, uint32_t v2, bool is_poi, char *out);

#endif // PURRGO_MAP_IDX_H
