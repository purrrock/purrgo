#include "purrgo/gfx_text.h"
#include "purrgo/font5x7.h"
#include "purrgo/gfx_renderer.h"

/*
 * Внутренняя функция: прозрачная отрисовка (только текст).
 * Используется для подписей на карте, чтобы фон буквы не затирал созданный ореол и геометрию.
 */
static void gfx_draw_char_transparent(gfx_context_t *ctx, int16_t x, int16_t y, char c) {
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
 * Отвечает за отрисовку одного символа с заливкой фона (шрифт 5x7).
 * Закрашивает и текст (color_fg), и фон (color_bg), включая межбуквенный 6-й пиксель.
 * Критично для правильной отрисовки инверсных (выделенных) пунктов меню в UI.
 */
void gfx_draw_char(gfx_context_t *ctx, int16_t x, int16_t y, char c) {
    if (!ctx || !ctx->draw_pixel) return;
    
    const unsigned char* bitmap = font5x7[(unsigned char)c];
    gfx_color_t fg = ctx->color_fg;
    gfx_color_t bg = ctx->color_bg;

    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            // Рисуем либо цвет текста, либо цвет фона
            ctx->color_fg = ((bitmap[col] >> row) & 1) ? fg : bg;
            gfx_draw_pixel(ctx, x + col, y + row);
        }
    }
    
    // Обязательная заливка межбуквенного интервала (6-я колонка) цветом фона
    ctx->color_fg = bg;
    for (int row = 0; row < 8; row++) {
        gfx_draw_pixel(ctx, x + 5, y + row);
    }
    
    // Восстановление оригинального цвета в контексте
    ctx->color_fg = fg;
}

/*
 * Внутренняя функция: отрисовка контура (ореола) вокруг символа.
 */
static void gfx_draw_char_halo(gfx_context_t *ctx, int16_t x, int16_t y, char c) {
    if (!ctx || !ctx->draw_pixel) return;

    const unsigned char* bitmap = font5x7[(unsigned char)c];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            if ((bitmap[col] >> row) & 1) {
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
            x += 6; 
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
            // Используем прозрачную отрисовку, чтобы фон не затер ореол
            gfx_draw_char_transparent(ctx, x, y, *s);
            x += 6;
        }
        s++;
    }

    ctx->color_fg = original_fg;
}

/*
 * Стандартная отрисовка строки текущим цветом (с непрозрачным фоном).
 * Используется для UI-элементов.
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