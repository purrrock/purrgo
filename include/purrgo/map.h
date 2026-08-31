// file: include/purrgo/map.h
#ifndef PURRGO_MAP_H
#define PURRGO_MAP_H

#include <stdint.h>
#include <stdbool.h>

#include "purrgo/gfx_renderer.h"


/*
 * Абстракция файловой системы для инъекции зависимостей.
 *
 * map.c использует этот интерфейс для работы как с обычными
 * файлами fs_hal, так и с другими источниками данных.
 */
typedef struct {
    void* handle;

    uint32_t (*read)(
        void* handle,
        void* buffer,
        uint32_t size
    );

    bool (*seek)(
        void* handle,
        uint32_t offset
    );
} purrgo_fs_t;


/*
 * Нативный формат координат системы:
 *
 *     градусы * 10^7
 *
 * BBox задаёт прямоугольную область карты.
 */
typedef struct {
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;
} purrgo_bbox_t;


/*
 * Параметры целевого дисплея (Viewport).
 */
typedef struct {
    uint16_t width;
    uint16_t height;

    int16_t offset_x;
    int16_t offset_y;
} purrgo_viewport_t;


/*
 * Тип слоя, обрабатываемого IDX renderer.
 *
 * MAP_LAYER_LINES:
 *     линейные объекты.
 *     Геометрия находится в MLP.
 *
 * MAP_LAYER_POLYGONS:
 *     полигональные объекты.
 *     Геометрия находится в MLP.
 *
 * MAP_LAYER_POIS:
 *     точечные объекты.
 *
 * POI не используют MLP.
 * Их координаты находятся непосредственно в BBox Data Node:
 *
 *     xmin == xmax
 *     ymin == ymax
 */
typedef enum {
    MAP_LAYER_LINES = 0,
    MAP_LAYER_POLYGONS,
    MAP_LAYER_POIS
} purrgo_map_layer_t;


/*
 * Рендерит один слой карты для указанного viewport.
 *
 * Для линий и полигонов:
 *
 *     idx_fs != NULL
 *     mlp_fs != NULL
 *
 * Для POI:
 *
 *     idx_fs != NULL
 *     mlp_fs == NULL
 *
 * Выбор LOD выполняется внутри renderer на основании
 * текущего масштаба карты.
 */
void purrgo_map_render_layer(
    purrgo_fs_t *idx_fs,
    purrgo_fs_t *mlp_fs,
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *viewport,
    purrgo_map_layer_t layer_type
);


/*
 * High-level viewport rendering function that handles opening files.
 *
 * Функция открывает необходимые файлы слоя карты и запускает
 * их отрисовку для заданного viewport.
 */
bool purrgo_map_render_viewport(
    gfx_context_t *gfx,
    const purrgo_viewport_t *viewport,
    const purrgo_bbox_t *camera,
    const char *map_dir
);


#endif /* PURRGO_MAP_H */