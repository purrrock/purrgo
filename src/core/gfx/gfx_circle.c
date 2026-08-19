#include "purrgo/gfx_circle.h"
#include <stddef.h>

void gfx_draw_circle(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t r)
{
    if (ctx == NULL || r < 0) return;

    int16_t x = r;
    int16_t y = 0;

    // Переменная ошибки для алгоритма Брезенхема/Midpoint.
    // Начинается с 1 - r, но для целых чисел удобнее использовать 1 - r,
    // или эквивалентную форму (здесь используем err = 1 - r)
    int16_t err = 1 - r;

    // Отрисовываем 8 симметричных точек
    while (x >= y) {
        // Октанты 1 и 8 (справа)
        gfx_draw_pixel(ctx, x0 + x, y0 + y);
        gfx_draw_pixel(ctx, x0 + x, y0 - y);

        // Октанты 2 и 7 (сверху)
        gfx_draw_pixel(ctx, x0 + y, y0 + x);
        gfx_draw_pixel(ctx, x0 + y, y0 - x);

        // Октанты 3 и 6 (слева)
        gfx_draw_pixel(ctx, x0 - x, y0 + y);
        gfx_draw_pixel(ctx, x0 - x, y0 - y);

        // Октанты 4 и 5 (снизу)
        gfx_draw_pixel(ctx, x0 - y, y0 + x);
        gfx_draw_pixel(ctx, x0 - y, y0 - x);

        y++;

        // Обновление переменной ошибки
        if (err < 0) {
            // Если точка внутри окружности, смещаемся только по y
            err += 2 * y + 1;
        } else {
            // Иначе смещаемся и по x, и по y
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

// Отрисовка горизонтальной линии с аппаратным/прямым отсечением для быстрой заливки
static void draw_horizontal_line(gfx_context_t *ctx, int16_t x_start, int16_t x_end, int16_t y)
{
    // Отсекаем по Y (clipping)
    if (y < 0 || y >= ctx->height) return;

    // Гарантируем, что x_start <= x_end
    if (x_start > x_end) {
        int16_t temp = x_start;
        x_start = x_end;
        x_end = temp;
    }

    // Отсекаем по X (clipping)
    if (x_end < 0 || x_start >= ctx->width) return;

    if (x_start < 0) x_start = 0;
    if (x_end >= ctx->width) x_end = ctx->width - 1;

    // Рисуем линию
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
        // Отрисовка горизонтальных линий между симметричными точками на одной высоте
        // Линии по y0 + y и y0 - y, шириной от x0 - x до x0 + x
        draw_horizontal_line(ctx, x0 - x, x0 + x, y0 + y);
        draw_horizontal_line(ctx, x0 - x, x0 + x, y0 - y);

        // Линии по y0 + x и y0 - x, шириной от x0 - y до x0 + y
        // (рисуем их только если они не пересекаются с предыдущими, т.е. x != y)
        if (x != y) {
            draw_horizontal_line(ctx, x0 - y, x0 + y, y0 + x);
            draw_horizontal_line(ctx, x0 - y, x0 + y, y0 - x);
        }

        y++;

        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}
