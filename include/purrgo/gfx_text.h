#ifndef GFX_TEXT_H
#define GFX_TEXT_H

#include "purrgo/gfx_renderer.h"

void gfx_draw_char(gfx_context_t *ctx, int16_t x, int16_t y, char c);

void gfx_draw_string(gfx_context_t *ctx, int16_t x, int16_t y, const char *str);

/*
 * Отрисовка строки с белым ореолом и черным текстом для подписей на карте.
 */
void gfx_draw_string_halo(gfx_context_t *ctx, int16_t x, int16_t y, const char *str);

#endif /* GFX_TEXT_H */