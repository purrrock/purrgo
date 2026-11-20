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
 * Подпись для карты из начала поля названия записи .db (раздел 10.6.1).
 * Вынесена для тестов; out - не меньше PURRGO_MAP_LABEL_MAX + 1 байт. Возвращает длину.
 */
size_t map_label_format(const uint8_t *field, size_t n, char *out);

#endif // PURRGO_MAP_IDX_H
