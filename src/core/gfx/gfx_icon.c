#include "purrgo/gfx_icon.h"
#include <stddef.h>

void gfx_draw_icon_11x11(gfx_context_t *ctx, int16_t x, int16_t y, const uint8_t icon[11][11])
{
    if (ctx == NULL || icon == NULL) {
        return;
    }

    // Сохраняем исходные цвета
    gfx_color_t old_fg;
    gfx_color_t old_bg;
    gfx_get_color(ctx, &old_fg, &old_bg);

    int16_t start_x = x - 5;
    int16_t start_y = y - 5;

    for (int row = 0; row < 11; row++) {
        for (int col = 0; col < 11; col++) {
            uint8_t pixel = icon[row][col];

            // Проверяем бит прозрачности (бит 2)
            if (pixel & 0x04) {
                // Извлекаем цвет (биты 1:0)
                gfx_color_t color = (gfx_color_t)(pixel & 0x03);

                // Устанавливаем новый цвет переднего плана, сохраняя фон
                gfx_set_color(ctx, color, old_bg);

                // Отрисовываем пиксель с использованием существующего clipping
                gfx_draw_pixel(ctx, start_x + col, start_y + row);
            }
        }
    }

    // Восстанавливаем исходные цвета
    gfx_set_color(ctx, old_fg, old_bg);
}
