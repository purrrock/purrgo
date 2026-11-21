/*
 * Время неизвестно - на экранах прочерки, а не нули и не "последнее известное" время:
 *   - карта: время в строке статуса; путевой компьютер: строки LOC и UTC;
 *   - признак time_valid ведёт приёмник (UBX-NAV-PVT, NMEA RMC) и сбрасывает таймаут
 *     молчания приёмника;
 *   - поля hours/minutes при time_valid = false на картинку не влияют.
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/app_ui.h"
#include "purrgo/config.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_text.h"
#include "purrgo/hardware_config.h"
#include "purrgo/ui/ui_dirty.h"
#include "ui/ui_trip.h"
#include "fake_fs.h"
#include "fake_power.h"
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

static void feed_epoch(void)
{
    feed("GPRMC,093015.00,A,3133.54000,N,03528.39000,E,0.50,90.00,191026,,,A");
    feed("GPGGA,093015.00,3133.54000,N,03528.39000,E,1,08,1.1,100.0,M,18.9,M,,");
}

/* Шаг 10 мс; feeding = true: решение приходит раз в секунду, иначе приёмник молчит. */
static void run_ms(uint32_t ms, bool feeding)
{
    for (uint32_t t = 0; t < ms; t += 10) {
        g_now += 10;
        if (feeding && g_now % 1000u == 0u) { feed_epoch(); }
        purrgo_app_tick(g_now);
    }
}

static void render(gfx_context_t *g)
{
    purrgo_ui_dirty_mark(UI_DIRTY_STATUS);
    purrgo_app_ui_render(g, purrgo_app_get_gnss_solution(), NULL);
}

/* Сравнивает левый верхний угол строки статуса карты с эталоном - текстом на светлом фоне. */
static int status_time_diff(const char *text)
{
    static uint8_t saved[W * H];
    static uint8_t ref[W * H];
    gfx_context_t rg;
    int diff = 0;

    memcpy(saved, g_fb, sizeof saved);
    memset(g_fb, 0, sizeof g_fb);
    CHECK(gfx_init(&rg, W, H, g_fb, cb_draw, cb_read));
    gfx_set_color(&rg, BLACK, WHITE);
    gfx_fill_rect(&rg, 0, 0, W, 9);
    gfx_draw_string(&rg, 2, 1, text);
    memcpy(ref, g_fb, sizeof ref);
    memcpy(g_fb, saved, sizeof saved);

    for (int y = 0; y < 9; y++)
        for (int x = 0; x < 36; x++)       /* "hh:mm" - 5 знаков по 6 пикселей от x = 2 */
            if (g_fb[y * W + x] != ref[y * W + x]) diff++;
    return diff;
}

static void app_start(gfx_context_t *g)
{
    fake_fs_reset();
    fake_power_reset();
    purrgo_logger_reset();
    memset(&app_config, 0, sizeof app_config);
    app_config.last_lat_1e7 = 315590000;
    app_config.last_lon_1e7 = 354731666;
    g_now += 200000;
    purrgo_app_init();
    purrgo_app_ui_reset();
    memset(g_fb, 0, sizeof g_fb);
    CHECK(gfx_init(g, W, H, g_fb, cb_draw, cb_read));
}

/* Карта: прочерки до первого решения, время при решении и снова прочерки, когда приёмник замолчал. */
static void test_map_status_bar(void)
{
    gfx_context_t g;

    app_start(&g);
    app_config.tz_offset_minutes = 0;        /* purrgo_app_init() загружает конфиг со своим поясом по умолчанию */
    CHECK(purrgo_app_get_state() == APP_STATE_MAP);

    /* Приёмник ещё ничего не прислал: не "00:00". */
    CHECK(!purrgo_app_get_gnss_solution()->time_valid);
    render(&g);
    CHECK(status_time_diff("--:--") == 0);
    CHECK(status_time_diff("00:00") != 0);

    /* Решение пришло: местное время (смещение пояса 0). */
    feed_epoch();
    run_ms(3000, true);
    CHECK(purrgo_app_get_gnss_solution()->time_valid && purrgo_app_get_gnss_solution()->valid);
    render(&g);
    CHECK(status_time_diff("09:30") == 0);

    /* Часовой пояс применяется к времени. */
    app_config.tz_offset_minutes = 180;
    render(&g);
    CHECK(status_time_diff("12:30") == 0);
    app_config.tz_offset_minutes = 0;

    /* Приёмник замолчал дольше таймаута: время больше не обновляется - прочерки,
     * а не 09:30, застрявшее на экране. */
    run_ms(8000, false);
    CHECK(!purrgo_app_get_gnss_solution()->time_valid && !purrgo_app_get_gnss_solution()->valid);
    CHECK(purrgo_app_ui_needs_render(g_now));
    render(&g);
    CHECK(status_time_diff("--:--") == 0);
}

static int render_trip_fb(uint8_t *out, const purrgo_gnss_solution_t *sol)
{
    gfx_context_t g;

    memset(g_fb, 0, sizeof g_fb);
    if (!gfx_init(&g, W, H, g_fb, cb_draw, cb_read)) { return 0; }
    ui_trip_render_values(&g, sol, NULL);
    memcpy(out, g_fb, sizeof g_fb);
    return 1;
}

/* Путевой компьютер: при неизвестном времени LOC и UTC не зависят от полей времени решения. */
static void test_trip_rows(void)
{
    static uint8_t a[W * H], b[W * H], c[W * H], known[W * H];
    gfx_context_t g;
    purrgo_gnss_solution_t s;

    app_start(&g);
    app_config.tz_offset_minutes = 120;

    memset(&s, 0, sizeof s);
    s.valid = true; s.fix_type = 3; s.satellites_tracked = 8;
    s.lat_1e7 = 315590000; s.lon_1e7 = 354731666;
    s.year = 26; s.month = 10; s.day = 19;

    s.time_valid = true; s.hours = 9; s.minutes = 30;
    CHECK(render_trip_fb(known, &s));

    s.time_valid = false; s.hours = 0; s.minutes = 0;           /* нули */
    CHECK(render_trip_fb(a, &s));
    s.hours = 9; s.minutes = 30;                                /* последнее известное время */
    CHECK(render_trip_fb(b, &s));
    s.hours = 23; s.minutes = 59;
    CHECK(render_trip_fb(c, &s));

    CHECK(memcmp(a, b, sizeof a) == 0);
    CHECK(memcmp(a, c, sizeof a) == 0);
    CHECK(memcmp(a, known, sizeof a) != 0);

    /* Различие только в строках LOC и UTC (y = 10..33), остальной экран тот же. */
    int diff_outside = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (a[y * W + x] != known[y * W + x] && (y < 10 || y >= 34)) diff_outside++;
    CHECK(diff_outside == 0);
}

int main(void)
{
    test_map_status_bar();
    test_trip_rows();
    return test_finish("test_time_unknown");
}
