#include "purrgo/gfx_circle.h"
#include "purrgo/gfx_renderer.h"
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
            // Avoid UB with negative shifts by using multiplication
            err += ((y - x) * 2) + 1;
        }
    }
}

void gfx_fill_circle(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t r)
{
    if (ctx == NULL || ctx->draw_pixel == NULL || r < 0) return;

    int16_t x = r;
    int16_t y = 0;
    int16_t err = 1 - r;

    // Временная подмена цвета для заливки цветом фона
    gfx_color_t old_fg = ctx->color_fg;
    ctx->color_fg = ctx->color_bg;

    while (x >= y) {
        gfx_draw_hline(ctx, x0 - x, x0 + x, y0 + y);
        gfx_draw_hline(ctx, x0 - x, x0 + x, y0 - y);

        if (x != y) {
            gfx_draw_hline(ctx, x0 - y, x0 + y, y0 + x);
            gfx_draw_hline(ctx, x0 - y, x0 + y, y0 - x);
        }

        y++;

        if (err < 0) {
            // Оптимизация вычисления ошибки
            err += (y << 1) + 1;
        } else {
            x--;
            // Avoid UB with negative shifts by using multiplication
            err += ((y - x) * 2) + 1;
        }
    }

    // Восстановление исходного цвета переднего плана
    ctx->color_fg = old_fg;
}


/*
 * Отрисовка POI в виде контрастного круга.
 *
 * Внешний контур:
 *     WHITE
 *
 * Внутренняя область:
 *     DARK_GRAY
 *
 * POI является точечным объектом, поэтому x0/y0 — координаты
 * его центра. Радиус передается вызывающим кодом.
 *
 * Цвета здесь намеренно не берутся из ctx->color_fg/color_bg:
 * POI должен иметь фиксированный контрастный вид независимо
 * от текущего состояния графического контекста.
 *
 * gfx_draw_pixel() и gfx_draw_hline() выполняют clipping,
 * поэтому отдельная проверка границ экрана здесь не требуется.
 */
void gfx_draw_poi_circle(
    gfx_context_t *ctx,
    int16_t x0,
    int16_t y0,
    int16_t r
)
{
    if (ctx == NULL || ctx->draw_pixel == NULL || r < 0) {
        return;
    }

    /*
     * Сначала заполняем круг DARK_GRAY.
     *
     * Сохраняем текущий цвет переднего плана, чтобы примитив
     * не изменял состояние внешнего графического контекста.
     */
    gfx_color_t old_fg = ctx->color_fg;
    ctx->color_fg = DARK_GRAY;

    int16_t x = r;
    int16_t y = 0;
    int16_t err = 1 - r;

    while (x >= y) {
        gfx_draw_hline(ctx, x0 - x, x0 + x, y0 + y);
        gfx_draw_hline(ctx, x0 - x, x0 + x, y0 - y);

        if (x != y) {
            gfx_draw_hline(ctx, x0 - y, x0 + y, y0 + x);
            gfx_draw_hline(ctx, x0 - y, x0 + y, y0 - x);
        }

        y++;

        if (err < 0) {
            err += (y << 1) + 1;
        } else {
            x--;
            err += ((y - x) * 2) + 1;
        }
    }

    /*
     * Затем поверх заливки рисуем контур WHITE.
     *
     * Это гарантирует контраст POI как на светлых,
     * так и на темных участках карты.
     */
    ctx->color_fg = WHITE;

    x = r;
    y = 0;
    err = 1 - r;

    while (x >= y) {
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
            err += (y << 1) + 1;
        } else {
            x--;
            err += ((y - x) * 2) + 1;
        }
    }

    /*
     * Восстанавливаем исходный цвет контекста.
     */
    ctx->color_fg = old_fg;
}