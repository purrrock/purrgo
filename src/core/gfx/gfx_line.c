#include "purrgo/gfx_line.h"
#include "purrgo/gfx_renderer.h"

// Битовые маски зон для алгоритма Коэна-Сазерленда
#define INSIDE 0 // 0000
#define LEFT   1 // 0001
#define RIGHT  2 // 0010
#define BOTTOM 4 // 0100
#define TOP    8 // 1000

// Побитовая оптимизация модуля без ветвления для архитектуры ARM Cortex
static inline int16_t gfx_abs(int16_t a) {
    int16_t mask = a >> 15;
    return (a + mask) ^ mask;
}

// Вычисление кода зоны для точки относительно границ экрана
static uint8_t compute_outcode(const gfx_context_t *ctx, int16_t x, int16_t y) {
    uint8_t code = INSIDE;
    if (x < ctx->clip_x) {
        code |= LEFT;
    } else if (x >= ctx->clip_x + ctx->clip_w) {
        code |= RIGHT;
    }
    
    if (y < ctx->clip_y) {
        code |= BOTTOM;
    } else if (y >= ctx->clip_y + ctx->clip_h) {
        code |= TOP;
    }
    return code;
}

static bool clip_line(gfx_context_t *ctx, int16_t *x0, int16_t *y0, int16_t *x1, int16_t *y1) {
    uint8_t outcode0 = compute_outcode(ctx, *x0, *y0);
    uint8_t outcode1 = compute_outcode(ctx, *x1, *y1);
    bool accept = false;

    while (true) {
        if (!(outcode0 | outcode1)) {
            accept = true;
            break;
        } else if (outcode0 & outcode1) {
            break;
        } else {
            int16_t x = 0, y = 0;
            uint8_t outcodeOut = outcode0 ? outcode0 : outcode1;

            int32_t dx = *x1 - *x0;
            int32_t dy = *y1 - *y0;

            if (outcodeOut & TOP) {
                x = (int16_t)(*x0 + (int32_t)((int64_t)dx * (ctx->clip_y + ctx->clip_h - 1 - *y0) / dy));
                y = (int16_t)(ctx->clip_y + ctx->clip_h - 1);
            } else if (outcodeOut & BOTTOM) {
                x = (int16_t)(*x0 + (int32_t)((int64_t)dx * (ctx->clip_y - *y0) / dy));
                y = ctx->clip_y;
            } else if (outcodeOut & RIGHT) {
                y = (int16_t)(*y0 + (int32_t)((int64_t)dy * (ctx->clip_x + ctx->clip_w - 1 - *x0) / dx));
                x = (int16_t)(ctx->clip_x + ctx->clip_w - 1);
            } else if (outcodeOut & LEFT) {
                y = (int16_t)(*y0 + (int32_t)((int64_t)dy * (ctx->clip_x - *x0) / dx));
                x = ctx->clip_x;
            }

            if (outcodeOut == outcode0) {
                *x0 = x;
                *y0 = y;
                outcode0 = compute_outcode(ctx, *x0, *y0);
            } else {
                *x1 = x;
                *y1 = y;
                outcode1 = compute_outcode(ctx, *x1, *y1);
            }
        }
    }
    return accept;
}

/*
 * Рисование линий: пиксели пишутся напрямую через callback платформы с отсечением по
 * области clip (без вызова gfx_draw_pixel на каждый пиксель), вычисления в int32_t,
 * а длинные пологие линии выводятся горизонтальными отрезками (fill_span).
 * Набор закрашиваемых пикселей совпадает с прежней реализацией (проверяет
 * test_gfx_line_equiv).
 */
typedef struct {
    gfx_draw_pixel_fn put;
    gfx_fill_span_fn span;     /* может быть NULL */
    void *fb;
    uint8_t *fb2;              /* не NULL: прямая запись в 2-bpp буфер (ctx->direct_fb2bpp) */
    int32_t width;             /* строка 2-bpp буфера, пикселей */
    gfx_color_t color;
    int32_t x0, y0;            /* область отсечения [x0, x1) x [y0, y1) */
    int32_t x1, y1;
} line_painter_t;

/* Отрезок короче этого выводится по пикселям: вызов fill_span на коротком отрезке не окупается. */
#ifndef GFX_LINE_SPAN_MIN
#define GFX_LINE_SPAN_MIN 6
#endif
/* Пологие линии короче этого по dx идут пиксель за пикселем (отрезки в них слишком короткие). */
#ifndef GFX_LINE_RUN_MIN_DX
#define GFX_LINE_RUN_MIN_DX 16
#endif

static void lp_init(line_painter_t *p, const gfx_context_t *ctx)
{
    p->put = ctx->draw_pixel;
    p->span = ctx->fill_span;
    p->fb = ctx->framebuffer;
    p->fb2 = NULL;
    p->width = ctx->width;
    p->color = ctx->color_fg;
    p->x0 = ctx->clip_x;
    p->y0 = ctx->clip_y;
    p->x1 = (int32_t)ctx->clip_x + ctx->clip_w;
    p->y1 = (int32_t)ctx->clip_y + ctx->clip_h;

    if (ctx->direct_fb2bpp && ctx->framebuffer != NULL) {
        /* Прямая запись обходит проверки границ draw_pixel: область - не шире экрана. */
        p->fb2 = (uint8_t *)ctx->framebuffer;
        if (p->x0 < 0) p->x0 = 0;
        if (p->y0 < 0) p->y0 = 0;
        if (p->x1 > ctx->width) p->x1 = ctx->width;
        if (p->y1 > ctx->height) p->y1 = ctx->height;
    }
}

/* Пиксель внутри области отсечения (и экрана): без проверок границ. */
static inline void lp_put(const line_painter_t *p, int32_t x, int32_t y)
{
    if (p->fb2 != NULL) {
        const uint32_t idx = (uint32_t)y * (uint32_t)p->width + (uint32_t)x;
        const unsigned shift = (3u - (idx & 3u)) << 1;
        p->fb2[idx >> 2] = (uint8_t)((p->fb2[idx >> 2] & ~(0x03u << shift)) |
                                     (((unsigned)p->color & 3u) << shift));
    } else {
        p->put(p->fb, (int16_t)x, (int16_t)y, p->color);
    }
}

static inline void lp_pixel(const line_painter_t *p, int32_t x, int32_t y)
{
    if (x >= p->x0 && x < p->x1 && y >= p->y0 && y < p->y1) {
        lp_put(p, x, y);
    }
}

/* Горизонтальный отрезок [xa, xb] (xa <= xb) в строке y с отсечением. */
static void lp_hspan(const line_painter_t *p, int32_t xa, int32_t xb, int32_t y)
{
    if (y < p->y0 || y >= p->y1) {
        return;
    }
    if (xa < p->x0) {
        xa = p->x0;
    }
    if (xb >= p->x1) {
        xb = p->x1 - 1;
    }
    if (xa > xb) {
        return;
    }
    if (p->span != NULL && xb - xa + 1 >= GFX_LINE_SPAN_MIN) {
        p->span(p->fb, (int16_t)xa, (int16_t)xb, (int16_t)y, p->color);
        return;
    }
    for (int32_t x = xa; x <= xb; x++) {
        lp_put(p, x, y);
    }
}

static inline int32_t abs32(int32_t v)
{
    return v < 0 ? -v : v;
}

/* Отрезок пикселей одной строки по ходу линии: от xa до xb в любом порядке. */
static inline void lp_run(const line_painter_t *p, int32_t xa, int32_t xb, int32_t y)
{
    if (xa <= xb) {
        lp_hspan(p, xa, xb, y);
    } else {
        lp_hspan(p, xb, xa, y);
    }
}

void gfx_draw_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (ctx == NULL || ctx->draw_pixel == NULL) return;

    // 1. Отсечение отрезка (Cohen-Sutherland)
    if (!clip_line(ctx, &x0, &y0, &x1, &y1)) {
        return;
    }

    // 2. Отрисовка видимой части (Bresenham)
    line_painter_t p;
    lp_init(&p, ctx);

    int32_t cx = x0;
    int32_t cy = y0;
    const int32_t ex = x1;
    const int32_t ey = y1;
    const int32_t dx = abs32(ex - cx);
    const int32_t sx = (cx < ex) ? 1 : -1;
    const int32_t dy = -abs32(ey - cy);
    const int32_t sy = (cy < ey) ? 1 : -1;
    int32_t err = dx + dy;

    if (p.span != NULL && dx >= GFX_LINE_RUN_MIN_DX && dx > -dy) {
        /* Длинная пологая линия: пиксели одной строки подряд выводятся отрезком. */
        int32_t run_x = cx;
        for (;;) {
            if (cx == ex && cy == ey) {
                lp_run(&p, run_x, cx, cy);
                break;
            }

            const int32_t e2 = err * 2;
            const bool step_y = (e2 <= dx);
            if (step_y) {
                lp_run(&p, run_x, cx, cy);
            }
            if (e2 >= dy) {
                err += dy;
                cx += sx;
            }
            if (step_y) {
                err += dx;
                cy += sy;
                run_x = cx;
            }
        }
        return;
    }

    for (;;) {
        lp_pixel(&p, cx, cy);
        if (cx == ex && cy == ey) {
            break;
        }

        const int32_t e2 = err * 2;
        if (e2 >= dy) {
            err += dy;
            cx += sx;
        }
        if (e2 <= dx) {
            err += dx;
            cy += sy;
        }
    }
}

void gfx_draw_dashed_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (ctx == NULL || ctx->draw_pixel == NULL) return;

    // Сохраняем геометрическое начало линии для синхронизации фазы штрихов
    int16_t orig_x0 = x0;
    int16_t orig_y0 = y0;

    // 1. Отсечение отрезка (Cohen-Sutherland)
    if (!clip_line(ctx, &x0, &y0, &x1, &y1)) {
        return;
    }

    // Параметры паттерна штриха
    const int16_t dash_len = 4;
    const int16_t gap_len = 4;
    const int16_t pattern_len = dash_len + gap_len; // 8 пикселей

    // Вычисляем количество пропущенных пикселей (Chebyshev distance)
    int16_t dx_clip = gfx_abs(x0 - orig_x0);
    int16_t dy_clip = gfx_abs(y0 - orig_y0);
    int16_t clipped_steps = (dx_clip > dy_clip) ? dx_clip : dy_clip;
    
    // Инициализируем счетчик с учетом отсеченной части.
    // Так как pattern_len = 8 (степень двойки), компилятор оптимизирует 
    // операцию деления с остатком (%) в побитовое И (& 7).
    int16_t dash_counter = clipped_steps & 7;

    // 2. Отрисовка видимой части (Bresenham optimized)
    int16_t dx = gfx_abs(x1 - x0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = -gfx_abs(y1 - y0);
    int16_t sy = y0 < y1 ? 1 : -1;

    int16_t err = dx + dy;

    while (true) {
        if (dash_counter < dash_len) {
            gfx_draw_pixel(ctx, x0, y0);
        }

        dash_counter++;
        if (dash_counter >= pattern_len) {
            dash_counter = 0;
        }

        if (x0 == x1 && y0 == y1) {
            break;
        }

        const int16_t e2 = (int16_t)(err * 2);

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

void gfx_draw_thick_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t thickness)
{
    if (ctx == NULL || thickness <= 0) return;

    if (thickness == 1) {
        gfx_draw_line(ctx, x0, y0, x1, y1);
        return;
    }

    if (ctx->draw_pixel == NULL) return;

    // Сохраняем исходные координаты для отрисовки "шапок" на стыках
    const int16_t orig_x0 = x0;
    const int16_t orig_y0 = y0;
    const int16_t orig_x1 = x1;
    const int16_t orig_y1 = y1;

    // 1. Отсечение отрезка (Cohen-Sutherland)
    if (!clip_line(ctx, &x0, &y0, &x1, &y1)) {
        return;
    }

    line_painter_t p;
    lp_init(&p, ctx);

    int32_t cx = x0;
    int32_t cy = y0;
    const int32_t ex = x1;
    const int32_t ey = y1;
    const int32_t dx = abs32(ex - cx);
    const int32_t sx = (cx < ex) ? 1 : -1;
    const int32_t dy = -abs32(ey - cy);
    const int32_t sy = (cy < ey) ? 1 : -1;
    int32_t err = dx + dy;

    // Флаг ориентации линии: если dx больше абсолютного dy, линия пологая
    const bool is_horizontal = (dx > -dy);

    // Смещения толщины поперёк линии: [lo, hi] (толщина 2: 0..1, 3: -1..1, иначе по центру).
    int32_t lo;
    int32_t hi;
    if (thickness == 2) {
        lo = 0;
        hi = 1;
    } else if (thickness == 3) {
        lo = -1;
        hi = 1;
    } else {
        lo = -(int32_t)(thickness >> 1);
        hi = lo + thickness - 1;
    }

    // Отрисовка концов ("caps"), если они не были отсечены (чтобы не было пробелов на стыках).
    if (thickness == 2 || thickness == 3) {
        const int32_t c_lo = (thickness == 2) ? 0 : -1;
        const int32_t c_hi = 1;
        if (x0 == orig_x0 && y0 == orig_y0) {
            for (int32_t oy = c_lo; oy <= c_hi; ++oy) {
                for (int32_t ox = c_lo; ox <= c_hi; ++ox) {
                    lp_pixel(&p, (int32_t)orig_x0 + ox, (int32_t)orig_y0 + oy);
                }
            }
        }
        if (x1 == orig_x1 && y1 == orig_y1) {
            for (int32_t oy = c_lo; oy <= c_hi; ++oy) {
                for (int32_t ox = c_lo; ox <= c_hi; ++ox) {
                    lp_pixel(&p, (int32_t)orig_x1 + ox, (int32_t)orig_y1 + oy);
                }
            }
        }
    }

    if (is_horizontal && p.span != NULL && dx >= GFX_LINE_RUN_MIN_DX) {
        /* Длинная пологая линия: каждая из thickness строк выводится отрезком на весь ход. */
        int32_t run_x = cx;
        for (;;) {
            const bool last = (cx == ex && cy == ey);
            const int32_t e2 = err * 2;
            const bool step_y = !last && (e2 <= dx);

            if (last || step_y) {
                for (int32_t off = lo; off <= hi; ++off) {
                    lp_run(&p, run_x, cx, cy + off);
                }
            }
            if (last) {
                break;
            }
            if (e2 >= dy) {
                err += dy;
                cx += sx;
            }
            if (step_y) {
                err += dx;
                cy += sy;
                run_x = cx;
            }
        }
        return;
    }

    for (;;) {
        if (is_horizontal) {
            for (int32_t off = lo; off <= hi; ++off) {
                lp_pixel(&p, cx, cy + off);
            }
        } else {
            lp_hspan(&p, cx + lo, cx + hi, cy);
        }

        if (cx == ex && cy == ey) {
            break;
        }

        const int32_t e2 = err * 2;
        if (e2 >= dy) {
            err += dy;
            cx += sx;
        }
        if (e2 <= dx) {
            err += dx;
            cy += sy;
        }
    }
}

void gfx_draw_dotted_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (ctx == NULL || ctx->draw_pixel == NULL) return;

    // Сохраняем геометрическое начало линии для синхронизации фазы
    int16_t orig_x0 = x0;
    int16_t orig_y0 = y0;

    // 1. Отсечение отрезка (Cohen-Sutherland)
    if (!clip_line(ctx, &x0, &y0, &x1, &y1)) {
        return;
    }

    // Вычисляем количество пропущенных пикселей (Chebyshev distance)
    int16_t dx_clip = gfx_abs(x0 - orig_x0);
    int16_t dy_clip = gfx_abs(y0 - orig_y0);
    int16_t clipped_steps = (dx_clip > dy_clip) ? dx_clip : dy_clip;

    // Инициализируем счетчик с учетом отсеченной части (по модулю 2, через & 1)
    int16_t dot_counter = clipped_steps & 1;

    // 2. Отрисовка видимой части (Bresenham optimized)
    int16_t dx = gfx_abs(x1 - x0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = -gfx_abs(y1 - y0);
    int16_t sy = y0 < y1 ? 1 : -1;

    int16_t err = dx + dy;

    while (true) {
        // Чередование: 1 пиксель рисуем, 1 пропускаем
        if (dot_counter == 0) {
            gfx_draw_pixel(ctx, x0, y0);
        }

        dot_counter = (dot_counter + 1) & 1;

        if (x0 == x1 && y0 == y1) {
            break;
        }

        const int16_t e2 = (int16_t)(err * 2);

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

void gfx_draw_railway_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (ctx == NULL || ctx->draw_pixel == NULL) return;

    gfx_color_t original_fg = ctx->color_fg;

    // Отрисовка сплошной 3-пиксельной черной линии
    ctx->color_fg = BLACK;
    gfx_draw_thick_line(ctx, x0, y0, x1, y1, 3);

    // Отрисовка 1-пиксельной белой прерывистой линии поверх
    ctx->color_fg = WHITE;
    gfx_draw_dashed_line(ctx, x0, y0, x1, y1);

    ctx->color_fg = original_fg;
}