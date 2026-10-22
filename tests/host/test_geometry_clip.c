/*
 * Геометрия далёких вершин и заливка полигонов (патч 0023):
 *   - project_to_screen_i32() не насыщается в int16 и совпадает с project_to_screen()
 *     там, где результат помещается в int16;
 *   - map_clip_segment_to_safe() сохраняет наклон отрезка;
 *   - gfx_fill_compound_polygon() с вершинами в миллионах пикселей даёт тот же
 *     результат, что независимый эталон (чётно-нечётное правило, int64, qsort);
 *   - scanline с числом пересечений больше GFX_MAX_POLYGON_NODES (128) заливается
 *     целиком, а не обрывается на середине.
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/gfx_renderer.h"
#include "purrgo/gfx_polygon.h"
#include "map_projection.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

#define MAX_W 1024
#define MAX_H 64

static int g_w = 64, g_h = 48;
static uint8_t g_fb[MAX_W * MAX_H];

static void cb_draw(void *fb, int16_t x, int16_t y, gfx_color_t c)
{
    if (x >= 0 && x < g_w && y >= 0 && y < g_h) { ((uint8_t *)fb)[y * g_w + x] = (uint8_t)c; }
}
static gfx_color_t cb_read(void *fb, int16_t x, int16_t y)
{
    if (x >= 0 && x < g_w && y >= 0 && y < g_h) { return ((uint8_t *)fb)[y * g_w + x]; }
    return 0;
}

static gfx_context_t g_gfx;

static void canvas(int w, int h)
{
    g_w = w; g_h = h;
    memset(g_fb, 0, sizeof g_fb);
    (void)gfx_init(&g_gfx, (int16_t)w, (int16_t)h, g_fb, cb_draw, cb_read);
    gfx_set_color(&g_gfx, 1, 0);
}

/* ------------------------------------------------------------ эталон --- */

static int cmp64(const void *a, const void *b)
{
    long long x = *(const long long *)a, y = *(const long long *)b;
    return (x > y) - (x < y);
}

static void ref_fill(const gfx_point_t *p, int n, const uint32_t *parts, int np, uint8_t *out)
{
    memset(out, 0, sizeof g_fb);
    for (int y = 0; y < g_h; y++) {
        long long xs[2048];
        int cnt = 0;
        for (int r = 0; r < np; r++) {
            int s = (int)parts[r], e = (r + 1 < np) ? (int)parts[r + 1] : n;
            if (e - s < 3) { continue; }
            for (int i = s; i < e; i++) {
                int j = (i == e - 1) ? s : i + 1;
                long long y0 = p[i].y, y1 = p[j].y;
                if (y0 == y1) { continue; }
                if (!((y0 <= y && y1 > y) || (y1 <= y && y0 > y))) { continue; }
                long long x0 = p[i].x, x1 = p[j].x;
                xs[cnt++] = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
            }
        }
        qsort(xs, (size_t)cnt, sizeof xs[0], cmp64);
        for (int k = 0; k + 1 < cnt; k += 2) {
            long long a = xs[k], b = xs[k + 1];
            if (a > b) { long long t = a; a = b; b = t; }
            for (long long x = a < 0 ? 0 : a; x <= b && x < g_w; x++) { out[y * g_w + x] = 1; }
        }
    }
}

static int diff_count(const uint8_t *a, const uint8_t *b)
{
    int d = 0;
    for (int i = 0; i < g_w * g_h; i++) { d += (a[i] != b[i]); }
    return d;
}

static uint32_t g_rng = 987654321u;
static uint32_t rnd(void) { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }
static int32_t rnd_range(int32_t lo, int32_t hi) { return lo + (int32_t)(rnd() % (uint32_t)(hi - lo + 1)); }

static uint8_t g_ref[MAX_W * MAX_H];

/* ------------------------------------------------------------- тесты --- */

static void test_projection_i32(void)
{
    purrgo_bbox_t cam = { 0, 0, 1000, 1000 };
    purrgo_viewport_t vp = { 176, 246, 0, 9 };
    int32_t pts[][2] = { {0, 0}, {500, 500}, {1000, 1000}, {-300, 2000}, {7000000, -9000000} };

    for (unsigned i = 0; i < sizeof pts / sizeof pts[0]; i++) {
        int16_t a, b; int32_t c, d;
        project_to_screen(pts[i][0], pts[i][1], &cam, &vp, &a, &b);
        project_to_screen_i32(pts[i][0], pts[i][1], &cam, &vp, &c, &d);
        if (c >= INT16_MIN && c <= INT16_MAX && d >= INT16_MIN && d <= INT16_MAX) {
            CHECK(a == c && b == d);
        }
    }

    /* Далёкая точка: int16-вариант насыщается, i32 - нет. */
    int16_t a, b; int32_t c, d;
    project_to_screen(7000000, -9000000, &cam, &vp, &a, &b);
    project_to_screen_i32(7000000, -9000000, &cam, &vp, &c, &d);
    CHECK(a == INT16_MAX && b == INT16_MAX);
    CHECK(c == 1232000 && d == 2214255);

    /* Предельный случай: узкая камера и точка на другом конце света. */
    purrgo_bbox_t narrow = { 0, 0, 1, 1 };
    project_to_screen_i32(1800000000, -900000000, &narrow, &vp, &c, &d);
    CHECK(c == (int32_t)PURRGO_PROJ_LIMIT && d == (int32_t)PURRGO_PROJ_LIMIT);
}

static void test_segment_clip(void)
{
    int32_t x0 = 10, y0 = 20, x1 = 100, y1 = 50;
    CHECK(map_clip_segment_to_safe(&x0, &y0, &x1, &y1));
    CHECK(x0 == 10 && y0 == 20 && x1 == 100 && y1 == 50);   /* внутри - без изменений */

    x0 = 10; y0 = 10; x1 = 1000000; y1 = 500000;
    CHECK(map_clip_segment_to_safe(&x0, &y0, &x1, &y1));
    CHECK(x0 == 10 && y0 == 10);
    CHECK(x1 <= PURRGO_SAFE_COORD && y1 <= PURRGO_SAFE_COORD);
    /* точка на исходной прямой y = 10 + (x - 10) / 2 с точностью до пикселя */
    long long expect_y = 10 + (x1 - 10) / 2;
    CHECK(llabs((long long)y1 - expect_y) <= 1);

    x0 = 200000; y0 = 5; x1 = 300000; y1 = 9;               /* целиком вне области */
    CHECK(!map_clip_segment_to_safe(&x0, &y0, &x1, &y1));

    x0 = -2000000; y0 = 0; x1 = 2000000; y1 = 8;            /* пересекает область насквозь */
    CHECK(map_clip_segment_to_safe(&x0, &y0, &x1, &y1));
    CHECK(x0 == -PURRGO_SAFE_COORD && x1 == PURRGO_SAFE_COORD);
    CHECK(llabs((long long)y0 - 4 + 0) <= 4 && y1 - y0 >= 0);
}

static void test_far_vertex_triangle(void)
{
    canvas(64, 48);
    /* Вершина C далеко: настоящий наклон ребра A-C - 2 по x на 1 по y. */
    gfx_point_t tri[3] = { {10, 10}, {10, 40}, {200000, 100000} };
    uint32_t parts[1] = {0};

    gfx_fill_compound_polygon(&g_gfx, tri, 3, parts, 1);
    ref_fill(tri, 3, parts, 1, g_ref);
    CHECK(diff_count(g_fb, g_ref) == 0);

    /* Насыщение вершины в int16 (как было) даёт другой рисунок: тест чувствителен. */
    gfx_point_t sat[3] = { {10, 10}, {10, 40}, {32767, 32767} };
    uint8_t saved[MAX_W * MAX_H];
    memcpy(saved, g_fb, sizeof saved);
    canvas(64, 48);
    gfx_fill_compound_polygon(&g_gfx, sat, 3, parts, 1);
    CHECK(diff_count(g_fb, saved) > 0);
}

static void test_random_polygons(void)
{
    int mismatches = 0;

    for (int iter = 0; iter < 400; iter++) {
        gfx_point_t pts[48];
        uint32_t parts[3] = {0, 0, 0};
        int n = 3 + (int)(rnd() % 20);
        int np = 1;

        if (iter % 3 == 0) {                       /* два кольца: внешнее и дыра */
            np = 2;
            parts[1] = (uint32_t)(n / 2 < 3 ? 3 : n / 2);
            if ((int)parts[1] > n - 3) { n = (int)parts[1] + 3; }
        }
        for (int k = 0; k < n; k++) {
            int far = (rnd() % 4 == 0);
            pts[k].x = far ? rnd_range(-(1 << 28), 1 << 28) : rnd_range(-20, 84);
            pts[k].y = far ? rnd_range(-(1 << 28), 1 << 28) : rnd_range(-20, 68);
        }
        canvas(64, 48);
        gfx_fill_compound_polygon(&g_gfx, pts, (uint16_t)n, parts, (uint16_t)np);
        ref_fill(pts, n, parts, np, g_ref);
        if (diff_count(g_fb, g_ref) != 0) { mismatches++; }
    }
    CHECK(mismatches == 0);
}

/* Зигзаг на 2*teeth рёбер: каждая scanline пересекает их все. */
static void zigzag_case(int teeth)
{
    gfx_point_t pts[1200];
    int n = 2 * teeth;
    uint32_t parts[1] = {0};

    CHECK(n <= 1200);
    for (int k = 0; k < teeth; k++) {
        pts[2 * k].x = 3 * k;        pts[2 * k].y = 5;
        pts[2 * k + 1].x = 3 * k + 1; pts[2 * k + 1].y = 30;
    }
    canvas(MAX_W, MAX_H);
    gfx_fill_compound_polygon(&g_gfx, pts, (uint16_t)n, parts, 1);
    ref_fill(pts, n, parts, 1, g_ref);

    CHECK(diff_count(g_fb, g_ref) == 0);

    /* Заливка дошла до последней строки полигона и до правого края (не оборвана). */
    int rows_with_ink = 0;
    for (int y = 5; y < 30; y++) {
        for (int x = 0; x < g_w; x++) { if (g_fb[y * g_w + x]) { rows_with_ink++; break; } }
    }
    CHECK(rows_with_ink == 25);
}

static void test_many_crossings(void)
{
    zigzag_case(40);    /* 80 пересечений: быстрый путь */
    zigzag_case(70);    /* 140 > 128: запасной путь, 3 порции */
    zigzag_case(200);   /* 400: 7 порций */
    zigzag_case(330);   /* 660 пересечений: больше PURRGO_MAP_MAX_POINTS */
}

static void test_many_crossings_far(void)
{
    /* Запасной путь вместе с далёкими вершинами. */
    gfx_point_t pts[300];
    uint32_t parts[1] = {0};
    int n = 300;

    for (int k = 0; k < n; k++) {
        pts[k].x = (k % 2) ? 3 * k : 3 * k;
        pts[k].y = (k % 2) ? 40 : 3;
    }
    pts[150].x = 5000000; pts[150].y = 900000;
    pts[151].x = -5000000; pts[151].y = -700000;

    canvas(MAX_W, MAX_H);
    gfx_fill_compound_polygon(&g_gfx, pts, (uint16_t)n, parts, 1);
    ref_fill(pts, n, parts, 1, g_ref);
    CHECK(diff_count(g_fb, g_ref) == 0);
}

int main(void)
{
    test_projection_i32();
    test_segment_clip();
    test_far_vertex_triangle();
    test_random_polygons();
    test_many_crossings();
    test_many_crossings_far();
    return test_finish("test_geometry_clip");
}
