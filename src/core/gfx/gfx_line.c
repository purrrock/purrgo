#include "purrgo/gfx_line.h"
#include <stdlib.h> // For abs, although we can implement our own if abs is restricted, but usually stdlib abs is fine

// Вспомогательная функция для модуля числа
static inline int16_t gfx_abs(int16_t a) {
    return (a < 0) ? -a : a;
}

void gfx_draw_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (ctx == NULL) return;

    // Вычисляем абсолютные разницы координат (dx и dy)
    int16_t dx = gfx_abs(x1 - x0);
    int16_t dy = gfx_abs(y1 - y0);

    // Определяем направление шага по осям X и Y
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;

    // Инициализируем переменную ошибки
    // Если dx > dy, то линия идет более горизонтально, иначе - более вертикально.
    int16_t err = ((dx > dy) ? dx : -dy) / 2;
    int16_t e2;

    while (1) {
        // Отрисовка текущего пикселя
        // gfx_draw_pixel уже включает проверку выхода за границы (clipping)
        gfx_draw_pixel(ctx, x0, y0);

        // Условие выхода: достигнута конечная точка
        if (x0 == x1 && y0 == y1) {
            break;
        }

        // Сохраняем текущее значение ошибки для проверок по обеим осям
        e2 = err;

        // Коррекция ошибки и шаг по оси X
        if (e2 > -dx) {
            err -= dy;
            x0 += sx;
        }

        // Коррекция ошибки и шаг по оси Y
        if (e2 < dy) {
            err += dx;
            y0 += sy;
        }
    }
}
