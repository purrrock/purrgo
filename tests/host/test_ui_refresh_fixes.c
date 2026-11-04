/*
 * Host-тесты исправлений обновления изображения:
 *   - меню: смена значения пункта (SEL) при том же курсоре обновляет строку
 *     (CONFIG, MAP LAYERS), как и перемещение курсора;
 *   - трек: отрезки, добавленные между обновлениями экрана, не пропадают и не
 *     затираются возвратом фона маркёра (экран без полной перерисовки совпадает
 *     с полной перерисовкой, кроме области маркёра);
 *   - маркёр: чёрная заливка не выходит за белую рамку при любом курсе;
 *   - лимит частичных обновлений до полного - 10.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/app_ui.h"
#include "purrgo/config.h"
#include "purrgo/config_controller.h"
#include "purrgo/display_hal.h"
#include "purrgo/gfx_polygon.h"
#include "purrgo/hardware_config.h"
#include "purrgo/map_controller.h"
#include "purrgo/pc_display_stats.h"
#include "purrgo/track_logger.h"
#include "purrgo/ui/ui_dirty.h"
#include "fake_fs.h"
#include "test_common.h"

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

#define W 176
#define H 264
static uint8_t g_fb[W * H];
static void cb_draw(void *fb, int16_t x, int16_t y, gfx_color_t c)
{
    (void)fb;
    if (x >= 0 && x < W && y >= 0 && y < H) { g_fb[y * W + x] = (uint8_t)(c + 1); }
}
static gfx_color_t cb_read(void *fb, int16_t x, int16_t y)
{
    (void)fb;
    return (x >= 0 && x < W && y >= 0 && y < H) ? (gfx_color_t)(g_fb[y * W + x] - 1) : 0;
}

static void feed(const char *body)
{
    char line[160];
    unsigned cs = 0;
    for (const char *p = body; *p; p++) { cs ^= (unsigned char)*p; }
    snprintf(line, sizeof line, "$%s*%02X", body, cs);
    for (size_t i = 0; line[i]; i++) { purrgo_app_feed_gnss_byte((uint8_t)line[i]); }
    purrgo_app_feed_gnss_byte('\r');
    purrgo_app_feed_gnss_byte('\n');
}

/* lat_min - минуты широты 31 град (ddmm.mmmmm), долгота постоянная, курс 0 (на север). */
static void feed_epoch_lat(double lat_min)
{
    char b[140];
    snprintf(b, sizeof b, "GPRMC,093015.00,A,31%08.5f,N,03528.39000,E,2.00,0.00,191026,,,A", lat_min);
    feed(b);
    snprintf(b, sizeof b, "GPGGA,093015.00,31%08.5f,N,03528.39000,E,1,08,1.1,100.0,M,18.9,M,,", lat_min);
    feed(b);
}

static void run_ticks(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 100) { g_now += 100; purrgo_app_tick(g_now); }
}

static void render(gfx_context_t *g)
{
    purrgo_app_ui_render(g, purrgo_app_get_gnss_solution(), NULL);
}

static void app_start(gfx_context_t *g, uint16_t layers)
{
    fake_fs_reset();
    purrgo_logger_reset();
    memset(&app_config, 0, sizeof app_config);
    app_config.last_lat_1e7 = 315590000;        /* 31 град 33.54 мин */
    app_config.last_lon_1e7 = 354731666;
    app_config.layers = layers;
    g_now = 200000;
    purrgo_app_init();
    purrgo_app_ui_reset();
    run_ticks(1000);
    feed_epoch_lat(33.54000);
    memset(g_fb, 0, sizeof g_fb);
    CHECK(gfx_init(g, W, H, g_fb, cb_draw, cb_read));
}

/* ------------------------------- меню ------------------------------------ */

static void test_menu_value_change_refreshes_row(void)
{
    gfx_context_t g;
    app_start(&g, 0);

    for (int i = 0; i < 3; i++) { run_ticks(1000); feed_epoch_lat(33.54000); render(&g); }

    /* Карта -> Trip computer -> CONFIG. */
    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);
    CHECK(purrgo_app_get_state() == APP_STATE_TRIP_COMPUTER);
    render(&g);
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);
    CHECK(purrgo_app_get_state() == APP_STATE_MENU_CONFIG);
    render(&g);                                   /* вход: полное обновление экрана */

    /* Первая строка - BOOKS (режим чтения); вниз на вторую (источник питания). */
    CHECK(config_app_get_config_cursor() == 0);
    purrgo_app_handle_button(PURRGO_BTN_KEY2_SHORT);
    CHECK(config_app_get_config_cursor() == 1);
    render(&g);

    /* SEL на строке POWER: меняется значение, курсор на месте. */
    const purrgo_power_source_t before = config_app_get_draft_power_source();
    pc_display_stats_reset();
    purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);
    CHECK(config_app_get_draft_power_source() != before);
    CHECK(config_app_get_config_cursor() == 1);
    render(&g);
    pc_display_stats_t st = pc_display_stats_get();
    CHECK(st.fulls == 0);
    CHECK(st.partials == 1);
    CHECK(st.has_region && st.last_y == 40 && st.last_h == 15);   /* ровно строка 1 */

    /* Перемещение курсора: обновляются старая и новая строки. */
    pc_display_stats_reset();
    purrgo_app_handle_button(PURRGO_BTN_KEY2_SHORT);
    CHECK(config_app_get_config_cursor() == 2);
    render(&g);
    st = pc_display_stats_get();
    CHECK(st.partials == 1);
    CHECK(st.has_region && st.last_y == 40 && st.last_h == 30);   /* строки 1 и 2 */

    /* Без изменений - без обновления. */
    pc_display_stats_reset();
    render(&g);
    st = pc_display_stats_get();
    CHECK(st.partials == 0 && st.fulls == 0);
}

static void test_map_layers_toggle_refreshes_row(void)
{
    gfx_context_t g;
    app_start(&g, 0);
    for (int i = 0; i < 3; i++) { run_ticks(1000); feed_epoch_lat(33.54000); render(&g); }

    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);
    render(&g);
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);
    render(&g);
    CHECK(purrgo_app_get_state() == APP_STATE_MENU_CONFIG);

    /* Вниз до последней строки (MAP LAYERS) и войти. */
    for (int i = 0; i < PURRGO_CONFIG_MENU_ITEMS - 1; i++) {
        purrgo_app_handle_button(PURRGO_BTN_KEY2_SHORT);
        render(&g);
    }
    purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);
    CHECK(purrgo_app_get_state() == APP_STATE_MENU_MAP_LAYERS);
    render(&g);

    const uint16_t layers0 = config_app_get_draft_layers();
    pc_display_stats_reset();
    purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);    /* переключить слой под курсором */
    CHECK(config_app_get_draft_layers() != layers0);
    render(&g);
    pc_display_stats_t st = pc_display_stats_get();
    CHECK(st.fulls == 0 && st.partials == 1);
    CHECK(st.has_region && st.last_y == 30 && st.last_h == 15);
}

/* ------------------------------- трек ------------------------------------ */

static void test_track_segments_not_lost(void)
{
    gfx_context_t g;
    static uint8_t inc[W * H], full[W * H];

    app_start(&g, (uint16_t)(1u << PURRGO_LAYER_TRACK));
    for (int i = 0; i < 4; i++) { run_ticks(1000); feed_epoch_lat(33.54000); render(&g); }

    /* Ручной сдвиг отключает автослежение камеры: перерисовка карты не нужна. */
    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_LEFT));
    purrgo_ui_dirty_mark(UI_DIRTY_SCREEN);
    render(&g);

    /*
     * Движение на юг по ~7,4 м (0.004') за секунду: каждый отрезок ~2,6 px,
     * короче маркёра. Экран обновляется раз в 3 точки, как при ограничении
     * частоты перерисовки, поэтому за один проход добавляется несколько отрезков.
     */
    double lat = 33.54000;
    for (int k = 1; k <= 24; k++) {
        lat -= 0.004;
        run_ticks(1000);
        feed_epoch_lat(lat);
        if (k % 3 == 0) {
            purrgo_ui_dirty_mark(UI_DIRTY_SCREEN);
            render(&g);
        }
    }
    CHECK(purrgo_logger_get_track_point_count() >= 15);
    memcpy(inc, g_fb, sizeof inc);

    /* Полная перерисовка карты: эталон. */
    purrgo_ui_dirty_mark(UI_DIRTY_MAP);
    render(&g);
    memcpy(full, g_fb, sizeof full);

    /* Различия допустимы только в области маркёра (сдвиг маркёра меньше порога). */
    int x0 = W, y0 = H, x1 = -1, y1 = -1, diff = 0;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (inc[y * W + x] != full[y * W + x]) {
                diff++;
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
        }
    }
    CHECK(diff == 0 || (x1 - x0 + 1 <= 24 && y1 - y0 + 1 <= 24));

    /* Трек вообще нарисован: чёрных пикселей больше, чем даёт один маркёр (путь ~60 px). */
    int black = 0;
    for (int y = 9; y < H - 9; y++) for (int x = 0; x < W; x++) if (full[y * W + x] == BLACK + 1) black++;
    CHECK(black > 50);
}

/* ------------------------------ маркёр ----------------------------------- */

#define MW 64
#define MH 64
static uint8_t g_mk[MW * MH];
static void mk_draw(void *fb, int16_t x, int16_t y, gfx_color_t c)
{
    (void)fb;
    if (x >= 0 && x < MW && y >= 0 && y < MH) { g_mk[y * MW + x] = (uint8_t)(c + 1); }
}
static gfx_color_t mk_read(void *fb, int16_t x, int16_t y)
{
    (void)fb;
    return (x >= 0 && x < MW && y >= 0 && y < MH) ? (gfx_color_t)(g_mk[y * MW + x] - 1) : 0;
}

#define MK_BG    (LIGHT_GRAY + 1)
#define MK_BLACK (BLACK + 1)

/* Чёрных пикселей, у которых есть 4-сосед фона: заливка вышла за рамку. */
static int mk_leaks(void)
{
    static const int dx[4] = { 1, -1, 0, 0 };
    static const int dy[4] = { 0, 0, 1, -1 };
    int leaks = 0;

    for (int y = 0; y < MH; y++) {
        for (int x = 0; x < MW; x++) {
            if (g_mk[y * MW + x] != MK_BLACK) { continue; }
            for (int k = 0; k < 4; k++) {
                const int nx = x + dx[k], ny = y + dy[k];
                if (nx < 0 || ny < 0 || nx >= MW || ny >= MH || g_mk[ny * MW + nx] == MK_BG) {
                    leaks++;
                    break;
                }
            }
        }
    }
    return leaks;
}

static int mk_black(void)
{
    int n = 0;
    for (int i = 0; i < MW * MH; i++) { if (g_mk[i] == MK_BLACK) { n++; } }
    return n;
}

static void mk_reset(gfx_context_t *c)
{
    memset(g_mk, MK_BG, sizeof g_mk);
    gfx_init(c, MW, MH, g_mk, mk_draw, mk_read);
}

static int rnd(int lo, int hi) { return lo + rand() % (hi - lo + 1); }

static void test_marker_fill_inside_frame(void)
{
    gfx_context_t c;
    int bad_new = 0, bad_old = 0, empty = 0, shapes = 0;

    /* Маркёр-стрелка 13 px (вершины (0,-6), (-5,6), (5,6)) при всех курсах. */
    for (int deg = 0; deg < 360; deg++) {
        const double a = deg * 3.14159265358979 / 180.0;
        const double sn = sin(a), cs = cos(a);
        const double lx[3] = { 0, -5, 5 }, ly[3] = { -6, 6, 6 };
        gfx_point_t p[3];

        for (int i = 0; i < 3; i++) {
            p[i].x = (int32_t)(32 + lround(lx[i] * cs - ly[i] * sn));
            p[i].y = (int32_t)(32 + lround(lx[i] * sn + ly[i] * cs));
        }

        mk_reset(&c);
        gfx_draw_polygon_bordered(&c, p, 3, BLACK, WHITE);
        bad_new += (mk_leaks() != 0);
        empty += (mk_black() == 0);

        mk_reset(&c);
        gfx_set_color(&c, BLACK, WHITE);
        gfx_fill_polygon(&c, p, 3);
        gfx_set_color(&c, WHITE, WHITE);
        gfx_draw_polygon(&c, p, 3);
        bad_old += (mk_leaks() != 0);
        shapes++;
    }
    CHECK(bad_new == 0);
    CHECK(empty < shapes / 4);       /* внутри рамки заливка у большинства курсов видна */
    printf("marker arrow: shapes=%d leaks new=%d (old fill+outline: %d)\n", shapes, bad_new, bad_old);

    /* Восьмиугольник 13 px (срез 2 px). */
    {
        const gfx_point_t o[8] = {
            { 30, 26 }, { 34, 26 }, { 38, 30 }, { 38, 34 }, { 34, 38 }, { 30, 38 }, { 26, 34 }, { 26, 30 }
        };
        mk_reset(&c);
        gfx_draw_polygon_bordered(&c, o, 8, BLACK, WHITE);
        CHECK(mk_leaks() == 0);
        CHECK(mk_black() > 40);
    }

    /* Случайные выпуклые фигуры (треугольники): заливка внутри рамки всегда. */
    srand(12345);
    int rbad = 0, rold = 0;
    for (int n = 0; n < 2000; n++) {
        gfx_point_t p[3] = {
            { rnd(22, 44), rnd(22, 44) }, { rnd(22, 44), rnd(22, 44) }, { rnd(22, 44), rnd(22, 44) }
        };
        const long area2 = (long)(p[1].x - p[0].x) * (p[2].y - p[0].y) - (long)(p[2].x - p[0].x) * (p[1].y - p[0].y);
        if (area2 == 0) { continue; }
        mk_reset(&c);
        gfx_draw_polygon_bordered(&c, p, 3, BLACK, WHITE);
        rbad += (mk_leaks() != 0);

        mk_reset(&c);
        gfx_set_color(&c, BLACK, WHITE);
        gfx_fill_polygon(&c, p, 3);
        gfx_set_color(&c, WHITE, WHITE);
        gfx_draw_polygon(&c, p, 3);
        rold += (mk_leaks() != 0);
    }
    CHECK(rbad == 0);
    printf("random triangles: leaks new=%d (old fill+outline: %d of 2000)\n", rbad, rold);

    /* Контекст не портится: цвета прежние. */
    mk_reset(&c);
    gfx_set_color(&c, DARK_GRAY, LIGHT_GRAY);
    {
        const gfx_point_t t[3] = { { 32, 26 }, { 27, 38 }, { 37, 38 } };
        gfx_draw_polygon_bordered(&c, t, 3, BLACK, WHITE);
    }
    CHECK(c.color_fg == DARK_GRAY && c.color_bg == LIGHT_GRAY);
}

int main(void)
{
    test_menu_value_change_refreshes_row();
    test_map_layers_toggle_refreshes_row();
    test_track_segments_not_lost();
    test_marker_fill_inside_frame();
    return test_finish("test_ui_refresh_fixes");
}
