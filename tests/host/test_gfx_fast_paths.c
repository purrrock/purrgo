/*
 * Быстрые пути без изменения результата:
 *   - gfx_fb2bpp_fill_span() и callback fill_span в gfx_draw_hline()/gfx_fill_rect()/
 *     gfx_fill_polygon() дают тот же 2-bpp framebuffer, что цепочка draw_pixel
 *     (раскладка как у display_set_pixel() в прошивке);
 *   - project_to_screen() с 32-битным делением совпадает с эталонным int64_t-вариантом
 *     на случайных и граничных аргументах, включая переход через 180 градусов.
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/gfx_fb2bpp.h"
#include "purrgo/gfx_renderer.h"
#include "purrgo/gfx_polygon.h"
#include "purrgo/gfx_rect.h"
#include "map_projection.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

#define W 176
#define H 264
#define FB_BYTES (W * H / 4)

/* ---- 2-bpp framebuffer: ровно как display_set_pixel()/display_get_pixel() ---- */
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

static uint32_t g_rng = 12345u;
static uint32_t rnd(void) { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }
static int16_t rnd_range(int lo, int hi) { return (int16_t)(lo + (int)(rnd() % (uint32_t)(hi - lo + 1))); }

/* Один и тот же случайный набор операций на двух контекстах: пиксельном и с fill_span. */
static void random_ops(gfx_context_t *g, uint32_t seed)
{
    g_rng = seed;
    for (int i = 0; i < 400; i++) {
        gfx_set_color(g, (gfx_color_t)(rnd() & 3), (gfx_color_t)(rnd() & 3));
        switch (rnd() % 5) {
        case 0:   /* отрезки, в том числе обратный порядок и выход за экран */
            gfx_draw_hline(g, rnd_range(-40, W + 40), rnd_range(-40, W + 40), rnd_range(-10, H + 10));
            break;
        case 1:   /* прямоугольники */
            gfx_fill_rect(g, rnd_range(-30, W), rnd_range(-30, H), rnd_range(-5, 120), rnd_range(-5, 80));
            break;
        case 2: { /* полигоны (заливка идёт через gfx_draw_hline) */
            gfx_point_t pts[6];
            int n = 3 + (int)(rnd() % 4);
            for (int k = 0; k < n; k++) { pts[k].x = rnd_range(-30, W + 30); pts[k].y = rnd_range(-30, H + 30); }
            gfx_fill_polygon(g, pts, (uint8_t)n);
            break;
        }
        case 3:   /* отсечение */
            gfx_set_clip(g, rnd_range(0, 60), rnd_range(0, 60), rnd_range(1, 120), rnd_range(1, 150));
            gfx_draw_hline(g, 0, W - 1, rnd_range(0, H - 1));
            gfx_fill_rect(g, 0, 0, W, H);
            gfx_reset_clip(g);
            break;
        default:  /* одиночные пиксели вперемешку с отрезками */
            gfx_draw_pixel(g, rnd_range(0, W - 1), rnd_range(0, H - 1));
            break;
        }
    }
}

static void test_fill_span_matches_pixels(void)
{
    static uint8_t fb_a[FB_BYTES], fb_b[FB_BYTES];

    for (uint32_t seed = 1; seed <= 40; seed++) {
        memset(fb_a, 0x1B, sizeof fb_a);     /* неоднородный фон */
        memset(fb_b, 0x1B, sizeof fb_b);

        gfx_context_t a, b;
        CHECK(gfx_init(&a, W, H, fb_a, cb_draw, cb_read));
        CHECK(gfx_init(&b, W, H, fb_b, cb_draw, cb_read));
        gfx_set_fill_span_callback(&b, cb_span);

        random_ops(&a, seed);
        random_ops(&b, seed);
        CHECK(memcmp(fb_a, fb_b, sizeof fb_a) == 0);
    }
}

/* Прямые проверки граничных случаев самой функции заливки. */
static void test_fb2bpp_edges(void)
{
    static uint8_t fb[FB_BYTES], ref[FB_BYTES];

    for (int16_t len = 1; len <= 9; len++) {
        for (int16_t x0 = 0; x0 < 12; x0++) {
            for (uint8_t color = 0; color < 4; color++) {
                memset(fb, 0xA5, sizeof fb);
                memset(ref, 0xA5, sizeof ref);
                int16_t x1 = (int16_t)(x0 + len - 1);
                for (int16_t x = x0; x <= x1; x++) { px_set(ref, x, 3, color); }
                gfx_fb2bpp_fill_span(fb, W, H, x0, x1, 3, color);
                CHECK(memcmp(fb, ref, sizeof fb) == 0);
            }
        }
    }

    /* Последняя строка, последний пиксель строки, весь экран по строкам. */
    memset(fb, 0, sizeof fb);
    memset(ref, 0, sizeof ref);
    for (int16_t y = 0; y < H; y++) {
        for (int16_t x = 0; x < W; x++) { px_set(ref, x, y, 2); }
        gfx_fb2bpp_fill_span(fb, W, H, W - 1, 0, y, 2);     /* x0 > x1: порядок не важен */
    }
    CHECK(memcmp(fb, ref, sizeof fb) == 0);

    /* За пределами экрана ничего не пишется и ничего не падает. */
    memset(fb, 0x33, sizeof fb);
    memset(ref, 0x33, sizeof ref);
    gfx_fb2bpp_fill_span(fb, W, H, 0, W - 1, -1, 3);
    gfx_fb2bpp_fill_span(fb, W, H, 0, W - 1, H, 3);
    gfx_fb2bpp_fill_span(fb, W, H, -50, -1, 5, 3);
    gfx_fb2bpp_fill_span(fb, W, H, W, W + 50, 5, 3);
    gfx_fb2bpp_fill_span(NULL, W, H, 0, 10, 5, 3);
    CHECK(memcmp(fb, ref, sizeof fb) == 0);
    gfx_fb2bpp_fill_span(fb, W, H, -20, 3, 7, 1);           /* частично за левым краем */
    for (int16_t x = 0; x <= 3; x++) { px_set(ref, x, 7, 1); }
    gfx_fb2bpp_fill_span(fb, W, H, W - 3, W + 20, 7, 1);    /* частично за правым краем */
    for (int16_t x = W - 3; x < W; x++) { px_set(ref, x, 7, 1); }
    CHECK(memcmp(fb, ref, sizeof fb) == 0);
}

/* ---- project_to_screen: эталон - прежняя реализация на int64_t ---- */
static void ref_project(int32_t lon, int32_t lat, const purrgo_bbox_t *cam,
                        const purrgo_viewport_t *vp, int16_t *sx, int16_t *sy)
{
    int64_t dx_raw = (int64_t)lon - (int64_t)cam->min_x;
    if (cam->min_x > cam->max_x && dx_raw < 0) { dx_raw += 3600000000LL; }
    int64_t dx = dx_raw * (int64_t)vp->width;
    int64_t width = camera_span_x(cam);
    int64_t px = (width > 0) ? (dx / width) : 0;
    px += (int64_t)vp->offset_x;
    int64_t dy = ((int64_t)lat - (int64_t)cam->min_y) * (int64_t)vp->height;
    int64_t height = (int64_t)cam->max_y - (int64_t)cam->min_y;
    int64_t py = (height > 0) ? ((int64_t)vp->height - (dy / height)) : 0;
    py += (int64_t)vp->offset_y;
    if (px < INT16_MIN) px = INT16_MIN;
    if (px > INT16_MAX) px = INT16_MAX;
    if (py < INT16_MIN) py = INT16_MIN;
    if (py > INT16_MAX) py = INT16_MAX;
    *sx = (int16_t)px;
    *sy = (int16_t)py;
}

static int32_t rnd32(void) { return (int32_t)((rnd() << 8) ^ rnd()); }

static void check_cam(const purrgo_bbox_t *cam, const purrgo_viewport_t *vp, int points)
{
    int bad = 0;
    for (int i = 0; i < points; i++) {
        int32_t lon, lat;
        switch (i % 4) {
        case 0: lon = rnd32() % 1800000000; lat = rnd32() % 900000000; break;               /* весь мир */
        case 1: lon = cam->min_x + (rnd32() % 200000);  lat = cam->min_y + (rnd32() % 200000); break;   /* рядом с камерой */
        case 2: lon = (rnd32() % 2) ? 1800000000 : -1800000000; lat = (rnd32() % 2) ? 900000000 : -900000000; break;  /* края */
        default: lon = cam->max_x + (rnd32() % 100); lat = cam->max_y - (rnd32() % 100); break;
        }
        int16_t ax, ay, bx, by;
        project_to_screen(lon, lat, cam, vp, &ax, &ay);
        ref_project(lon, lat, cam, vp, &bx, &by);
        if (ax != bx || ay != by) { bad++; }
    }
    CHECK(bad == 0);
}

static void test_projection_matches_reference(void)
{
    purrgo_viewport_t vp = { .width = 176, .height = 246, .offset_x = 0, .offset_y = 9 };

    /* Обычная камера (500 м), камера через 180-й меридиан, камера огромного размера, вырожденные. */
    purrgo_bbox_t cams[] = {
        { .min_x = 354700000,  .max_x = 354760000,  .min_y = 315500000, .max_y = 315590000 },
        { .min_x = 1799990000, .max_x = -1799990000, .min_y = 100000000, .max_y = 100090000 },
        { .min_x = -1800000000, .max_x = 1800000000, .min_y = -900000000, .max_y = 900000000 },
        { .min_x = 100, .max_x = 100, .min_y = 50, .max_y = 50 },
        { .min_x = -2000000000, .max_x = 2000000000, .min_y = -2000000000, .max_y = 2000000000 },
    };
    for (size_t c = 0; c < sizeof cams / sizeof cams[0]; c++) {
        g_rng = (uint32_t)(777 + c);
        check_cam(&cams[c], &vp, 20000);
    }

    /* Другие размеры области. */
    purrgo_viewport_t vp2 = { .width = 264, .height = 176, .offset_x = 5, .offset_y = 0 };
    g_rng = 99;
    check_cam(&cams[0], &vp2, 20000);
}

int main(void)
{
    test_fb2bpp_edges();
    test_fill_span_matches_pixels();
    test_projection_matches_reference();
    return test_finish("test_gfx_fast_paths");
}
