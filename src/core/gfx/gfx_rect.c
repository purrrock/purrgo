#include "purrgo/gfx_rect.h"
#include <stddef.h>

void gfx_draw_rect(gfx_context_t *ctx, int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (ctx == NULL || w <= 0 || h <= 0) return;

    // Top and bottom
    for (int16_t i = x; i < x + w; i++) {
        gfx_draw_pixel(ctx, i, y);
        gfx_draw_pixel(ctx, i, y + h - 1);
    }
    // Left and right (avoiding corners already drawn by top/bottom)
    for (int16_t i = y + 1; i < y + h - 1; i++) {
        gfx_draw_pixel(ctx, x, i);
        gfx_draw_pixel(ctx, x + w - 1, i);
    }
}

// Utility function to get max of two int16_t
static inline int16_t max16(int16_t a, int16_t b) {
    return (a > b) ? a : b;
}

// Utility function to get min of two int16_t
static inline int16_t min16(int16_t a, int16_t b) {
    return (a < b) ? a : b;
}

void gfx_fill_rect(gfx_context_t *ctx, int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (ctx == NULL || ctx->draw_pixel == NULL || w <= 0 || h <= 0) return;

    int16_t start_x = max16(ctx->clip_x, x);
    int16_t start_y = max16(ctx->clip_y, y);
    int16_t end_x = min16(ctx->clip_x + ctx->clip_w, x + w);
    int16_t end_y = min16(ctx->clip_y + ctx->clip_h, y + h);

    for (int16_t cy = start_y; cy < end_y; cy++) {
        for (int16_t cx = start_x; cx < end_x; cx++) {
            ctx->draw_pixel(ctx->framebuffer, cx, cy, ctx->color_bg);
        }
    }
}
