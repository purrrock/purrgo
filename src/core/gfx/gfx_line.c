#include "purrgo/gfx_line.h"

// Битовые маски зон для алгоритма Коэна-Сазерленда
#define INSIDE 0 // 0000
#define LEFT   1 // 0001
#define RIGHT  2 // 0010
#define BOTTOM 4 // 0100
#define TOP    8 // 1000

// Побитовая оптимизация модуля без ветвления для архитектуры ARM Cortex
static inline int16_t gfx_abs(int16_t a) {
    // Арифметический сдвиг вправо на 15 бит даст -1 (0xFFFF) для отрицательных, и 0 для положительных
    int16_t mask = a >> 15;
    return (a + mask) ^ mask;
}

// Вычисление кода зоны для точки относительно границ экрана
static uint8_t compute_outcode(gfx_context_t *ctx, int16_t x, int16_t y) {
    uint8_t code = INSIDE;
    if (x < 0) {
        code |= LEFT;
    } else if (x >= ctx->width) {
        code |= RIGHT;
    }
    
    if (y < 0) {
        code |= BOTTOM;
    } else if (y >= ctx->height) {
        code |= TOP;
    }
    return code;
}

void gfx_draw_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (ctx == NULL || ctx->draw_pixel == NULL) return;

    // 1. Отсечение отрезка (Cohen-Sutherland)
    uint8_t outcode0 = compute_outcode(ctx, x0, y0);
    uint8_t outcode1 = compute_outcode(ctx, x1, y1);
    bool accept = false;

    while (true) {
        if (!(outcode0 | outcode1)) {
            // Побитовое ИЛИ равно 0: обе точки внутри экрана
            accept = true;
            break;
        } else if (outcode0 & outcode1) {
            // Побитовое И не равно 0: обе точки находятся в одной невидимой зоне
            break;
        } else {
            // Линия пересекает границу экрана. Вычисляем точку пересечения.
            int16_t x, y;
            uint8_t outcodeOut = outcode0 ? outcode0 : outcode1;

            // Использование 32-битной арифметики для предотвращения переполнения
            // при умножении (координаты до умножения int16_t)
            int32_t dx = x1 - x0;
            int32_t dy = y1 - y0;

            if (outcodeOut & TOP) {
                x = x0 + dx * (ctx->height - 1 - y0) / dy;
                y = ctx->height - 1;
            } else if (outcodeOut & BOTTOM) {
                x = x0 + dx * (0 - y0) / dy;
                y = 0;
            } else if (outcodeOut & RIGHT) {
                y = y0 + dy * (ctx->width - 1 - x0) / dx;
                x = ctx->width - 1;
            } else if (outcodeOut & LEFT) {
                y = y0 + dy * (0 - x0) / dx;
                x = 0;
            }

            // Заменяем точку вне экрана на точку пересечения и обновляем код
            if (outcodeOut == outcode0) {
                x0 = x;
                y0 = y;
                outcode0 = compute_outcode(ctx, x0, y0);
            } else {
                x1 = x;
                y1 = y;
                outcode1 = compute_outcode(ctx, x1, y1);
            }
        }
    }

    if (!accept) {
        return; // Линия полностью вне экрана
    }

    // 2. Отрисовка видимой части (Bresenham optimized)
    int16_t dx = gfx_abs(x1 - x0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = -gfx_abs(y1 - y0); // Храним dy как отрицательное значение
    int16_t sy = y0 < y1 ? 1 : -1;
    
    // Инициализация ошибки без деления
    int16_t err = dx + dy; 
    int16_t e2;

    while (true) {
        ctx->draw_pixel(ctx->framebuffer, x0, y0, ctx->color_fg);

        if (x0 == x1 && y0 == y1) {
            break;
        }

        // Вычисление смещения через побитовый сдвиг (умножение на 2)
        e2 = err << 1; 
        
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}