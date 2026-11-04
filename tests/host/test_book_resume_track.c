/*
 * Host-тест: после выхода из читалки запись трека возобновляется и точки пишутся.
 *
 * Поток без GSA - как у PC-мока (pc_gnss_mock.c): fix_type приходит только из GSA,
 * поэтому сброс решения при входе в читалку не должен оставлять fix_type = "нет
 * фикса", иначе фильтр качества логгера отбросит все точки (файл создан, трека нет).
 * Поток с GSA - как у u-blox на железе.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include "purrgo/menu_controller.h"
#include "purrgo/track_logger.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

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

static int g_sec;
/* Эпоха как у u-blox: GGA, GSA, RMC (порядок как в реальном потоке). */
static bool g_with_gsa;
static void feed_epoch(double lat_min)
{
    char b[160];
    g_sec = (g_sec + 1) % 60;
    snprintf(b, sizeof b, "GPGGA,0930%02d.00,31%08.5f,N,03528.39000,E,1,08,1.1,100.0,M,18.9,M,,", g_sec, lat_min);
    feed(b);
    if (g_with_gsa) { feed("GNGSA,A,3,01,02,03,04,05,06,07,08,,,,,1.8,1.1,1.4,1"); }
    snprintf(b, sizeof b, "GPRMC,0930%02d.00,A,31%08.5f,N,03528.39000,E,5.00,0.00,191026,,,A", g_sec, lat_min);
    feed(b);
}

static double g_lat = 33.54000;
static void run_seconds(int n, bool with_gnss)
{
    for (int s = 0; s < n; s++) {
        for (int i = 0; i < 10; i++) { g_now += 100; purrgo_app_tick(g_now); }
        if (with_gnss) { g_lat += 0.01; feed_epoch(g_lat); }
    }
}

static void scenario(bool with_gsa)
{
    g_with_gsa = with_gsa;
    g_lat = 33.54000;
    fake_fs_reset();
    fake_power_reset();
    purrgo_logger_reset();
    memset(&app_config, 0, sizeof app_config);
    app_config.last_lat_1e7 = 315590000;
    app_config.last_lon_1e7 = 354731666;
    g_now = 200000;
    purrgo_app_init();

    run_seconds(10, true);
    printf("before: logger=%d points=%zu file=%s\n", purrgo_logger_get_state(),
           purrgo_logger_get_track_point_count(),
           purrgo_logger_get_active_filename() ? purrgo_logger_get_active_filename() : "-");
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    CHECK(purrgo_logger_get_track_point_count() > 0);

    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);
    CHECK(purrgo_app_get_state() == APP_STATE_MENU_MAIN);
    purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);     /* BOOKS */
    printf("in books: state=%d logger=%d\n", purrgo_app_get_state(), purrgo_logger_get_state());
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);

    /* Эмулятор: байты NMEA продолжают приходить, пока читалка открыта. */
    run_seconds(10, true);

    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);     /* выход */
    printf("left: state=%d\n", purrgo_app_get_state());
    CHECK(purrgo_app_get_state() == APP_STATE_MENU_MAIN);

    run_seconds(15, true);
    printf("after: logger=%d points=%zu file=%s\n", purrgo_logger_get_state(),
           purrgo_logger_get_track_point_count(),
           purrgo_logger_get_active_filename() ? purrgo_logger_get_active_filename() : "-");
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    CHECK(purrgo_logger_get_track_point_count() > 0);
}

int main(void)
{
    scenario(false);
    scenario(true);
    return test_finish("test_book_resume_track");
}
