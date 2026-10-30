/*
 * Навигатор при отрицательной высоте (например, берег Мёртвого моря, ~ -430 м):
 *   - разбор GGA с отрицательной высотой и координатами 31.56 N, 35.47 E;
 *   - обновление решения и политика перерисовки на экране Trip computer
 *     (порог по высоте считается по модулю разности, знак не важен);
 *   - вывод "-430 m" на экране и <ele>-430</ele> в GPX;
 *   - расчёт восхода/заката на этой широте.
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/app_ui.h"
#include "purrgo/config.h"
#include "purrgo/gnss_adapter.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/sun.h"
#include "purrgo/track_logger.h"
#include "purrgo/ui/ui_dirty.h"
#include "ui_trip.h"
#include "fake_fs.h"
#include "test_common.h"

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

/* Собрать фразу NMEA с вычисленной контрольной суммой. */
static void make_sentence(char *out, size_t n, const char *body)
{
    unsigned cs = 0;
    for (const char *p = body; *p; p++) { cs ^= (unsigned char)*p; }
    snprintf(out, n, "$%s*%02X", body, cs);
}

static void feed(const char *body)
{
    char line[160];
    make_sentence(line, sizeof line, body);
    size_t len = strlen(line);
    for (size_t i = 0; i < len; i++) { purrgo_app_feed_gnss_byte((uint8_t)line[i]); }
    purrgo_app_feed_gnss_byte('\r');
    purrgo_app_feed_gnss_byte('\n');
}

/* RMC+GGA одной эпохи: берег Мёртвого моря, 31°33.540' N 35°28.390' E, высота alt (строкой). */
static void feed_epoch(const char *alt)
{
    char gga[128];
    feed("GPRMC,093015.00,A,3133.54000,N,03528.39000,E,0.50,90.00,191026,,,A");
    snprintf(gga, sizeof gga, "GPGGA,093015.00,3133.54000,N,03528.39000,E,1,08,1.1,%s,M,18.9,M,,", alt);
    feed(gga);
}

static void run_ticks(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 100) { g_now += 100; purrgo_app_tick(g_now); }
}

/* ------------------------------------------------------------ парсер --- */

static int32_t alt_after(const char *alt_field)
{
    purrgo_gnss_solution_t s;
    memset(&s, 0, sizeof s);
    char gga[128], line[160];
    snprintf(gga, sizeof gga, "GPGGA,093015.00,3133.54000,N,03528.39000,E,1,08,1.1,%s,M,18.9,M,,", alt_field);
    make_sentence(line, sizeof line, gga);
    (void)purrgo_gnss_process_nmea(line, &s);
    return s.alt_m;
}

static void test_parse_negative_altitude(void)
{
    CHECK(alt_after("-430.4") == -430);
    CHECK(alt_after("-430.0") == -430);
    CHECK(alt_after("-429.6") == -429);      /* усечение к нулю, как и для положительных */
    CHECK(alt_after("-0.5") == 0);
    CHECK(alt_after("-12.3") == -12);
    CHECK(alt_after("-1.0") == -1);
    CHECK(alt_after("-10994.0") == -10994);  /* Марианская впадина: диапазон int32 */
    CHECK(alt_after("430.4") == 430);
    CHECK(alt_after("8848.9") == 8848);

    /* Пустое поле высоты не затирает прежнее значение. */
    purrgo_gnss_solution_t s;
    memset(&s, 0, sizeof s);
    char line[160];
    make_sentence(line, sizeof line, "GPGGA,093015.00,3133.54000,N,03528.39000,E,1,08,1.1,-430.4,M,18.9,M,,");
    CHECK(!purrgo_gnss_process_nmea(line, &s));      /* GGA не RMC */
    CHECK(s.alt_m == -430);
    make_sentence(line, sizeof line, "GPGGA,093016.00,3133.54000,N,03528.39000,E,1,08,1.1,,M,18.9,M,,");
    (void)purrgo_gnss_process_nmea(line, &s);
    CHECK(s.alt_m == -430);

    /* Переход через уровень моря в обе стороны. */
    CHECK(alt_after("3.0") == 3);
    CHECK(alt_after("-3.0") == -3);
}

static void test_parse_dead_sea_position(void)
{
    purrgo_gnss_solution_t s;
    memset(&s, 0, sizeof s);
    char line[160];
    make_sentence(line, sizeof line, "GPRMC,093015.00,A,3133.54000,N,03528.39000,E,0.50,90.00,191026,,,A");
    CHECK(purrgo_gnss_process_nmea(line, &s));
    CHECK(s.valid);
    CHECK(s.lat_1e7 == 315590000);
    CHECK(s.lon_1e7 == 354731666);
}

/* -------------------------------------------------------- приложение --- */

static void test_app_negative_altitude(void)
{
    fake_fs_reset();
    memset(&app_config, 0, sizeof app_config);
    g_now = 200000;
    purrgo_app_init();
    run_ticks(1000);
    feed_epoch("-430.4");
    CHECK(purrgo_app_get_gnss_solution()->valid);
    CHECK(purrgo_app_get_gnss_solution()->alt_m == -430);

    /* на Trip computer */
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);
    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);
    CHECK(purrgo_app_get_state() == APP_STATE_TRIP_COMPUTER);
    for (int i = 0; i < 12; i++) {
        run_ticks(1000);
        feed_epoch("-430.4");
        while (purrgo_app_ui_needs_render(g_now)) { purrgo_ui_dirty_clear(UI_DIRTY_SCREEN); purrgo_ui_dirty_clear(UI_DIRTY_MAP); }
    }

    /* Изменение меньше порога (5 м): перерисовки нет, независимо от знака. */
    int redraws = 0;
    for (int i = 0; i < 12; i++) {
        run_ticks(1000);
        feed_epoch(i % 2 ? "-432.1" : "-428.9");
        if (purrgo_app_ui_needs_render(g_now)) { redraws++; purrgo_ui_dirty_clear(UI_DIRTY_SCREEN); }
    }
    CHECK(redraws == 0);
    CHECK(purrgo_app_get_gnss_solution()->alt_m == -428 || purrgo_app_get_gnss_solution()->alt_m == -432);

    /* Спуск на 10 м: перерисовка есть. */
    int redraws_down = 0;
    for (int i = 0; i < 8; i++) {
        run_ticks(1000);
        feed_epoch("-440.0");
        if (purrgo_app_ui_needs_render(g_now)) { redraws_down++; purrgo_ui_dirty_clear(UI_DIRTY_SCREEN); }
    }
    CHECK(redraws_down >= 1);
    CHECK(purrgo_app_get_gnss_solution()->alt_m == -440);

    /* Подъём через уровень моря: -3 -> +4 (разница 7 м) тоже приводит к перерисовке. */
    for (int i = 0; i < 8; i++) { run_ticks(1000); feed_epoch("-3.0"); while (purrgo_app_ui_needs_render(g_now)) { purrgo_ui_dirty_clear(UI_DIRTY_SCREEN); } }
    int redraws_cross = 0;
    for (int i = 0; i < 8; i++) {
        run_ticks(1000);
        feed_epoch("4.0");
        if (purrgo_app_ui_needs_render(g_now)) { redraws_cross++; purrgo_ui_dirty_clear(UI_DIRTY_SCREEN); }
    }
    CHECK(redraws_cross >= 1);
    CHECK(purrgo_app_get_gnss_solution()->alt_m == 4);
}

/* ------------------------------------------------------------- экран --- */

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
    return (x >= 0 && x < W && y >= 0 && y < H) ? g_fb[y * W + x] : 0;
}

static void render_values(int32_t alt, uint8_t *out)
{
    gfx_context_t g;
    purrgo_gnss_solution_t s;
    memset(&s, 0, sizeof s);
    s.valid = true; s.fix_type = 3; s.fix_quality = 1; s.satellites_tracked = 8;
    s.lat_1e7 = 315590000; s.lon_1e7 = 354731666; s.alt_m = alt;
    s.year = 26; s.month = 10; s.day = 19; s.hours = 9; s.minutes = 30; s.seconds = 15;
    memset(g_fb, 0, sizeof g_fb);
    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));
    ui_trip_render_grid(&g);
    ui_trip_render_values(&g, &s, NULL);
    memcpy(out, g_fb, sizeof g_fb);
}

static void test_ui_renders_minus_sign(void)
{
    static uint8_t neg[W * H], pos[W * H], neg2[W * H], zero[W * H];
    render_values(-430, neg);
    render_values(430, pos);
    render_values(-430, neg2);
    render_values(0, zero);

    CHECK(memcmp(neg, neg2, sizeof neg) == 0);        /* детерминированно */
    CHECK(memcmp(neg, pos, sizeof neg) != 0);         /* знак минус виден на экране */

    /* Различия только в строке ALT (шестая строка сетки: y = 10 + 5 * 12 = 70 .. 80). */
    int outside = 0, inside = 0;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (neg[y * W + x] != pos[y * W + x]) { (y >= 70 && y < 82) ? inside++ : outside++; }
        }
    }
    CHECK(inside > 0);
    CHECK(outside == 0);

    /* Строка с минусом шире и не обрезана: "-430 m" не пересекается с правым краем экрана. */
    int rightmost = 0;
    for (int y = 70; y < 82; y++) { for (int x = 0; x < W; x++) { if (neg[y * W + x] != 3 + 1 && neg[y * W + x] != 0) { if (x > rightmost) rightmost = x; } } }
    CHECK(rightmost < W - 1);
    CHECK(memcmp(zero, neg, sizeof zero) != 0);

    /* Текст высоты формируется штатным purrgo_snprintf. */
    char buf[32];
    purrgo_snprintf(buf, sizeof buf, "%d m        ", (int)-430);
    CHECK(strncmp(buf, "-430 m", 6) == 0);
    purrgo_snprintf(buf, sizeof buf, "%d m        ", (int)-5);
    CHECK(strncmp(buf, "-5 m", 4) == 0);
}

/* --------------------------------------------------------------- GPX --- */

static void test_gpx_negative_elevation(void)
{
    purrgo_logger_stop();
    fake_fs_reset();
    app_config.tz_offset_minutes = 180;
    purrgo_logger_set_mode(LOGGER_MODE_STANDARD);

    purrgo_gnss_solution_t f;
    memset(&f, 0, sizeof f);
    f.valid = true; f.year = 26; f.month = 10; f.day = 19; f.hours = 9; f.minutes = 30; f.seconds = 15;
    f.lat_1e7 = 315590000; f.lon_1e7 = 354731666; f.alt_m = -430;
    CHECK(purrgo_logger_start(&f));
    CHECK(purrgo_logger_add_point(&f));
    const char *name = purrgo_logger_get_active_filename();
    char saved[64];
    snprintf(saved, sizeof saved, "%s", name ? name : "");
    purrgo_logger_stop();

    uint32_t n = 0;
    const uint8_t *d = fake_fs_data(saved, &n);
    CHECK(d != NULL);
    if (d) {
        char *t = malloc(n + 1);
        memcpy(t, d, n); t[n] = 0;
        CHECK(strstr(t, "<ele>-430</ele>") != NULL);
        CHECK(strstr(t, "lat=\"31.5590000\"") != NULL);
        CHECK(strstr(t, "lon=\"35.4731666\"") != NULL);
        free(t);
    }
    app_config.tz_offset_minutes = 0;
}

/* ------------------------------------------------------------- солнце --- */

static void test_sun_at_dead_sea(void)
{
    purrgo_sun_info_t info;
    memset(&info, 0, sizeof info);
    purrgo_sun_calc(315590000, 354731666, 26, 10, 19, 9, 30, 180, &info);
    CHECK(info.status == SUN_STATUS_NORMAL);
    CHECK(info.sunrise_hour >= 5 && info.sunrise_hour <= 7);
    CHECK(info.sunset_hour >= 16 && info.sunset_hour <= 18);
    CHECK(info.is_daytime);                    /* 12:30 по местному времени */
    CHECK(info.time_to_event_min > 0 && info.time_to_event_min < 24 * 60);
}

int main(void)
{
    test_parse_negative_altitude();
    test_parse_dead_sea_position();
    test_app_negative_altitude();
    test_ui_renders_minus_sign();
    test_gpx_negative_elevation();
    test_sun_at_dead_sea();
    return test_finish("test_negative_altitude");
}
