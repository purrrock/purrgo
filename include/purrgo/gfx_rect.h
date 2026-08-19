#ifndef GFX_RECT_H
#define GFX_RECT_H

#include "purrgo/gfx_renderer.h"
#include <stdint.h>

void gfx_draw_rect(gfx_context_t *ctx, int16_t x, int16_t y, int16_t w, int16_t h);
void gfx_fill_rect(gfx_context_t *ctx, int16_t x, int16_t y, int16_t w, int16_t h);

#endif /* GFX_RECT_H */
