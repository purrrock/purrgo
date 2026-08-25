#ifndef GFX_LINE_H
#define GFX_LINE_H

#include "purrgo/gfx_renderer.h"
#include <stdint.h>

/**
 * @brief Отрисовка линии с использованием целочисленного алгоритма Брезенхема.
 *
 * @param ctx Контекст графического ядра.
 * @param x0 Начальная координата x.
 * @param y0 Начальная координата y.
 * @param x1 Конечная координата x.
 * @param y1 Конечная координата y.
 */
void gfx_draw_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1);

/**
 * @brief Отрисовка тонкой штриховой линии (пунктира) с использованием алгоритма Брезенхема.
 *
 * @param ctx Контекст графического ядра.
 * @param x0 Начальная координата x.
 * @param y0 Начальная координата y.
 * @param x1 Конечная координата x.
 * @param y1 Конечная координата y.
 */
void gfx_draw_dashed_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1);

/**
 * @brief Отрисовка утолщенной линии с использованием целочисленного алгоритма (дублирование пикселей).
 *
 * @param ctx Контекст графического ядра.
 * @param x0 Начальная координата x.
 * @param y0 Начальная координата y.
 * @param x1 Конечная координата x.
 * @param y1 Конечная координата y.
 * @param thickness Толщина линии в пикселях.
 */
void gfx_draw_thick_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t thickness);

/**
 * @brief Отрисовка точечной линии (чередование 1 пиксель линии, 1 пиксель пропуска).
 *
 * @param ctx Контекст графического ядра.
 * @param x0 Начальная координата x.
 * @param y0 Начальная координата y.
 * @param x1 Конечная координата x.
 * @param y1 Конечная координата y.
 */
void gfx_draw_dotted_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1);

/**
 * @brief Отрисовка железнодорожной линии.
 * 3-пиксельная черная линия с 1-пиксельной белой прерывистой линией поверх.
 *
 * @param ctx Контекст графического ядра.
 * @param x0 Начальная координата x.
 * @param y0 Начальная координата y.
 * @param x1 Конечная координата x.
 * @param y1 Конечная координата y.
 */
void gfx_draw_railway_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1);

#endif /* GFX_LINE_H */
