/*
 * Host-тесты правок интерфейса карты:
 *   - маркёр GNSS не перерисовывается, пока смещение от нарисованного положения
 *     не больше PURRGO_MAP_MARKER_MIN_SHIFT_PX (3 px); большее смещение рисуется;
 *   - шаг ручного сдвига карты по вертикали больше, чем по горизонтали
 *     (1/2 высоты окна карты против 1/4 ширины);
 *   - заряд батареи (заглушка) в верхней строке карты и в <extensions> точки GPX.
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/app_ui.h"
#include "purrgo/battery.h"
#include "purrgo/config.h"
#include "purrgo/geo.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_text.h"
#include "purrgo/hardware_config.h"
#include "purrgo/map_controller.h"
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
#define PX_BLACK (BLACK + 1)

static int count_px(int x0, int y0, int x1, int y1, uint8_t v)
{
    int n = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) if (g_fb[y * W + x] == v) n++;
    return n;
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

/* Фикс на широте 31 град + lat_min минут (ddmm.mmmmm), долгота постоянная, курс 90. */
static void feed_epoch_lat(const char *lat_ddmm)
{
    char b[120];
    snprintf(b, sizeof b, "GPRMC,093015.00,A,%s,N,03528.39000,E,0.50,90.00,191026,,,A", lat_ddmm);
    feed(b);
    snprintf(b, sizeof b, "GPGGA,093015.00,%s,N,03528.39000,E,1,08,1.1,100.0,M,18.9,M,,", lat_ddmm);
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

static void test_marker_threshold_and_battery_line(void)
{
    gfx_context_t g;
    static uint8_t snap[W * H];

    fake_fs_reset();
    memset(&app_config, 0, sizeof app_config);
    app_config.last_lat_1e7 = 315590000;        /* 31 град 33.54 мин */
    app_config.last_lon_1e7 = 354731666;
    g_now = 200000;
    purrgo_app_init();
    run_ticks(1000);
    feed_epoch_lat("3133.54000");
    CHECK(purrgo_app_get_gnss_solution()->valid);

    memset(g_fb, 0, sizeof g_fb);
    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));
    for (int i = 0; i < 4; i++) {
        run_ticks(3000);
        feed_epoch_lat("3133.54000");
        render(&g);
    }

    /*
     * Ручной сдвиг карты отключает автослежение: иначе камера перецентрируется
     * и вызывает полную перерисовку карты, а проверяется именно частичное
     * обновление маркёра. Маркёр после сдвига влево остаётся в окне карты.
     */
    CHECK(purrgo_app_get_state() == APP_STATE_MAP);
    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_LEFT));
    CHECK(map_app_is_manual_pan_active());
    for (int i = 0; i < 3; i++) {
        run_ticks(3000);
        feed_epoch_lat("3133.54000");
        render(&g);
    }

    /* Масштаб по умолчанию 500M: 500 м на 176 px (~2,84 м/px), 0.001' широты ~ 1,85 м. */
    int marker_px = count_px(0, 9, W, H - 9, PX_BLACK);
    CHECK(marker_px > 0);

    /* Заряд (заглушка 100%) нарисован в верхней строке слева от масштаба "500M" (x = 150). */
    CHECK(count_px(120, 0, 144, 9, PX_BLACK) > 0);

    memcpy(snap, g_fb, sizeof snap);

    /* Сдвиг ~5,6 м (~2 px, но уже >= 5 м порога app_fsm): маркёр остаётся на месте. */
    run_ticks(3000);
    feed_epoch_lat("3133.54300");
    render(&g);
    run_ticks(3000);
    feed_epoch_lat("3133.54300");
    render(&g);
    CHECK(memcmp(snap, g_fb, sizeof snap) == 0);

    /* Накопленный сдвиг ~13,9 м (~4,9 px) от нарисованного положения: маркёр перерисован. */
    run_ticks(3000);
    feed_epoch_lat("3133.54750");
    render(&g);
    run_ticks(3000);
    feed_epoch_lat("3133.54750");
    render(&g);
    CHECK(memcmp(snap, g_fb, sizeof snap) != 0);
    CHECK(count_px(0, 9, W, H - 9, PX_BLACK) > 0);
}

static void test_pan_step(void)
{
    memset(&app_config, 0, sizeof app_config);
    app_config.last_lat_1e7 = 315590000;
    app_config.last_lon_1e7 = 354731666;
    purrgo_map_controller_init();

    const uint32_t width_m = map_app_get_map_scale_width_m();
    const int32_t lat0 = map_app_get_map_center_lat();
    const int32_t lon0 = map_app_get_map_center_lon();

    /* Вертикаль: 1/PURRGO_MAP_PAN_STEP_Y_DIV высоты окна карты (по умолчанию 1/2). */
    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_UP));
    int32_t dlat = map_app_get_map_center_lat() - lat0;
    int64_t expect_dy_m = ((int64_t)width_m * PURRGO_MAP_VIEWPORT_HEIGHT_PX / PURRGO_HW_DISPLAY_WIDTH_PX)
                           / PURRGO_MAP_PAN_STEP_Y_DIV;
    CHECK(dlat == (int32_t)(expect_dy_m * PURRGO_1E7_PER_METER));
    CHECK(map_app_is_manual_pan_active());

    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_DOWN));
    CHECK(map_app_get_map_center_lat() == lat0);

    /* Горизонталь: 1/PURRGO_MAP_PAN_STEP_X_DIV ширины (по умолчанию 1/4), с поправкой на cos(широты). */
    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_RIGHT));
    int32_t dlon = map_app_get_map_center_lon() - lon0;
    int32_t cos_val = purrgo_geo_cos_10k(lat0);
    int64_t expect_dx = ((int64_t)(width_m / PURRGO_MAP_PAN_STEP_X_DIV) * PURRGO_1E7_PER_METER * 10000) / cos_val;
    CHECK(dlon == (int32_t)expect_dx);

    /* Вертикальный шаг в пикселях заметно больше горизонтального (по умолчанию ~123 против ~44). */
    int64_t px_y = (int64_t)dlat * PURRGO_HW_DISPLAY_WIDTH_PX / ((int64_t)width_m * PURRGO_1E7_PER_METER);
    int64_t px_x = (int64_t)PURRGO_HW_DISPLAY_WIDTH_PX / PURRGO_MAP_PAN_STEP_X_DIV;
    CHECK(px_y > px_x);
    CHECK(px_y <= PURRGO_MAP_VIEWPORT_HEIGHT_PX);
}

static void test_track_battery(void)
{
    purrgo_logger_reset();
    fake_fs_reset();
    memset(&app_config, 0, sizeof app_config);
    purrgo_logger_set_mode(LOGGER_MODE_STANDARD);

    purrgo_gnss_solution_t f;
    memset(&f, 0, sizeof f);
    f.valid = true; f.year = 26; f.month = 10; f.day = 19; f.hours = 12;
    f.lat_1e7 = 537000000; f.lon_1e7 = 284000000; f.alt_m = 100; f.speed_knots = 500;
    CHECK(purrgo_logger_start(&f));
    char name[64];
    snprintf(name, sizeof name, "%s", purrgo_logger_get_active_filename());
    CHECK(purrgo_logger_add_point(&f));
    purrgo_logger_stop();

    uint32_t n = 0;
    const uint8_t *d = fake_fs_data(name, &n);
    CHECK(d != NULL);
    if (d) {
        char *t = malloc((size_t)n + 1u);
        memcpy(t, d, n); t[n] = 0;
        purrgo_battery_t b;
        CHECK(purrgo_battery_read(&b) && b.percent == 100);
        CHECK(strstr(t, "xmlns:pg=") != NULL);
        CHECK(strstr(t, "<extensions><pg:bat>100</pg:bat></extensions>") != NULL);
        /* <extensions> - последний элемент точки (порядок по схеме GPX 1.1). */
        const char *ext = strstr(t, "<extensions>");
        const char *tm = strstr(t, "<time>");
        const char *end = strstr(t, "</trkpt>");
        CHECK(tm && ext && end && tm < ext && ext < end);
        CHECK(strstr(t, "</gpx>") != NULL);
        free(t);
    }
}

int main(void)
{
    test_marker_threshold_and_battery_line();
    test_pan_step();
    test_track_battery();
    return test_finish("test_marker_pan_battery");
}
