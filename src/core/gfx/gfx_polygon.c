#include "purrgo/gfx_polygon.h"
#include "purrgo/gfx_line.h"
#include "purrgo/gfx_renderer.h"

void gfx_draw_polygon(gfx_context_t *ctx, const gfx_point_t *points, uint16_t count) {
    if (!ctx || !points || count < 2) {
        return;
    }

    for (uint16_t i = 0; i < count - 1; i++) {
        gfx_draw_line(ctx, points[i].x, points[i].y, points[i+1].x, points[i+1].y);
    }

    // Замыкаем полигон
    gfx_draw_line(ctx, points[count - 1].x, points[count - 1].y, points[0].x, points[0].y);
}

void gfx_fill_polygon(gfx_context_t *ctx, const gfx_point_t *points, uint16_t count) {
    if (!ctx || !points || count < 3) {
        return;
    }

    int16_t min_y = points[0].y;
    int16_t max_y = points[0].y;

    for (uint16_t i = 1; i < count; i++) {
        if (points[i].y < min_y) min_y = points[i].y;
        if (points[i].y > max_y) max_y = points[i].y;
    }

    if (min_y < 0) min_y = 0;
    if (max_y >= ctx->height) max_y = ctx->height - 1;

    // Временная подмена цвета для заливки цветом фона
    gfx_color_t old_fg = ctx->color_fg;
    ctx->color_fg = ctx->color_bg;

    for (int16_t y = min_y; y <= max_y; y++) {
        int16_t nodeX[32];
        uint16_t nodes = 0;

        for (uint16_t i = 0; i < count; i++) {
            uint16_t j = (i == count - 1) ? 0 : i + 1;

            int16_t y_start = points[i].y;
            int16_t y_end = points[j].y;
            int16_t x_start = points[i].x;
            int16_t x_end = points[j].x;

            // Игнорируем горизонтальные ребра и проверяем полуоткрытый интервал
            if (y_start == y_end) continue;

            if ((y_start <= y && y_end > y) || (y_end <= y && y_start > y)) {
                if (nodes < 32) {
                    nodeX[nodes++] = x_start + (int32_t)(x_end - x_start) * (y - y_start) / (y_end - y_start);
                }
            }
        }

        // Сортировка пузырьком
        for (uint16_t i = 0; i < nodes; i++) {
            for (uint16_t j = 0; j < nodes - i - 1; j++) {
                if (nodeX[j] > nodeX[j + 1]) {
                    int16_t tmp = nodeX[j];
                    nodeX[j] = nodeX[j + 1];
                    nodeX[j + 1] = tmp;
                }
            }
        }

        // Отрисовка попарных интервалов с использованием gfx_draw_hline (которая теперь берет подмененный color_fg)
        for (uint16_t i = 0; i < nodes; i += 2) {
            if (i + 1 < nodes) {
                gfx_draw_hline(ctx, nodeX[i], nodeX[i+1], y);
            }
        }
    }

    // Восстановление исходного цвета переднего плана
    ctx->color_fg = old_fg;
}