/*
 * Финальный экран выключения (меню "POWER OFF" или разряд батареи):
 *   - карта (с маркером) остаётся на экране независимо от текущего экрана;
 *   - верхняя и нижняя строки статуса - инверсный текст "POWER OFF";
 *   - в верхней строке сохраняется масштаб карты;
 *   - purrgo_app_ui_render_power_off() ничего не делает, если рендера не было.
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/app_ui.h"
#include "purrgo/config.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_text.h"
#include "purrgo/hardware_config.h"
#include "purrgo/map.h"
#include "purrgo/sun.h"
#include "purrgo/ui/ui_map_dirty.h"
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

/* Значение в g_fb: цвет + 1 (0 - пиксель не рисовался). */
#define PX_BLACK (BLACK + 1)
#define PX_WHITE (WHITE + 1)

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

static int count_px(int x0, int y0, int x1, int y1, uint8_t v)
{
    int n = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) if (g_fb[y * W + x] == v) n++;
    return n;
}

static void test_power_off_screen(void)
{
    static uint8_t ref[W * H];
    gfx_context_t g;

    /* Рендера ещё не было: функция безопасно отказывается. */
    CHECK(!purrgo_app_ui_is_power_off());
    CHECK(!purrgo_app_ui_render_power_off());
    CHECK(!purrgo_app_ui_is_power_off());

    fake_fs_reset();
    memset(&app_config, 0, sizeof app_config);
    app_config.last_lat_1e7 = 315590000;        /* центр карты - у фикса, маркер в середине области */
    app_config.last_lon_1e7 = 354731666;
    g_now = 200000;
    purrgo_app_init();
    run_ticks(1000);
    feed_epoch();
    CHECK(purrgo_app_get_gnss_solution()->valid);

    memset(g_fb, 0, sizeof g_fb);
    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));

    /* Обычный проход на карте: маркер у центра, строки статуса обычные (не инверсные). */
    for (int i = 0; i < 6; i++) {
        run_ticks(1000);
        feed_epoch();
        purrgo_app_ui_render(&g, purrgo_app_get_gnss_solution(), NULL);
    }
    CHECK(count_px(0, 0, W, 9, PX_BLACK) < W * 9 / 2);          /* верхняя строка светлая */
    /* Слои карты выключены, поэтому чёрные пиксели области карты - это маркер. */
    int marker_px = count_px(0, 9, W, H - 9, PX_BLACK);
    CHECK(marker_px > 0);

    /* Пользователь ушёл в Trip computer: на экране его текст. */
    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);      /* KEY4 SHORT на карте сдвигает её */
    CHECK(purrgo_app_get_state() == APP_STATE_TRIP_COMPUTER);
    run_ticks(1000);
    feed_epoch();
    purrgo_app_ui_render(&g, purrgo_app_get_gnss_solution(), NULL);
    CHECK(count_px(10, 12, 100, 24, PX_BLACK) > 0);             /* строка "UTC:" и т.п. */

    /* Выключение. */
    CHECK(purrgo_app_ui_render_power_off());
    CHECK(purrgo_app_ui_is_power_off());

    /* Верхняя строка: чёрная, с белым "POWER OFF" слева. */
    memset(ref, 0, sizeof ref);
    {
        uint8_t saved[W * H];
        memcpy(saved, g_fb, sizeof saved);
        memset(g_fb, 0, sizeof g_fb);
        gfx_context_t rg;
        CHECK(gfx_init(&rg, W, H, g_fb, cb_draw, cb_read));
        gfx_set_color(&rg, WHITE, BLACK);
        gfx_fill_rect(&rg, 0, 0, W, 9);
        gfx_draw_string(&rg, 2, 1, "POWER OFF");
        gfx_fill_rect(&rg, 0, H - 9, W, 9);
        gfx_draw_string(&rg, (W - 54) / 2, H - 8 + 1, "POWER OFF");
        memcpy(ref, g_fb, sizeof ref);
        memcpy(g_fb, saved, sizeof saved);
    }
    int top_diff = 0, bottom_diff = 0;
    for (int y = 0; y < 9; y++)
        for (int x = 0; x < 2 + 54 + 1; x++)                    /* область текста "POWER OFF" */
            if (g_fb[y * W + x] != ref[y * W + x]) top_diff++;
    for (int y = H - 9; y < H; y++)
        for (int x = 0; x < W; x++)                              /* вся нижняя строка */
            if (g_fb[y * W + x] != ref[y * W + x]) bottom_diff++;
    CHECK(top_diff == 0);
    CHECK(bottom_diff == 0);

    /* Верхняя строка вне текста "POWER OFF": чёрный фон и белый масштаб справа. */
    CHECK(count_px(70, 0, W, 9, PX_WHITE) > 0);                  /* подпись масштаба видна */
    CHECK(count_px(70, 0, W, 9, PX_BLACK) > count_px(70, 0, W, 9, PX_WHITE));
    CHECK(count_px(0, 0, W, 9, 0) == 0);                         /* все пиксели строки нарисованы */

    /* Карта осталась: чёрных пикселей столько же, сколько у одного маркера, то есть
       текст Trip computer стёрт, а маркер на месте. */
    CHECK(count_px(0, 9, W, H - 9, PX_BLACK) == marker_px);      /* маркер на прежнем месте */

    /* purrgo_app_ui_reset(): экран выключения снят, данные последнего рендера забыты. */
    purrgo_app_ui_reset();
    CHECK(!purrgo_app_ui_is_power_off());
    CHECK(!purrgo_app_ui_render_power_off());
}

int main(void)
{
    test_power_off_screen();
    return test_finish("test_power_off_screen");
}
