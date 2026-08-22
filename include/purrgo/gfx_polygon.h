#ifndef GFX_POLYGON_H
#define GFX_POLYGON_H

#include "purrgo/gfx_renderer.h"
#include <stdint.h>

/**
 * @brief Отрисовка контура полигона.
 *
 * @param ctx Контекст графического ядра.
 * @param points Массив вершин полигона.
 * @param count Количество вершин в массиве.
 */
void gfx_draw_polygon(gfx_context_t *ctx, const gfx_point_t *points, uint16_t count);

/**
 * @brief Заливка полигона (с поддержкой compound polygons/holes).
 *
 * @param ctx Контекст графического ядра.
 * @param points Массив вершин полигона.
 * @param num_points Общее количество вершин.
 * @param parts Массив начальных индексов каждого кольца.
 * @param num_parts Количество колец.
 */
void gfx_fill_polygon(gfx_context_t *ctx, const gfx_point_t *points, uint16_t num_points, const uint32_t *parts, uint16_t num_parts);

#endif /* GFX_POLYGON_H */
