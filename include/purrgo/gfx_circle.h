#ifndef GFX_CIRCLE_H
#define GFX_CIRCLE_H

#include "purrgo/gfx_renderer.h"
#include <stdint.h>

void gfx_draw_circle(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t r);
void gfx_fill_circle(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t r);

#endif /* GFX_CIRCLE_H */
