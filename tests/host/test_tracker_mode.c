/*
 * Режим "трекер с отключённым дисплеем" (пункт меню TRACKER (SCREEN OFF)):
 *   - пункт стоит сразу ниже POWER OFF;
 *   - вход: логгер переходит в EXPEDITION (настройка LOG в app_config не меняется),
 *     на экран один раз выводится предупреждение;
 *   - в режиме запись трека идёт, а перерисовок больше нет (смена минуты, фикса,
 *     состояния логгера, обычные кнопки);
 *   - выход только по PURRGO_BTN_COMBO_K1_K4: возврат на карту, логгеру
 *     возвращается режим из настроек, карта перерисовывается;
 *   - вне режима трекера комбинация игнорируется.
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/app_ui.h"
#include "purrgo/config.h"
#include "purrgo/menu_controller.h"
#include "purrgo/track_logger.h"
#include "purrgo/hardware_config.h"
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

/*
 * Эпоха с заданной минутой (UTC); step сдвигает позицию на север примерно на
 * 150 м за единицу, чтобы фильтр трека (EXPEDITION: 100 м) принимал точки.
 */
static void feed_epoch(int minute, int step)
{
    char rmc[160], gga[160];
    const int lat_min_1e5 = 3354000 + step * 8000;     /* 33.54000' + step * 0.08' */
    snprintf(rmc, sizeof rmc,
             "GPRMC,09%02d15.00,A,31%02d.%05d,N,03528.39000,E,0.50,90.00,191026,,,A",
             minute, lat_min_1e5 / 100000, lat_min_1e5 % 100000);
    snprintf(gga, sizeof gga,
             "GPGGA,09%02d15.00,31%02d.%05d,N,03528.39000,E,1,08,1.1,100.0,M,18.9,M,,",
             minute, lat_min_1e5 / 100000, lat_min_1e5 % 100000);
    feed(rmc);
    feed(gga);
}

static void run_ticks(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 100) { g_now += 100; purrgo_app_tick(g_now); }
}

static int count_px(uint8_t v)
{
    int n = 0;
    for (int i = 0; i < W * H; i++) if (g_fb[i] == v) n++;
    return n;
}

static void test_tracker_mode(void)
{
    gfx_context_t g;
    char text[64];

    fake_fs_reset();
    memset(&app_config, 0, sizeof app_config);
    g_now = 200000;
    purrgo_logger_reset();
    purrgo_app_ui_reset();
    purrgo_app_init();                 /* загружает настройки по умолчанию: LOG = STANDARD */
    app_config.log_mode = LOGGER_MODE_STANDARD;
    app_config.last_lat_1e7 = 315590000;
    app_config.last_lon_1e7 = 354731666;
    purrgo_logger_set_mode(app_config.log_mode);
    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));

    run_ticks(1000);
    feed_epoch(30, 0);
    run_ticks(1000);
    CHECK(purrgo_app_get_gnss_solution()->valid);
    purrgo_app_ui_render(&g, purrgo_app_get_gnss_solution(), NULL);

    /* Меню: сразу после POWER OFF идёт пункт трекера. */
    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);     /* карта -> Trip computer */
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);    /* Trip computer -> меню */
    CHECK(purrgo_app_get_state() == APP_STATE_MENU_MAIN);
    menu_app_get_main_item_text(2, text, sizeof text);
    CHECK(strcmp(text, "POWER OFF") == 0);
    menu_app_get_main_item_text(3, text, sizeof text);
    CHECK(strncmp(text, "TRACKER", 7) == 0);

    /* Вне режима трекера комбинация ничего не делает. */
    purrgo_app_handle_button(PURRGO_BTN_COMBO_K1_K4);
    CHECK(purrgo_app_get_state() == APP_STATE_MENU_MAIN);

    /* Курсор на пункт трекера (3 раза вниз) и выбор. */
    for (int i = 0; i < 3; i++) { purrgo_app_handle_button(PURRGO_BTN_KEY2_SHORT); }
    CHECK(menu_app_get_main_cursor() == 3);
    CHECK(!purrgo_app_tracker_mode_active());
    CHECK(purrgo_logger_get_mode() == LOGGER_MODE_STANDARD);
    purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);
    CHECK(purrgo_app_get_state() == APP_STATE_TRACKER_SCREEN_OFF);
    CHECK(purrgo_app_tracker_mode_active());

    /* Запись как в EXPEDITION; настройка пользователя не тронута. */
    CHECK(purrgo_logger_get_mode() == LOGGER_MODE_EXPEDITION);
    CHECK(app_config.log_mode == LOGGER_MODE_STANDARD);

    /* Один проход отрисовки: предупреждение (есть и чёрные, и белые пиксели). */
    CHECK(purrgo_app_ui_needs_render(g_now));
    memset(g_fb, 0, sizeof g_fb);
    purrgo_app_ui_render(&g, purrgo_app_get_gnss_solution(), NULL);
    CHECK(count_px(BLACK + 1) > 500);
    CHECK(count_px(WHITE + 1) > 500);
    CHECK(!purrgo_app_ui_needs_render(g_now));

    /* Запись идёт, а экран больше не требует отрисовки: минуты, позиция, кнопки. */
    for (int m = 31; m < 40; m++) {
        feed_epoch(m, m - 30);
        run_ticks(10000);
        CHECK(!purrgo_app_ui_needs_render(g_now));
    }
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    CHECK(purrgo_logger_get_track_point_count() > 1);

    const purrgo_btn_t other[] = {
        PURRGO_BTN_KEY1_SHORT, PURRGO_BTN_KEY2_SHORT, PURRGO_BTN_KEY3_SHORT,
        PURRGO_BTN_KEY4_SHORT, PURRGO_BTN_KEY1_LONG,  PURRGO_BTN_KEY2_LONG,
        PURRGO_BTN_KEY3_LONG,  PURRGO_BTN_KEY4_LONG
    };
    for (size_t i = 0; i < sizeof other / sizeof other[0]; i++) {
        purrgo_app_handle_button(other[i]);
        CHECK(purrgo_app_get_state() == APP_STATE_TRACKER_SCREEN_OFF);
        CHECK(!purrgo_app_ui_needs_render(g_now));
    }
    CHECK(purrgo_logger_get_mode() == LOGGER_MODE_EXPEDITION);

    /* Выход: K1+K4. */
    const size_t points_before = purrgo_logger_get_track_point_count();
    purrgo_app_handle_button(PURRGO_BTN_COMBO_K1_K4);
    CHECK(purrgo_app_get_state() == APP_STATE_MAP);
    CHECK(!purrgo_app_tracker_mode_active());
    CHECK(purrgo_logger_get_mode() == LOGGER_MODE_STANDARD);
    CHECK(purrgo_app_ui_needs_render(g_now));
    CHECK(purrgo_logger_get_track_point_count() >= points_before);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);   /* трек не прерывался */

    /* Теперь на карте комбинация снова ничего не делает. */
    purrgo_app_ui_render(&g, purrgo_app_get_gnss_solution(), NULL);
    purrgo_app_handle_button(PURRGO_BTN_COMBO_K1_K4);
    CHECK(purrgo_app_get_state() == APP_STATE_MAP);
}

/* Если в настройках запись выключена, выход из трекера останавливает логгер. */
static void test_tracker_exit_restores_off(void)
{
    gfx_context_t g;

    fake_fs_reset();
    memset(&app_config, 0, sizeof app_config);
    g_now = 500000;
    purrgo_logger_reset();
    purrgo_app_ui_reset();
    purrgo_app_init();
    app_config.log_mode = LOGGER_MODE_OFF;      /* после init: он загружает настройки по умолчанию */
    purrgo_logger_set_mode(app_config.log_mode);
    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));

    run_ticks(1000);
    feed_epoch(10, 0);
    run_ticks(1000);
    purrgo_app_ui_render(&g, purrgo_app_get_gnss_solution(), NULL);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);

    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);
    for (int i = 0; i < 3; i++) { purrgo_app_handle_button(PURRGO_BTN_KEY2_SHORT); }
    purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);
    CHECK(purrgo_app_get_state() == APP_STATE_TRACKER_SCREEN_OFF);

    /* Даже при LOG: OFF в режиме трекера запись идёт. */
    feed_epoch(11, 1);
    run_ticks(3000);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);

    purrgo_app_handle_button(PURRGO_BTN_COMBO_K1_K4);
    CHECK(purrgo_app_get_state() == APP_STATE_MAP);
    CHECK(purrgo_logger_get_mode() == LOGGER_MODE_OFF);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);
}

int main(void)
{
    test_tracker_mode();
    test_tracker_exit_restores_off();
    return test_finish("test_tracker_mode");
}
