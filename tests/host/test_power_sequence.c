/*
 * Host-тест последовательности выключения (purrgo_system_power_off, src/core/power.c):
 * к моменту размонтирования носителя логгер остановлен, конфиг записан, файлы карты
 * и трека закрыты (ни одного открытого дескриптора), финальный экран отрисован;
 * размонтирование идёт строго раньше входа в низкое энергопотребление.
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/app_ui.h"
#include "purrgo/config.h"
#include "purrgo/fs_hal.h"
#include "purrgo/map.h"
#include "purrgo/power.h"
#include "purrgo/track_logger.h"
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

static void run_ticks(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 100) { g_now += 100; purrgo_app_tick(g_now); }
}

/* Состояние в момент purrgo_platform_storage_unmount(). */
static struct {
    int open_handles;
    track_logger_state_t logger_state;
    bool config_saved;
    bool screen_rendered;
} g_at_unmount;

static char g_cfg_before[2048];
static uint32_t g_cfg_before_size;

static void probe(void)
{
    uint32_t size = 0;
    g_at_unmount.open_handles = fake_fs_open_handles();
    g_at_unmount.logger_state = purrgo_logger_get_state();
    /* Конфиг записан именно при выключении: содержимое отличается от прежнего. */
    const uint8_t *cfg = fake_fs_data(purrgo_fs_get_config_path(), &size);
    g_at_unmount.config_saved = cfg != NULL && size > 0 &&
        (size != g_cfg_before_size || memcmp(cfg, g_cfg_before, size) != 0);
    g_at_unmount.screen_rendered = purrgo_app_ui_is_power_off();
}

static void test_power_off_sequence(void)
{
    gfx_context_t g;
    static const uint8_t junk[64] = { 0 };

    fake_fs_reset();
    fake_power_reset();
    memset(&app_config, 0, sizeof app_config);
    app_config.last_lat_1e7 = 315590000;
    app_config.last_lon_1e7 = 354731666;
    purrgo_logger_reset();
    purrgo_logger_set_mode(LOGGER_MODE_STANDARD);
    g_now = 200000;
    purrgo_app_init();
    run_ticks(1000);
    feed_epoch();
    CHECK(purrgo_app_get_gnss_solution()->valid);

    memset(g_fb, 0, sizeof g_fb);
    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));
    for (int i = 0; i < 3; i++) {
        run_ticks(1000);
        feed_epoch();
        purrgo_app_ui_render(&g, purrgo_app_get_gnss_solution(), NULL);
    }

    /* Открытый трек: логгер пишет в файл (обычно его уже запустил конечный автомат). */
    if (purrgo_logger_get_state() != LOGGER_STATE_RECORDING) {
        purrgo_gnss_solution_t fix = *purrgo_app_get_gnss_solution();
        CHECK(purrgo_logger_start(&fix));
    }
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);

    /* Открытые файлы карты (содержимое неважно: слой откроется, рендер вернёт false). */
    fake_fs_put_bin("maps/m/roads.idx", junk, sizeof junk);
    fake_fs_put_bin("maps/m/roads.mlp", junk, sizeof junk);
    purrgo_map_render_params_t params;
    memset(&params, 0, sizeof params);
    params.map_dir = "maps/m";
    params.layer_roads = true;
    const purrgo_viewport_t vp = { W, H, 0, 0 };
    const purrgo_bbox_t cam = { 0, 0, 1000, 1000 };
    (void)purrgo_map_render_viewport(&g, &vp, &cam, &params);
    CHECK(fake_fs_open_handles() >= 3);        /* трек + два файла карты */

    /* Снимок конфига до выключения и изменение настройки, которую нужно сохранить. */
    {
        uint32_t size = 0;
        const uint8_t *cfg = fake_fs_data(purrgo_fs_get_config_path(), &size);
        CHECK(cfg != NULL && size > 0 && size <= sizeof g_cfg_before);
        if (cfg != NULL && size <= sizeof g_cfg_before) {
            memcpy(g_cfg_before, cfg, size);
            g_cfg_before_size = size;
        }
    }
    app_config.tz_offset_minutes = (int16_t)(app_config.tz_offset_minutes + 180);

    fake_power_set_unmount_probe(probe);
    purrgo_system_power_off();

    CHECK(fake_power_unmount_calls() == 1);
    CHECK(fake_power_standby_calls() == 1);
    CHECK(fake_power_unmount_seq() < fake_power_standby_seq());

    CHECK(g_at_unmount.open_handles == 0);                       /* карта и трек закрыты */
    CHECK(g_at_unmount.logger_state != LOGGER_STATE_RECORDING);   /* логгер остановлен */
    CHECK(g_at_unmount.config_saved);                            /* PURRGO.CFG записан */
    CHECK(g_at_unmount.screen_rendered);                         /* финальный экран нарисован */
}

int main(void)
{
    test_power_off_sequence();
    return test_finish("test_power_sequence");
}
