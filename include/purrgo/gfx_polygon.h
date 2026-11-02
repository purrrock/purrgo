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
 * @brief Выпуклый полигон с заливкой строго внутри контура (маркёры).
 *
 * Обычные gfx_fill_polygon() + gfx_draw_polygon() растеризуют края по-разному
 * (заливка берёт пересечения с усечением, контур - Bresenham), поэтому на
 * мелких фигурах заливка местами выходит за контур на 1 px. Здесь на каждой
 * строке заливаются только пиксели строго между самым левым и самым правым
 * пикселем контура, а затем поверх рисуется сам контур (те же пиксели, что у
 * gfx_draw_polygon()): заливка не может оказаться снаружи рамки.
 *
 * Только для выпуклых полигонов высотой до GFX_BORDERED_MAX_H строк (иначе
 * рисуется обычная заливка и контур). Цвета foreground/background контекста
 * не меняются.
 *
 * @param fill   Цвет заливки.
 * @param border Цвет контура.
 */
#define GFX_BORDERED_MAX_H 32

void gfx_draw_polygon_bordered(
    gfx_context_t *ctx,
    const gfx_point_t *points,
    uint16_t count,
    gfx_color_t fill,
    gfx_color_t border
);

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
