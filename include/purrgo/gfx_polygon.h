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
 * @brief Заливка полигона без holes (обратная совместимость).
 *
 * @param ctx Контекст графического ядра.
 * @param points Массив вершин полигона.
 * @param count Количество вершин в массиве.
 */
void gfx_fill_polygon(gfx_context_t *ctx, const gfx_point_t *points, uint16_t count);

/**
 * @brief Заливка compound полигона (с holes).
 *
 * @param ctx Контекст графического ядра.
 * @param points Массив вершин полигона.
 * @param num_points Общее количество вершин во всех частях.
 * @param parts Массив начальных индексов частей полигона (колец).
 * @param num_parts Количество частей (колец) полигона.
 */
void gfx_fill_compound_polygon(
    gfx_context_t *ctx,
    const gfx_point_t *points,
    uint16_t num_points,
    const uint32_t *parts,
    uint16_t num_parts
);

#endif /* GFX_POLYGON_H */
