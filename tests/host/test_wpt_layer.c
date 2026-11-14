/*
 * Host-тесты слоя карты Waypoints (wpt_layer.c):
 *   - порядок слоёв и ключ LAYER_WAYPOINT в PURRGO.CFG;
 *   - точки <wpt> выбранного маршрута и всех *.gpx каталога WAYPTS;
 *   - фильтр имён (расширение, скрытые файлы), закрытие каталога и файлов;
 *   - кэш: повторный sync без чтения SD, перезагрузка при смене маршрута;
 *   - предел PURRGO_WPT_LAYER_MAX и приоритет точек маршрута;
 *   - отрисовка иконки PURRGO_WPT_LAYER_ICON (положение, отсечение вне кадра).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "purrgo/config.h"
#include "purrgo/gfx_renderer.h"
#include "purrgo/purrgo_poi_icons.h"
#include "purrgo/wpt_layer.h"
#include "fake_fs.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

static const char *CFG = "PURRGO.CFG";

/* ----------------------------------------------------------- помощники --- */

/* Дописывает в buf n точек <wpt>: широта 53.<base+i>, долгота 28.0000000. */
static size_t add_wpts(char *buf, size_t size, size_t used, int base, int n)
{
    for (int i = 0; i < n; i++) {
        const int lat_frac = base + i;      /* 1e-7 градуса после "53." */
        used += (size_t)snprintf(buf + used, size - used,
                                 "<wpt lat=\"53.%07d\" lon=\"28.0000000\"><name>P%d</name></wpt>\n",
                                 lat_frac, base + i);
    }
    return used;
}

static void put_gpx(const char *path, int base, int n)
{
    static char buf[16384];
    size_t used = (size_t)snprintf(buf, sizeof buf, "<gpx>\n");

    used = add_wpts(buf, sizeof buf, used, base, n);
    snprintf(buf + used, sizeof buf - used, "</gpx>\n");
    fake_fs_put(path, buf);
}

static void reset_all(void)
{
    fake_fs_reset();
    purrgo_config_init();
    purrgo_wpt_layer_clear();
}

/* ---------------------------------------------------------------- слои --- */

static void test_layer_order_and_config(void)
{
    CHECK(PURRGO_LAYER_WAYPOINT == PURRGO_LAYER_ROUTE + 1);
    CHECK(PURRGO_LAYER_TRACK == PURRGO_LAYER_WAYPOINT + 1);
    CHECK(strcmp(purrgo_layer_title(PURRGO_LAYER_WAYPOINT), "Waypoints") == 0);

    reset_all();
    CHECK(!purrgo_layer_on(app_config.layers, PURRGO_LAYER_WAYPOINT));   /* по умолчанию выключен */

    app_config.layers = (uint16_t)(app_config.layers | (1u << PURRGO_LAYER_WAYPOINT));
    CHECK(purrgo_config_save());

    uint32_t n;
    const uint8_t *d = fake_fs_data(CFG, &n);
    CHECK(d != NULL && strstr((const char *)d, "LAYER_WAYPOINT=1\n") != NULL);

    app_config.layers = 0;
    CHECK(purrgo_config_load());
    CHECK(purrgo_layer_on(app_config.layers, PURRGO_LAYER_WAYPOINT));
    CHECK(purrgo_layer_on(app_config.layers, PURRGO_LAYER_TRACK));       /* соседний слой не задет */
}

/* ------------------------------------------------------------ загрузка --- */

static void test_load_route_and_waypts(void)
{
    reset_all();

    /* Маршрут: линия + две путевые точки. Путь трека <trkpt> слоем не читается. */
    fake_fs_put("routes/r.gpx",
        "<gpx><wpt lat=\"53.0000001\" lon=\"28.0000000\"><name>R1</name></wpt>"
        "<wpt lat=\"53.0000002\" lon=\"28.0000000\"><name>R2</name></wpt>"
        "<trk><trkseg><trkpt lat=\"54.0\" lon=\"29.0\"/><trkpt lat=\"54.1\" lon=\"29.1\"/></trkseg></trk></gpx>");
    put_gpx("waypts/a.gpx", 100, 3);
    put_gpx("waypts/B.GPX", 200, 1);                    /* регистр расширения любой */
    put_gpx("waypts/.hidden.gpx", 300, 1);              /* скрытый - пропускается */
    put_gpx("waypts/notes.txt", 400, 1);                /* не gpx - пропускается */
    put_gpx("waypts/gpx", 500, 1);                      /* слишком короткое имя */

    CHECK(purrgo_wpt_layer_sync("r.gpx"));
    CHECK(purrgo_wpt_layer_count() == 6);               /* 2 + 3 + 1 */
    CHECK(!purrgo_wpt_layer_truncated());

    /* Сначала маршрут, потом каталог WAYPTS. */
    const purrgo_waypoint_t *p = purrgo_wpt_layer_get(0);
    CHECK(p != NULL && p->lat_1e7 == 530000001 && strcmp(p->name, "R1") == 0);
    p = purrgo_wpt_layer_get(1);
    CHECK(p != NULL && p->lat_1e7 == 530000002);

    int seen_a = 0, seen_b = 0, bad = 0;
    for (size_t i = 2; i < purrgo_wpt_layer_count(); i++) {
        p = purrgo_wpt_layer_get(i);
        const int32_t f = p->lat_1e7 - 530000000;
        if (f >= 100 && f < 103) { seen_a++; }
        else if (f == 200) { seen_b++; }
        else { bad++; }
    }
    CHECK(seen_a == 3 && seen_b == 1 && bad == 0);
    CHECK(purrgo_wpt_layer_get(6) == NULL);

    /* Ресурсы освобождены. */
    CHECK(fake_fs_open_handles() == 0);
    CHECK(fake_fs_open_dirs() == 0);
}

static void test_without_route_and_empty(void)
{
    reset_all();

    /* Пустой каталог, маршрута нет: рисовать нечего. */
    CHECK(!purrgo_wpt_layer_sync(""));
    CHECK(purrgo_wpt_layer_count() == 0);
    CHECK(!purrgo_wpt_layer_sync(NULL));

    /* Только WAYPTS: каталог перечитывается после clear. */
    put_gpx("waypts/a.gpx", 10, 2);
    purrgo_wpt_layer_clear();
    CHECK(purrgo_wpt_layer_sync(""));
    CHECK(purrgo_wpt_layer_count() == 2);

    /* Несуществующий и неверный маршрут не мешают точкам WAYPTS. */
    CHECK(purrgo_wpt_layer_sync("missing.gpx"));
    CHECK(purrgo_wpt_layer_count() == 2);
    CHECK(purrgo_wpt_layer_sync("../x.gpx"));
    CHECK(purrgo_wpt_layer_count() == 2);
    CHECK(fake_fs_open_handles() == 0 && fake_fs_open_dirs() == 0);

    /* Маршрут без <wpt> и каталог без файлов: пусто, без падения. */
    reset_all();
    fake_fs_put("routes/line.gpx",
        "<gpx><trk><trkseg><trkpt lat=\"54.0\" lon=\"29.0\"/><trkpt lat=\"54.1\" lon=\"29.1\"/></trkseg></trk></gpx>");
    CHECK(!purrgo_wpt_layer_sync("line.gpx"));
    CHECK(purrgo_wpt_layer_count() == 0);
}

/* ----------------------------------------------------------------- кэш --- */

static void test_cache_and_route_change(void)
{
    reset_all();
    put_gpx("routes/r1.gpx", 1000, 2);
    put_gpx("routes/r2.gpx", 2000, 5);
    put_gpx("waypts/a.gpx", 100, 3);

    CHECK(purrgo_wpt_layer_sync("r1.gpx"));
    CHECK(purrgo_wpt_layer_count() == 5);

    /* Повторный вызов с тем же маршрутом SD не читает. */
    const int opens = fake_fs_open_calls();
    CHECK(purrgo_wpt_layer_sync("r1.gpx"));
    CHECK(purrgo_wpt_layer_sync("r1.gpx"));
    CHECK(fake_fs_open_calls() == opens);
    CHECK(purrgo_wpt_layer_count() == 5);

    /* Другой маршрут: набор собирается заново, старые точки маршрута уходят. */
    CHECK(purrgo_wpt_layer_sync("r2.gpx"));
    CHECK(fake_fs_open_calls() > opens);
    CHECK(purrgo_wpt_layer_count() == 8);
    CHECK(purrgo_wpt_layer_get(0)->lat_1e7 == 530002000);

    /* Маршрут снят: остаются только точки WAYPTS. */
    CHECK(purrgo_wpt_layer_sync(""));
    CHECK(purrgo_wpt_layer_count() == 3);
}

/* ----------------------------------------------------------- предел 64 --- */

static void test_limit(void)
{
    reset_all();
    CHECK(PURRGO_WPT_LAYER_MAX == 64);

    /* Ровно в предел: усечения нет. */
    put_gpx("routes/r.gpx", 1000, 10);
    put_gpx("waypts/a.gpx", 100, 54);
    CHECK(purrgo_wpt_layer_sync("r.gpx"));
    CHECK(purrgo_wpt_layer_count() == 64);
    CHECK(!purrgo_wpt_layer_truncated());

    /* Больше предела: 10 + 40 + 40 = 90 -> 64, точки маршрута первые. */
    reset_all();
    put_gpx("routes/r.gpx", 1000, 10);
    put_gpx("waypts/a.gpx", 100, 40);
    put_gpx("waypts/b.gpx", 200, 40);
    CHECK(purrgo_wpt_layer_sync("r.gpx"));
    CHECK(purrgo_wpt_layer_count() == PURRGO_WPT_LAYER_MAX);
    CHECK(purrgo_wpt_layer_truncated());
    for (size_t i = 0; i < 10; i++) {
        CHECK(purrgo_wpt_layer_get(i)->lat_1e7 == 530001000 + (int32_t)i);
    }
    CHECK(purrgo_wpt_layer_get(PURRGO_WPT_LAYER_MAX) == NULL);
    CHECK(fake_fs_open_handles() == 0 && fake_fs_open_dirs() == 0);

    /* Один файл с избытком точек. */
    reset_all();
    put_gpx("waypts/big.gpx", 100, 100);
    CHECK(purrgo_wpt_layer_sync(""));
    CHECK(purrgo_wpt_layer_count() == PURRGO_WPT_LAYER_MAX);
    CHECK(purrgo_wpt_layer_truncated());

    /* Маршрут сам заполняет предел: файлы каталога не открываются. */
    reset_all();
    put_gpx("routes/r.gpx", 1000, 64);
    put_gpx("waypts/a.gpx", 100, 3);
    CHECK(purrgo_wpt_layer_sync("r.gpx"));
    CHECK(purrgo_wpt_layer_count() == 64);
    CHECK(purrgo_wpt_layer_truncated());
    for (size_t i = 0; i < 64; i++) {
        CHECK(purrgo_wpt_layer_get(i)->lat_1e7 >= 530001000);
    }
}

/* -------------------------------------------------------------- рендер --- */

#define W 64
#define H 48
#define BG 7
static uint8_t g_fb[W * H];
static gfx_context_t g_gfx;

static void cb_draw(void *fb, int16_t x, int16_t y, gfx_color_t c)
{
    if (x >= 0 && x < W && y >= 0 && y < H) { ((uint8_t *)fb)[y * W + x] = (uint8_t)c; }
}
static gfx_color_t cb_read(void *fb, int16_t x, int16_t y)
{
    return (x >= 0 && x < W && y >= 0 && y < H) ? ((uint8_t *)fb)[y * W + x] : 0;
}

static void canvas(uint8_t *fb)
{
    memset(fb, BG, W * H);
    (void)gfx_init(&g_gfx, W, H, g_fb, cb_draw, cb_read);
    gfx_set_color(&g_gfx, BLACK, WHITE);
}

/* Эталон: иконка с центром (cx, cy), пиксели по таблице purrgo_poi_icons. */
static void expect_icon(uint8_t *fb, int cx, int cy)
{
    const uint8_t (*icon)[PURRGO_POI_ICON_WIDTH] = purrgo_poi_icons[PURRGO_WPT_LAYER_ICON];

    for (int r = 0; r < (int)PURRGO_POI_ICON_HEIGHT; r++) {
        for (int c = 0; c < (int)PURRGO_POI_ICON_WIDTH; c++) {
            const uint8_t px = icon[r][c];
            const int x = cx - 5 + c, y = cy - 5 + r;

            if ((px & PURRGO_POI_ALPHA) && x >= 0 && x < W && y >= 0 && y < H) {
                fb[y * W + x] = (uint8_t)(px & 3u);
            }
        }
    }
}

static int icon_has_pixels(void)
{
    for (int r = 0; r < (int)PURRGO_POI_ICON_HEIGHT; r++) {
        for (int c = 0; c < (int)PURRGO_POI_ICON_WIDTH; c++) {
            if (purrgo_poi_icons[PURRGO_WPT_LAYER_ICON][r][c] & PURRGO_POI_ALPHA) { return 1; }
        }
    }
    return 0;
}

static void test_render(void)
{
    static uint8_t expected[W * H];
    /* 1000 единиц (1e-7 градуса) на пиксель: (32, 24) - центр кадра. */
    purrgo_bbox_t cam = { 280000000, 530000000, 280064000, 530048000 };
    purrgo_viewport_t vp = { W, H, 0, 0 };

    /* Иконка слоя путевых точек должна быть нарисована (не полностью прозрачна). */
    CHECK(icon_has_pixels());

    reset_all();
    fake_fs_put("routes/r.gpx",
        "<gpx>"
        "<wpt lat=\"53.0024000\" lon=\"28.0032000\"/>"    /* (32, 24) */
        "<wpt lat=\"53.0040000\" lon=\"28.0010000\"/>"    /* (10,  8) */
        "<wpt lat=\"53.0024000\" lon=\"28.0000000\"/>"    /* (0, 24): у края, видна частично */
        "<wpt lat=\"53.5000000\" lon=\"28.5000000\"/>"    /* далеко за кадром */
        "</gpx>");
    CHECK(purrgo_wpt_layer_sync("r.gpx"));
    CHECK(purrgo_wpt_layer_count() == 4);

    /* Пустые аргументы не падают и ничего не рисуют. */
    canvas(g_fb);
    purrgo_wpt_layer_render(NULL, &cam, &vp);
    purrgo_wpt_layer_render(&g_gfx, NULL, &vp);
    purrgo_wpt_layer_render(&g_gfx, &cam, NULL);
    canvas(expected);
    CHECK(memcmp(g_fb, expected, sizeof g_fb) == 0);

    /* Три видимые иконки на своих местах, четвёртая не рисуется. */
    purrgo_wpt_layer_render(&g_gfx, &cam, &vp);
    canvas(expected);
    expect_icon(expected, 32, 24);
    expect_icon(expected, 10, 8);
    expect_icon(expected, 0, 24);
    CHECK(memcmp(g_fb, expected, sizeof g_fb) == 0);

    /* Нет точек: ничего не рисуется. */
    purrgo_wpt_layer_clear();
    fake_fs_reset();
    purrgo_config_init();
    CHECK(!purrgo_wpt_layer_sync(""));
    canvas(g_fb);
    purrgo_wpt_layer_render(&g_gfx, &cam, &vp);
    canvas(expected);
    CHECK(memcmp(g_fb, expected, sizeof g_fb) == 0);
}

int main(void)
{
    test_layer_order_and_config();
    test_load_route_and_waypts();
    test_without_route_and_empty();
    test_cache_and_route_change();
    test_limit();
    test_render();
    return test_finish("test_wpt_layer");
}
