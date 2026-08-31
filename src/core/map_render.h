#ifndef PURRGO_MAP_RENDER_H
#define PURRGO_MAP_RENDER_H

#include <stdbool.h>
#include <stdint.h>
#include "purrgo/map.h"
#include "purrgo/map_style.h"
#include "map_internal.h"

void map_render_feature(
    purrgo_fs_t *mlp_fs,
    uint32_t v1_offset,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    gfx_context_t *gfx,
    bool is_polygon_layer,
    purrgo_map_style_t style,
    map_diag_t *diag
);

// Очистка BBox-кэша меток и очереди отложенных меток.
// Вызывать один раз перед отрисовкой нового кадра.
void map_render_clear_labels(void);

// Проверка коллизий и резервирование места для новой метки.
bool map_render_try_place_label(int16_t x, int16_t y, uint16_t w, uint16_t h);

// Добавить подпись в очередь отложенной отрисовки.
bool map_render_queue_label(
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    const char *text
);

// Отрисовать все ранее поставленные в очередь подписи.
void map_render_draw_queued_labels(gfx_context_t *gfx);

#endif // PURRGO_MAP_RENDER_H