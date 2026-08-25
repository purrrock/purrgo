#include "purrgo/gfx_circle.h"
#include "purrgo/gfx_renderer.h"
#include <stddef.h>

void gfx_draw_circle(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t r)
{
    if (ctx == NULL || r < 0) return;

    int16_t x = r;
    int16_t y = 0;
    int16_t err = 1 - r;

    while (x >= y) {
        // Отрисовка с программным отсечением (clipping) внутри gfx_draw_pixel
        gfx_draw_pixel(ctx, x0 + x, y0 + y);
        gfx_draw_pixel(ctx, x0 + x, y0 - y);
        gfx_draw_pixel(ctx, x0 + y, y0 + x);
        gfx_draw_pixel(ctx, x0 + y, y0 - x);
        gfx_draw_pixel(ctx, x0 - x, y0 + y);
        gfx_draw_pixel(ctx, x0 - x, y0 - y);
        gfx_draw_pixel(ctx, x0 - y, y0 + x);
        gfx_draw_pixel(ctx, x0 - y, y0 - x);

        y++;

        if (err < 0) {
            // Оптимизация: 2 * y заменено на битовый сдвиг влево (y << 1)
            err += (y << 1) + 1;
        } else {
            x--;
            // Avoid UB with negative shifts by using multiplication
            err += ((y - x) * 2) + 1;
        }
    }
}

void gfx_fill_circle(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t r)
{
    if (ctx == NULL || ctx->draw_pixel == NULL || r < 0) return;

    int16_t x = r;
    int16_t y = 0;
    int16_t err = 1 - r;

    // Временная подмена цвета для заливки цветом фона
    gfx_color_t old_fg = ctx->color_fg;
    ctx->color_fg = ctx->color_bg;

    while (x >= y) {
        gfx_draw_hline(ctx, x0 - x, x0 + x, y0 + y);
        gfx_draw_hline(ctx, x0 - x, x0 + x, y0 - y);

        if (x != y) {
            gfx_draw_hline(ctx, x0 - y, x0 + y, y0 + x);
            gfx_draw_hline(ctx, x0 - y, x0 + y, y0 - x);
        }

        y++;

        if (err < 0) {
            // Оптимизация вычисления ошибки
            err += (y << 1) + 1;
        } else {
            x--;
            // Avoid UB with negative shifts by using multiplication
            err += ((y - x) * 2) + 1;
        }
    }

    // Восстановление исходного цвета переднего плана
    ctx->color_fg = old_fg;
}