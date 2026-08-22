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
 * @brief Заливка полигона.
 *
 * @param ctx Контекст графического ядра.
 * @param points Массив вершин полигона.
 * @param count Количество вершин в массиве.
 */
void gfx_fill_polygon(gfx_context_t *ctx, const gfx_point_t *points, uint16_t count);

#endif /* GFX_POLYGON_H */
