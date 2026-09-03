#include "purrgo/gfx_icon.h"
#include <stddef.h>

void gfx_draw_icon_7x7(gfx_context_t *ctx, int16_t x, int16_t y, const uint8_t icon[7][7])
{
    if (ctx == NULL || icon == NULL) {
        return;
    }

    // Сохраняем исходный цвет переднего плана
    gfx_color_t old_fg;
    gfx_get_color(ctx, &old_fg, NULL);

    int16_t start_x = x - 3;
    int16_t start_y = y - 3;

    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 7; col++) {
            uint8_t pixel = icon[row][col];

            // Проверяем бит прозрачности (бит 2)
            if (pixel & 0x04) {
                // Извлекаем цвет (биты 1:0)
                gfx_color_t color = (gfx_color_t)(pixel & 0x03);

                // Устанавливаем новый цвет переднего плана
                gfx_set_color(ctx, color, ctx->color_bg);

                // Отрисовываем пиксель с использованием существующего clipping
                gfx_draw_pixel(ctx, start_x + col, start_y + row);
            }
        }
    }

    // Восстанавливаем исходный цвет переднего плана
    gfx_set_color(ctx, old_fg, ctx->color_bg);
}
