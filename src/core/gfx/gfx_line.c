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

void gfx_draw_thick_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t thickness)
{
    if (ctx == NULL || thickness <= 0) return;

    if (thickness == 1) {
        gfx_draw_line(ctx, x0, y0, x1, y1);
        return;
    }

    int16_t dx = gfx_abs(x1 - x0);
    int16_t dy = gfx_abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = ((dx > dy) ? dx : -dy) / 2;
    int16_t e2;

    // Смещение для дублирования пикселей:
    // половина толщины в одну сторону и половина в другую
    int16_t half_thick = thickness / 2;
    int16_t start_offset = -half_thick;
    int16_t end_offset = start_offset + thickness - 1;

    // Флаг: если dx > dy (линия крутая по X), дублируем по Y, иначе дублируем по X
    bool steep = (dx > dy);

    while (1) {
        // Отрисовка утолщения для текущей точки
        for (int16_t offset = start_offset; offset <= end_offset; ++offset) {
            if (steep) {
                // Линия идет вдоль X, утолщаем по Y
                gfx_draw_pixel(ctx, x0, y0 + offset);
            } else {
                // Линия идет вдоль Y, утолщаем по X
                gfx_draw_pixel(ctx, x0 + offset, y0);
            }
        }

        if (x0 == x1 && y0 == y1) {
            break;
        }

        e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy) {
            err += dx;
            y0 += sy;
        }
    }
}
