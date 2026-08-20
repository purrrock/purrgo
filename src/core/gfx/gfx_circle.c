#include "purrgo/gfx_circle.h"
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
            // Оптимизация: 2 * (y - x) заменено на битовый сдвиг влево
            err += ((y - x) << 1) + 1;
        }
    }
}

// Inline-функция для исключения накладных расходов на вызов
static inline void draw_horizontal_line(gfx_context_t *ctx, int16_t x_start, int16_t x_end, int16_t y)
{
    // Отсечение невидимых строк по оси Y
    if (y < 0 || y >= ctx->height) return;

    if (x_start > x_end) {
        int16_t temp = x_start;
        x_start = x_end;
        x_end = temp;
    }

    // Отсечение невидимых отрезков по оси X
    if (x_end < 0 || x_start >= ctx->width) return;

    if (x_start < 0) x_start = 0;
    if (x_end >= ctx->width) x_end = ctx->width - 1;

    // Прямой вызов платформенного коллбэка без проверок внутри цикла
    for (int16_t x = x_start; x <= x_end; x++) {
        ctx->draw_pixel(ctx->framebuffer, x, y, ctx->color_bg);
    }
}

void gfx_fill_circle(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t r)
{
    if (ctx == NULL || ctx->draw_pixel == NULL || r < 0) return;

    int16_t x = r;
    int16_t y = 0;
    int16_t err = 1 - r;

    while (x >= y) {
        draw_horizontal_line(ctx, x0 - x, x0 + x, y0 + y);
        draw_horizontal_line(ctx, x0 - x, x0 + x, y0 - y);

        if (x != y) {
            draw_horizontal_line(ctx, x0 - y, x0 + y, y0 + x);
            draw_horizontal_line(ctx, x0 - y, x0 + y, y0 - x);
        }

        y++;

        if (err < 0) {
            // Оптимизация вычисления ошибки
            err += (y << 1) + 1;
        } else {
            x--;
            err += ((y - x) << 1) + 1;
        }
    }
}