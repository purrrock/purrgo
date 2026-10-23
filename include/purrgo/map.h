// file: include/purrgo/map.h
#ifndef PURRGO_MAP_H
#define PURRGO_MAP_H

#include <stdint.h>
#include <stdbool.h>

#include "purrgo/gfx_renderer.h"
#include "purrgo/fs_hal.h"
#include "purrgo/map_types.h"

typedef struct {
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;
} purrgo_bbox_t;

typedef struct {
    uint16_t width;
    uint16_t height;

    int16_t offset_x;
    int16_t offset_y;
} purrgo_viewport_t;

typedef enum {
    MAP_LAYER_LINES = 0,
    MAP_LAYER_POLYGONS,
    MAP_LAYER_POIS
} purrgo_map_layer_t;

/*
 * Отрисовать один слой. idx - индекс слоя (обязателен), mlp - геометрия
 * (обязательна для линий и полигонов), db - подписи (может быть NULL).
 * Файлы открыты вызывающим на чтение; позиция в них не важна.
 * true - слой отрисован целиком; false - ошибка заголовков, чтения или разбора.
 */
bool purrgo_map_render_layer(
    purrgo_file_t *idx_file,
    purrgo_file_t *mlp_file,
    purrgo_file_t *db_file,
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *viewport,
    const purrgo_map_render_params_t *params,
    purrgo_map_layer_t layer_type
);

/* Отрисовать карту каталога params->map_dir со слоями и масштабом из params. */
bool purrgo_map_render_viewport(
    gfx_context_t *gfx,
    const purrgo_viewport_t *viewport,
    const purrgo_bbox_t *camera,
    const purrgo_map_render_params_t *params
);

void purrgo_map_shutdown(void);

#endif /* PURRGO_MAP_H */