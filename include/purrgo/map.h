// file: include/purrgo/map.h
#ifndef PURRGO_MAP_H
#define PURRGO_MAP_H

#include <stdint.h>
#include <stdbool.h>
#include "purrgo/gfx_renderer.h"

/* Абстракция файловой системы для инъекции зависимостей */
typedef struct {
    void* handle;
    uint32_t (*read)(void* handle, void* buffer, uint32_t size);
    bool (*seek)(void* handle, uint32_t offset);
} purrgo_fs_t;

/* Нативный формат координат системы (градусы * 10^7) */
typedef struct {
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;
} purrgo_bbox_t;

/* Параметры целевого дисплея (Viewport) */
typedef struct {
    uint16_t width;
    uint16_t height;
    int16_t offset_x;
    int16_t offset_y;
} purrgo_viewport_t;

void purrgo_map_render_layer(
    purrgo_fs_t* idx_fs, 
    purrgo_fs_t* mlp_fs, 
    gfx_context_t* gfx,
    const purrgo_bbox_t* camera,
    const purrgo_viewport_t* viewport,
    bool is_polygon_layer
);

#endif // PURRGO_MAP_H