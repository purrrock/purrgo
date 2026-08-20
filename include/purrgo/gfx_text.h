#ifndef GFX_TEXT_H
#define GFX_TEXT_H

#include "purrgo/gfx_renderer.h"

/*
 * Отвечает за отрисовку одного символа (шрифт 5x7).
 * Пробел между символами (1px) рисуется цветом фона.
 */
void gfx_draw_char(gfx_context_t *ctx, int16_t x, int16_t y, char c);

/*
 * Отрисовка строки с учетом переноса на новую строку по символу '\n'
 * или при достижении границы контекста (ctx->width).
 */
void gfx_draw_string(gfx_context_t *ctx, int16_t x, int16_t y, const char *str);

#endif /* GFX_TEXT_H */
