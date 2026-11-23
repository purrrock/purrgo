/*
 * gfx_draw_thick_line_halo(): обводка за один проход.
 * Для случайных отрезков (в том числе выходящих за экран, с произвольной областью
 * отсечения) проверяется:
 *  - обводка закрашивает всё, что закрашивает gfx_draw_thick_line(thickness);
 *  - обводка закрашивает всех соседей каждого пикселя линии на манхэттенском расстоянии
 *    <= halo, лежащих в области отсечения (кайма без дыр при любом наклоне; диагональный
 *    угол ступеньки при halo = 1 не закрашивается - так и задумано). У самой границы
 *    области отсечения (ближе halo + 2 пикселей) не проверяется: конец отрезка там обрезан
 *    clip_line без шапки, и у ядра, и у обводки;
 *  - обводка не выходит дальше halo пикселей от линии (по Чебышёву);
 *  - halo = 0 даёт тот же набор пикселей, что gfx_draw_thick_line (толщины 2 и 3).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include "purrgo/gfx_line.h"
#include "purrgo/gfx_renderer.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

#define W 96
#define H 80

static uint8_t g_fb[W * H];

static void cb_draw(void *fb, int16_t x, int16_t y, gfx_color_t c)
{
    if (x < 0 || x >= W || y < 0 || y >= H) { return; }
    ((uint8_t *)fb)[y * W + x] = (uint8_t)c;
}
static gfx_color_t cb_read(void *fb, int16_t x, int16_t y)
{
    if (x < 0 || x >= W || y < 0 || y >= H) { return 0; }
    return ((uint8_t *)fb)[y * W + x];
}

static uint32_t g_rng = 1u;
static uint32_t rnd(void) { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }
static int rnd_range(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }

static int at(const uint8_t *fb, int x, int y)
{
    return (x >= 0 && x < W && y >= 0 && y < H) ? fb[y * W + x] != 0 : 0;
}

static void test_halo(int thickness, int halo)
{
    static uint8_t core[W * H], cas[W * H];
    gfx_context_t g;
    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));
    gfx_set_color(&g, 1, 0);
    g_rng = 4242u + (uint32_t)(thickness * 10 + halo);

    int bad_super = 0, bad_nb = 0, bad_far = 0;

    for (int iter = 0; iter < 20000; iter++) {
        int x0 = rnd_range(-30, W + 30), y0 = rnd_range(-30, H + 30), x1, y1;
        switch (rnd() % 4) {
        case 0:  x1 = rnd_range(-30, W + 30); y1 = rnd_range(-30, H + 30); break;
        case 1:  x1 = x0 + rnd_range(-60, 60); y1 = y0 + rnd_range(-4, 4); break;
        case 2:  x1 = x0 + rnd_range(-4, 4); y1 = y0 + rnd_range(-60, 60); break;
        default: x1 = x0 + rnd_range(-12, 12); y1 = y0 + rnd_range(-12, 12); break;
        }

        int cx = 0, cy = 0, cw = W, ch = H;
        if (rnd() % 3 == 0) {
            cx = rnd_range(0, W - 20); cy = rnd_range(0, H - 20);
            cw = rnd_range(10, W - cx); ch = rnd_range(10, H - cy);
        }
        gfx_set_clip(&g, (int16_t)cx, (int16_t)cy, (int16_t)cw, (int16_t)ch);

        memset(g_fb, 0, sizeof g_fb);
        gfx_draw_thick_line(&g, (int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1, (int16_t)thickness);
        memcpy(core, g_fb, sizeof core);

        memset(g_fb, 0, sizeof g_fb);
        gfx_draw_thick_line_halo(&g, (int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1,
                                 (int16_t)thickness, (int16_t)halo);
        memcpy(cas, g_fb, sizeof cas);

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                const int in_clip = (x >= cx && x < cx + cw && y >= cy && y < cy + ch);
                const int c = at(core, x, y);
                const int h = at(cas, x, y);

                if (c && !h) { bad_super++; }

                const int interior = (x >= cx + halo + 2 && x < cx + cw - halo - 2 &&
                                      y >= cy + halo + 2 && y < cy + ch - halo - 2);
                if (c && interior) {
                    for (int dy = -halo; dy <= halo; dy++) {
                        for (int dx = -halo; dx <= halo; dx++) {
                            const int nx = x + dx, ny = y + dy;
                            if (abs(dx) + abs(dy) > halo) { continue; }
                            const int n_in = (nx >= cx && nx < cx + cw && ny >= cy && ny < cy + ch);
                            if (n_in && !at(cas, nx, ny)) { bad_nb++; }
                        }
                    }
                }

                if (h && in_clip) {
                    int near = 0;
                    for (int dy = -halo; dy <= halo && !near; dy++) {
                        for (int dx = -halo; dx <= halo; dx++) {
                            if (at(core, x + dx, y + dy)) { near = 1; break; }
                        }
                    }
                    /* Шапки на неотсечённых концах выходят за ядро: ядро их тоже рисует
                     * только для толщин 2 и 3, поэтому для других толщин допускается запас. */
                    if (!near && (thickness == 2 || thickness == 3)) { bad_far++; }
                }
            }
        }
        if (halo == 0) {
            CHECK(memcmp(core, cas, sizeof core) == 0 || (thickness != 2 && thickness != 3));
        }
    }
    if (bad_super || bad_nb || bad_far) {
        printf("thickness=%d halo=%d: super=%d neighbours=%d far=%d\n",
               thickness, halo, bad_super, bad_nb, bad_far);
    }
    CHECK(bad_super == 0);
    CHECK(bad_nb == 0);
    CHECK(bad_far == 0);
}

int main(void)
{
    test_halo(2, 1);
    test_halo(3, 1);
    test_halo(2, 2);
    test_halo(2, 0);
    test_halo(3, 0);
    return test_finish("test_gfx_line_halo");
}
