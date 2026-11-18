/*
 * Ускоренные gfx_draw_line() / gfx_draw_thick_line() (и штриховая, точечная и железнодорожная
 * линии, которые их используют) закрашивают ровно те же пиксели, что прежняя реализация
 * (tests/host/gfx_line_ref.c): случайные отрезки с выходом за экран и произвольной областью
 * отсечения, толщины 1..7, контекст с fill_span и без него.
 * Время работы старой и новой реализации печатается для информации (на PC, к скорости на
 * Cortex-M33 отношения не имеют, но число и порядок пикселей те же).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "purrgo/gfx_fb2bpp.h"
#include "purrgo/gfx_line.h"
#include "purrgo/gfx_renderer.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

void ref_gfx_draw_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void ref_gfx_draw_thick_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t thickness);
void ref_gfx_draw_dashed_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void ref_gfx_draw_dotted_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void ref_gfx_draw_railway_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1);

#define W 176
#define H 264
#define FB_BYTES (W * H / 4)

static void px_set(uint8_t *fb, int16_t x, int16_t y, uint8_t color)
{
    if (x < 0 || x >= W || y < 0 || y >= H) { return; }
    int idx = y * W + x;
    int shift = (3 - (idx & 3)) << 1;
    fb[idx >> 2] = (uint8_t)((fb[idx >> 2] & ~(0x03 << shift)) | ((color & 3) << shift));
}
static uint8_t px_get(const uint8_t *fb, int16_t x, int16_t y)
{
    if (x < 0 || x >= W || y < 0 || y >= H) { return 0; }
    int idx = y * W + x;
    return (uint8_t)((fb[idx >> 2] >> ((3 - (idx & 3)) << 1)) & 3);
}
static void cb_draw(void *fb, int16_t x, int16_t y, gfx_color_t c) { px_set((uint8_t *)fb, x, y, (uint8_t)c); }
static gfx_color_t cb_read(void *fb, int16_t x, int16_t y) { return px_get((const uint8_t *)fb, x, y); }
static void cb_span(void *fb, int16_t x0, int16_t x1, int16_t y, gfx_color_t c)
{
    gfx_fb2bpp_fill_span((uint8_t *)fb, W, H, x0, x1, y, (uint8_t)c);
}

static uint32_t g_rng = 1u;
static uint32_t rnd(void) { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }
static int16_t rnd_range(int lo, int hi) { return (int16_t)(lo + (int)(rnd() % (uint32_t)(hi - lo + 1))); }

typedef enum { K_LINE, K_THICK, K_DASHED, K_DOTTED, K_RAILWAY, K_COUNT } kind_t;

static void draw(gfx_context_t *g, kind_t k, int ref, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t t)
{
    switch (k) {
    case K_LINE:    ref ? ref_gfx_draw_line(g, x0, y0, x1, y1) : gfx_draw_line(g, x0, y0, x1, y1); break;
    case K_THICK:   ref ? ref_gfx_draw_thick_line(g, x0, y0, x1, y1, t) : gfx_draw_thick_line(g, x0, y0, x1, y1, t); break;
    case K_DASHED:  ref ? ref_gfx_draw_dashed_line(g, x0, y0, x1, y1) : gfx_draw_dashed_line(g, x0, y0, x1, y1); break;
    case K_DOTTED:  ref ? ref_gfx_draw_dotted_line(g, x0, y0, x1, y1) : gfx_draw_dotted_line(g, x0, y0, x1, y1); break;
    default:        ref ? ref_gfx_draw_railway_line(g, x0, y0, x1, y1) : gfx_draw_railway_line(g, x0, y0, x1, y1); break;
    }
}

/* Случайный отрезок: разной длины и наклона, в том числе почти горизонтальный и вертикальный. */
static void rnd_segment(int16_t *x0, int16_t *y0, int16_t *x1, int16_t *y1)
{
    *x0 = rnd_range(-60, W + 60);
    *y0 = rnd_range(-60, H + 60);
    switch (rnd() % 5) {
    case 0:  *x1 = rnd_range(-60, W + 60); *y1 = rnd_range(-60, H + 60); break;      /* произвольный */
    case 1:  *x1 = (int16_t)(*x0 + rnd_range(-200, 200)); *y1 = (int16_t)(*y0 + rnd_range(-6, 6)); break;   /* пологий */
    case 2:  *x1 = (int16_t)(*x0 + rnd_range(-6, 6)); *y1 = (int16_t)(*y0 + rnd_range(-200, 200)); break;   /* крутой */
    case 3:  *x1 = (int16_t)(*x0 + rnd_range(-25, 25)); *y1 = (int16_t)(*y0 + rnd_range(-25, 25)); break;   /* короткий */
    default: *x1 = (int16_t)(*x0 + rnd_range(-120, 120)); *y1 = *y0; break;                 /* горизонтальный */
    }
}

static void test_equivalence(int with_span, int direct)
{
    static uint8_t fb_a[FB_BYTES], fb_b[FB_BYTES];
    gfx_context_t a, b;
    CHECK(gfx_init(&a, W, H, fb_a, cb_draw, cb_read));
    CHECK(gfx_init(&b, W, H, fb_b, cb_draw, cb_read));
    if (with_span) {
        gfx_set_fill_span_callback(&a, cb_span);
        gfx_set_fill_span_callback(&b, cb_span);
    }
    gfx_set_direct_fb2bpp(&b, direct != 0);   /* эталон a всегда через draw_pixel */

    int mismatches = 0;
    g_rng = 777u + (uint32_t)(with_span * 10 + direct);

    for (int iter = 0; iter < 30000; iter++) {
        const kind_t k = (kind_t)(rnd() % K_COUNT);
        const int16_t t = (int16_t)(1 + rnd() % 7);
        int16_t x0, y0, x1, y1;
        rnd_segment(&x0, &y0, &x1, &y1);

        /* Периодически другая область отсечения и новая картинка. */
        if (iter % 50 == 0) {
            memset(fb_a, 0, sizeof fb_a);
            memset(fb_b, 0, sizeof fb_b);
            if (rnd() % 3 == 0) {
                gfx_reset_clip(&a);
                gfx_reset_clip(&b);
            } else {
                const int16_t cx = rnd_range(0, W - 20), cy = rnd_range(0, H - 20);
                const int16_t cw = rnd_range(10, W - cx), ch = rnd_range(10, H - cy);
                gfx_set_clip(&a, cx, cy, cw, ch);
                gfx_set_clip(&b, cx, cy, cw, ch);
            }
        }

        const gfx_color_t c = (gfx_color_t)(1 + rnd() % 3);
        gfx_set_color(&a, c, 0);
        gfx_set_color(&b, c, 0);
        draw(&a, k, 1, x0, y0, x1, y1, t);
        draw(&b, k, 0, x0, y0, x1, y1, t);

        if (memcmp(fb_a, fb_b, sizeof fb_a) != 0) {
            if (mismatches < 5) {
                printf("MISMATCH kind=%d t=%d (%d,%d)-(%d,%d) span=%d\n", k, t, x0, y0, x1, y1, with_span);
            }
            mismatches++;
            memcpy(fb_b, fb_a, sizeof fb_a);   /* продолжаем с общей картинкой */
        }
    }
    CHECK(mismatches == 0);
}

static void test_edge_cases(void)
{
    static uint8_t fb_a[FB_BYTES], fb_b[FB_BYTES];
    gfx_context_t a, b;
    CHECK(gfx_init(&a, W, H, fb_a, cb_draw, cb_read));
    CHECK(gfx_init(&b, W, H, fb_b, cb_draw, cb_read));
    gfx_set_fill_span_callback(&a, cb_span);
    gfx_set_fill_span_callback(&b, cb_span);
    gfx_set_color(&a, 3, 0);
    gfx_set_color(&b, 3, 0);

    /* Вырожденные и граничные отрезки: точка, горизонталь и вертикаль на краях экрана и вне его. */
    static const int16_t seg[][4] = {
        {10, 10, 10, 10}, {0, 0, W - 1, 0}, {0, H - 1, W - 1, H - 1}, {0, 0, 0, H - 1},
        {W - 1, 0, W - 1, H - 1}, {-100, 5, 300, 5}, {5, -100, 5, 400}, {0, 0, W - 1, H - 1},
        {W - 1, H - 1, 0, 0}, {-50, -50, 250, 20}, {100, 100, 30, 101}, {30, 101, 100, 100},
        {-5, 40, 200, 42}, {200, 42, -5, 40},
    };
    for (size_t i = 0; i < sizeof seg / sizeof seg[0]; i++) {
        for (int t = 1; t <= 7; t++) {
            for (kind_t k = K_LINE; k < K_COUNT; k++) {
                draw(&a, k, 1, seg[i][0], seg[i][1], seg[i][2], seg[i][3], (int16_t)t);
                draw(&b, k, 0, seg[i][0], seg[i][1], seg[i][2], seg[i][3], (int16_t)t);
                CHECK(memcmp(fb_a, fb_b, sizeof fb_a) == 0);
                memcpy(fb_b, fb_a, sizeof fb_a);
            }
        }
    }
}

static double bench(int ref, int with_span, int mode, int direct)
{
    static uint8_t fb[FB_BYTES];
    gfx_context_t g;
    gfx_init(&g, W, H, fb, cb_draw, cb_read);
    if (with_span) { gfx_set_fill_span_callback(&g, cb_span); }
    gfx_set_direct_fb2bpp(&g, direct != 0);
    gfx_set_color(&g, 3, 0);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    g_rng = 99u;
    for (int i = 0; i < 400000; i++) {
        int16_t x0 = rnd_range(0, W - 1), y0 = rnd_range(0, H - 1), x1, y1;
        if (mode == 0) {         /* короткие отрезки карты: 3-30 пикселей, толщина 2-3 */
            x1 = (int16_t)(x0 + rnd_range(-30, 30)); y1 = (int16_t)(y0 + rnd_range(-30, 30));
            draw(&g, K_THICK, ref, x0, y0, x1, y1, (int16_t)(2 + (i & 1)));
        } else {                 /* длинные пологие линии */
            x1 = (int16_t)(x0 + rnd_range(-150, 150)); y1 = (int16_t)(y0 + rnd_range(-8, 8));
            draw(&g, K_THICK, ref, x0, y0, x1, y1, 3);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (double)(t1.tv_sec - t0.tv_sec) * 1e3 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6;
}

static void print_bench(void)
{
    static const char *names[] = { "короткие толстые (2-3 px)", "длинные пологие толстые" };
    for (int mode = 0; mode < 2; mode++) {
        const double old_ms = bench(1, 1, mode, 0);
        const double new_ms = bench(0, 1, mode, 0);
        const double dir_ms = bench(0, 1, mode, 1);
        printf("  %-26s было %.0f мс, стало %.0f мс (x%.2f), с прямой записью %.0f мс (x%.2f), только PC\n",
               names[mode], old_ms, new_ms, old_ms / new_ms, dir_ms, old_ms / dir_ms);
    }
}

int main(void)
{
    test_equivalence(1, 0);
    test_equivalence(0, 0);
    test_equivalence(1, 1);   /* прямая запись в 2-bpp буфер, как на STM32 */
    test_equivalence(0, 1);
    test_edge_cases();
    print_bench();
    return test_finish("test_gfx_line_equiv");
}
