#ifndef GFX_CIRCLE_H
#define GFX_CIRCLE_H

#include "purrgo/gfx_renderer.h"
#include <stdint.h>

void gfx_draw_circle(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t r);
void gfx_fill_circle(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t r);

/**
 * @brief Отрисовывает POI контрастным кругом.
 *
 * Внутри круга используется DARK_GRAY, внешний контур — WHITE.
 * Цвета не зависят от color_fg/color_bg контекста.
 *
 * @param ctx Графический контекст.
 * @param x0 Координата центра X.
 * @param y0 Координата центра Y.
 * @param r Радиус круга в пикселях.
 */
void gfx_draw_poi_circle(
    gfx_context_t *ctx,
    int16_t x0,
    int16_t y0,
    int16_t r
);

#endif /* GFX_CIRCLE_H */