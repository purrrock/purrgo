#include "purrgo/gfx_renderer.h"
#include <stddef.h>

bool gfx_init(gfx_context_t *ctx,
              int16_t width,
              int16_t height,
              void *framebuffer,
              gfx_draw_pixel_fn draw_pixel_cb)
{
    if (ctx == NULL || framebuffer == NULL || draw_pixel_cb == NULL) {
        return false;
    }

    ctx->width = width;
    ctx->height = height;
    ctx->framebuffer = framebuffer;
    ctx->draw_pixel = draw_pixel_cb;
    ctx->color_fg = 1;
    ctx->color_bg = 0;

    return true;
}

void gfx_set_color(gfx_context_t *ctx, gfx_color_t fg, gfx_color_t bg)
{
    if (ctx == NULL) return;

    ctx->color_fg = fg;
    ctx->color_bg = bg;
}

void gfx_draw_pixel(gfx_context_t *ctx, int16_t x, int16_t y)
{
    if (ctx == NULL || ctx->draw_pixel == NULL) return;

    // Software clipping
    if (x >= 0 && x < ctx->width && y >= 0 && y < ctx->height) {
        ctx->draw_pixel(ctx->framebuffer, x, y, ctx->color_fg);
    }
}

void gfx_clear(gfx_context_t *ctx)
{
    if (ctx == NULL) return;

    // Temporarily swap foreground color with background color
    // since gfx_draw_pixel uses color_fg
    gfx_color_t old_fg = ctx->color_fg;
    ctx->color_fg = ctx->color_bg;

    for (int16_t y = 0; y < ctx->height; y++) {
        for (int16_t x = 0; x < ctx->width; x++) {
            gfx_draw_pixel(ctx, x, y);
        }
    }

    // Restore foreground color
    ctx->color_fg = old_fg;
}
