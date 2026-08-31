#include "purrgo/gfx_text.h"
#include "purrgo/font5x7.h"
#include "purrgo/gfx_renderer.h"

/*
 * Отвечает за отрисовку одного символа (шрифт 5x7).
 * Пробел между символами (1px) не закрашивается (прозрачный).
 */
void gfx_draw_char(gfx_context_t *ctx, int16_t x, int16_t y, char c) {
    if (!ctx || !ctx->draw_pixel) return;
    
    const unsigned char* bitmap = font5x7[(unsigned char)c];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            if ((bitmap[col] >> row) & 1) {
                gfx_draw_pixel(ctx, x + col, y + row);
            }
        }
    }
}

/*
 * Внутренняя функция: отрисовка контура (ореола) вокруг символа.
 * Отрисовывает пиксели в радиусе 1px вокруг каждого заполненного пикселя буквы.
 */
static void gfx_draw_char_halo(gfx_context_t *ctx, int16_t x, int16_t y, char c) {
    if (!ctx || !ctx->draw_pixel) return;

    const unsigned char* bitmap = font5x7[(unsigned char)c];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            if ((bitmap[col] >> row) & 1) {
                // Обводим вокруг целевого пикселя (матрица 3x3)
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        if (dx != 0 || dy != 0) {
                            gfx_draw_pixel(ctx, x + col + dx, y + row + dy);
                        }
                    }
                }
            }
        }
    }
}

/*
 * Отрисовка строки с жестко заданным белым ореолом и черным текстом.
 * Идеально подходит для подписей на карте (POI, улицы) поверх любой геометрии.
 */
void gfx_draw_string_halo(gfx_context_t *ctx, int16_t x, int16_t y, const char *str) {
    if (!ctx || !str) return;

    int16_t start_x = x;
    const char *s = str;
    gfx_color_t original_fg = ctx->color_fg;

    // --- Проход 1: Отрисовка белого ореола ---
    ctx->color_fg = WHITE;
    while (*s) {
        if (*s == '\n') {
            x = start_x;
            y += 8;
        } else {
            gfx_draw_char_halo(ctx, x, y, *s);
            x += 6; // 5px ширина + 1px межбуквенный интервал
        }
        s++;
    }

    // --- Проход 2: Отрисовка черного текста ---
    x = start_x;
    s = str;
    ctx->color_fg = BLACK;
    while (*s) {
        if (*s == '\n') {
            x = start_x;
            y += 8;
        } else {
            gfx_draw_char(ctx, x, y, *s);
            x += 6;
        }
        s++;
    }

    // Восстанавливаем цвет, который был до вызова функции
    ctx->color_fg = original_fg;
}

/*
 * Стандартная отрисовка строки текущим цветом (без обводки).
 * Используется для UI-элементов, где подложка гарантированно однотонная.
 */
void gfx_draw_string(gfx_context_t *ctx, int16_t x, int16_t y, const char *str) {
    if (!ctx || !str) return;
    
    int16_t start_x = x;
    
    while (*str) {
        if (*str == '\n') {
            x = start_x;
            y += 8;
        } else {
            gfx_draw_char(ctx, x, y, *str);
            x += 6;
        }
        str++;
    }
}