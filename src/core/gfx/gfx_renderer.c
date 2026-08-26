#include "purrgo/gfx_renderer.h"
#include <stddef.h>

bool gfx_init(gfx_context_t *ctx,
              int16_t width,
              int16_t height,
              void *framebuffer,
              gfx_draw_pixel_fn draw_pixel_cb, gfx_read_pixel_fn read_pixel_cb)
{
    if (ctx == NULL || framebuffer == NULL || draw_pixel_cb == NULL || read_pixel_cb == NULL) {
        return false;
    }

    ctx->width = width;
    ctx->height = height;
    ctx->framebuffer = framebuffer;
    ctx->draw_pixel = draw_pixel_cb;
    ctx->read_pixel = read_pixel_cb;
    ctx->color_fg = 1;
    ctx->color_bg = 0;

    ctx->clip_x = 0;
    ctx->clip_y = 0;
    ctx->clip_w = width;
    ctx->clip_h = height;

    return true;
}

void gfx_set_clip(gfx_context_t *ctx, int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (ctx == NULL) return;

    if (w < 0) w = 0;
    if (h < 0) h = 0;

    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }

    if (w < 0) w = 0;
    if (h < 0) h = 0;

    if (x >= ctx->width) {
        x = ctx->width;
        w = 0;
    }
    if (y >= ctx->height) {
        y = ctx->height;
        h = 0;
    }

    if (x + w > ctx->width) {
        w = ctx->width - x;
    }
    if (y + h > ctx->height) {
        h = ctx->height - y;
    }

    ctx->clip_x = x;
    ctx->clip_y = y;
    ctx->clip_w = w;
    ctx->clip_h = h;
}

void gfx_reset_clip(gfx_context_t *ctx)
{
    if (ctx == NULL) return;

    ctx->clip_x = 0;
    ctx->clip_y = 0;
    ctx->clip_w = ctx->width;
    ctx->clip_h = ctx->height;
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

    // Software clipping against clipping region
    if (x >= ctx->clip_x && x < ctx->clip_x + ctx->clip_w &&
        y >= ctx->clip_y && y < ctx->clip_y + ctx->clip_h) {
        ctx->draw_pixel(ctx->framebuffer, x, y, ctx->color_fg);
    }
}

void gfx_clear(gfx_context_t *ctx)
{
    if (ctx == NULL) return;

    // Temporarily swap foreground color with background color
    // since gfx_draw_hline uses color_fg
    gfx_color_t old_fg = ctx->color_fg;
    ctx->color_fg = ctx->color_bg;

    for (int16_t y = 0; y < ctx->height; y++) {
        gfx_draw_hline(ctx, 0, ctx->width - 1, y);
    }

    // Restore foreground color
    ctx->color_fg = old_fg;
}

void gfx_draw_hline(gfx_context_t *ctx, int16_t x_start, int16_t x_end, int16_t y)
{
    if (ctx == NULL || ctx->draw_pixel == NULL) return;

    // Отсечение невидимых строк по оси Y
    if (y < ctx->clip_y || y >= ctx->clip_y + ctx->clip_h) return;

    if (x_start > x_end) {
        int16_t temp = x_start;
        x_start = x_end;
        x_end = temp;
    }

    // Отсечение невидимых отрезков по оси X
    if (x_end < ctx->clip_x || x_start >= ctx->clip_x + ctx->clip_w) return;

    if (x_start < ctx->clip_x) x_start = ctx->clip_x;
    if (x_end >= ctx->clip_x + ctx->clip_w) x_end = ctx->clip_x + ctx->clip_w - 1;

    // Прямой вызов платформенного коллбэка. Используется цвет переднего плана (color_fg)
    for (int16_t x = x_start; x <= x_end; x++) {
        ctx->draw_pixel(ctx->framebuffer, x, y, ctx->color_fg);
    }
}

void gfx_draw_vline(gfx_context_t *ctx, int16_t x, int16_t y_start, int16_t y_end)
{
    if (ctx == NULL || ctx->draw_pixel == NULL) return;

    // Отсечение невидимых столбцов по оси X
    if (x < ctx->clip_x || x >= ctx->clip_x + ctx->clip_w) return;

    if (y_start > y_end) {
        int16_t temp = y_start;
        y_start = y_end;
        y_end = temp;
    }

    // Отсечение невидимых отрезков по оси Y
    if (y_end < ctx->clip_y || y_start >= ctx->clip_y + ctx->clip_h) return;

    if (y_start < ctx->clip_y) y_start = ctx->clip_y;
    if (y_end >= ctx->clip_y + ctx->clip_h) y_end = ctx->clip_y + ctx->clip_h - 1;

    // Прямой вызов платформенного коллбэка. Используется цвет переднего плана (color_fg)
    for (int16_t y = y_start; y <= y_end; y++) {
        ctx->draw_pixel(ctx->framebuffer, x, y, ctx->color_fg);
    }
}gfx_color_t gfx_read_pixel(gfx_context_t *ctx, int16_t x, int16_t y)
{
    if (ctx == NULL || ctx->read_pixel == NULL) return 0;

    // Software clipping against clipping region
    if (x >= ctx->clip_x && x < ctx->clip_x + ctx->clip_w &&
        y >= ctx->clip_y && y < ctx->clip_y + ctx->clip_h) {
        return ctx->read_pixel(ctx->framebuffer, x, y);
    }
    return 0;
}
