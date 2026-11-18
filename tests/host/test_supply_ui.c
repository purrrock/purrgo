/*
 * Индикатор остановки записи по питанию на экранах (purrgo/supply.h):
 *   - карта: вместо REC/NO-REC в строке статуса постоянный инверсный STOP:<причина>,
 *     пропадает после возврата питания к норме;
 *   - путевой компьютер: инверсная полоса "REC STOP: LOW <причина>" в верхней строке
 *     (поверх строк данных не рисуется), без блокировки полоса чистая;
 *   - остановка записи и смена причины перерисовывают экран сразу (без отсрочки
 *     перерисовки по данным);
 *   - причины VDD / батарея / обе выводятся полным текстом.
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/app_ui.h"
#include "purrgo/battery.h"
#include "purrgo/config.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_text.h"
#include "purrgo/hardware_config.h"
#include "purrgo/supply.h"
#include "purrgo/ui/ui_dirty.h"
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

/* Шаг 10 мс: фикс обновляется раз в секунду, чтобы не истекал таймаут GNSS. */
static void run_ms(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 10) {
        g_now += 10;
        if (g_now % 1000u == 0u) { feed_epoch(); }
        purrgo_app_tick(g_now);
    }
}

static int count_px(int x0, int y0, int x1, int y1, uint8_t v)
{
    int n = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) if (g_fb[y * W + x] == v) n++;
    return n;
}

static void render(gfx_context_t *g)
{
    purrgo_app_ui_render(g, purrgo_app_get_gnss_solution(), NULL);
}

static void app_start(gfx_context_t *g, purrgo_power_source_t src)
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
    app_config.power_source = src;      /* после init: он загружает конфиг и сбрасывает источник */
    feed_epoch();
    run_ms(3000);
    CHECK(purrgo_app_get_gnss_solution()->valid);
    CHECK(!purrgo_supply_storage_blocked());
    memset(g_fb, 0, sizeof g_fb);
    CHECK(gfx_init(g, W, H, g_fb, cb_draw, cb_read));
}

/* Время, за которое гарантированно набирается остановка по батарее. */
#define BATT_STOP_RUN_MS \
    (PURRGO_SUPPLY_BATT_POLL_MS * (PURRGO_SUPPLY_BATT_CONFIRM_POLLS + 1u) + PURRGO_SUPPLY_LOW_CONFIRM_MS + 100u)

static void set_pack_mv(uint32_t pack_mv)
{
    fake_power_set_battery_pin_mv(fake_power_pin_for_pack_mv(pack_mv));
}

/* Карта: STOP:<причина> вместо статуса записи. */
static void test_map_indicator(void)
{
    gfx_context_t g;

    app_start(&g, PURRGO_POWER_SRC_NIMH);
    CHECK(purrgo_app_get_state() == APP_STATE_MAP);
    for (int i = 0; i < 3; i++) { run_ms(1000); render(&g); }

    /* Запись идёт: строка статуса светлая, в зоне статуса записи нет инверсной плашки. */
    const int chip_area = 51 * 9;
    CHECK(count_px(60, 0, 111, 9, PX_BLACK) < chip_area / 4);
    CHECK(g_fb[0 * W + 60] == PX_WHITE);

    /* Просадка VDD: остановка записи сразу ставит экран в очередь на перерисовку. */
    purrgo_ui_dirty_clear(UI_DIRTY_SCREEN | UI_DIRTY_MAP | UI_DIRTY_STATUS);
    CHECK(!purrgo_app_ui_needs_render(g_now));
    fake_power_set_supply_low(true);
    run_ms(PURRGO_SUPPLY_LOW_CONFIRM_MS + 20u);
    CHECK(purrgo_supply_storage_blocked());
    CHECK(purrgo_ui_dirty_test(UI_DIRTY_STATUS));
    CHECK(purrgo_app_ui_needs_render(g_now));

    render(&g);
    /* Инверсная плашка STOP:VDD: левый край чёрный, большая часть области чёрная, текст белый. */
    CHECK(g_fb[0 * W + 60] == PX_BLACK);
    CHECK(count_px(60, 0, 111, 9, PX_BLACK) > chip_area * 55 / 100);
    CHECK(count_px(60, 0, 111, 9, PX_WHITE) > 0);

    /* Питание вернулось: плашка уходит, строка снова светлая. */
    fake_power_set_supply_low(false);
    run_ms(PURRGO_SUPPLY_RECOVER_MS + 2000u);
    CHECK(!purrgo_supply_storage_blocked());
    CHECK(purrgo_app_ui_needs_render(g_now));
    render(&g);
    CHECK(count_px(60, 0, 111, 9, PX_BLACK) < chip_area / 4);
    CHECK(g_fb[0 * W + 60] == PX_WHITE);
}

/* Рисует эталон полосы путевого компьютера и сравнивает с верхней строкой экрана. */
static int banner_diff(const char *text)
{
    static uint8_t saved[W * H];
    static uint8_t ref[W * H];
    gfx_context_t rg;
    int diff = 0;

    memcpy(saved, g_fb, sizeof saved);
    memset(g_fb, 0, sizeof g_fb);
    CHECK(gfx_init(&rg, W, H, g_fb, cb_draw, cb_read));
    gfx_set_color(&rg, WHITE, BLACK);
    gfx_fill_rect(&rg, 0, 0, W, 9);
    gfx_draw_string(&rg, (int16_t)((W - 6 * (int)strlen(text)) / 2), 1, text);
    memcpy(ref, g_fb, sizeof ref);
    memcpy(g_fb, saved, sizeof saved);

    for (int y = 0; y < 9; y++)
        for (int x = 0; x < W; x++)
            if (g_fb[y * W + x] != ref[y * W + x]) diff++;
    return diff;
}

/* Путевой компьютер: полоса с причиной, смена причины перерисовывает экран сразу. */
static void test_trip_indicator(void)
{
    gfx_context_t g;

    app_start(&g, PURRGO_POWER_SRC_LIION_AA);
    set_pack_mv(3800u);                                   /* батареи в норме */
    run_ms(2000u);
    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);       /* KEY4 SHORT на карте сдвигает её */
    CHECK(purrgo_app_get_state() == APP_STATE_TRIP_COMPUTER);
    run_ms(1000);
    render(&g);

    /* Без блокировки верх экрана чистый, строки данных на месте. */
    CHECK(count_px(0, 0, W, 9, PX_BLACK) == 0);
    CHECK(count_px(10, 12, 100, 24, PX_BLACK) > 0);

    /* Просадка VDD: полоса "REC STOP: LOW VDD". */
    purrgo_ui_dirty_clear(UI_DIRTY_SCREEN | UI_DIRTY_MAP | UI_DIRTY_STATUS);
    CHECK(!purrgo_app_ui_needs_render(g_now));
    fake_power_set_supply_low(true);
    run_ms(PURRGO_SUPPLY_LOW_CONFIRM_MS + 20u);
    CHECK(purrgo_supply_storage_blocked());
    CHECK(purrgo_app_ui_needs_render(g_now));             /* без отсрочки по данным */
    render(&g);
    CHECK(banner_diff("REC STOP: LOW VDD") == 0);
    CHECK(count_px(10, 12, 100, 24, PX_BLACK) > 0);       /* данные не затёрты */

    /* К VDD добавилась батарея: другая причина - другой текст и сразу перерисовка. */
    purrgo_ui_dirty_clear(UI_DIRTY_SCREEN | UI_DIRTY_STATUS);
    set_pack_mv(2600u);
    run_ms(BATT_STOP_RUN_MS);
    CHECK(purrgo_supply_reason() == (PURRGO_SUPPLY_REASON_VDD | PURRGO_SUPPLY_REASON_BATT));
    CHECK(purrgo_app_ui_needs_render(g_now));
    render(&g);
    CHECK(banner_diff("REC STOP: LOW BATT+VDD") == 0);

    /* VDD в норме, батарея остаётся: полоса про батарею. */
    purrgo_ui_dirty_clear(UI_DIRTY_SCREEN | UI_DIRTY_STATUS);
    fake_power_set_supply_low(false);
    run_ms(500u);
    CHECK(purrgo_supply_storage_blocked());
    CHECK(purrgo_app_ui_needs_render(g_now));
    render(&g);
    CHECK(banner_diff("REC STOP: LOW BATT") == 0);

    /* Источник питания сменили на 5 В: запись разрешена, полоса исчезла. */
    app_config.power_source = PURRGO_POWER_SRC_EXTERNAL_5V;
    run_ms(PURRGO_SUPPLY_RECOVER_MS + 3000u);
    CHECK(!purrgo_supply_storage_blocked());
    CHECK(purrgo_app_ui_needs_render(g_now));
    render(&g);
    CHECK(count_px(0, 0, W, 9, PX_BLACK) == 0);
    CHECK(count_px(10, 12, 100, 24, PX_BLACK) > 0);
}

/* Другие экраны (меню) остановку записи не перерисовывают: индикатора там нет. */
static void test_menu_not_redrawn(void)
{
    gfx_context_t g;

    app_start(&g, PURRGO_POWER_SRC_NIMH);
    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);       /* карта -> путевой компьютер */
    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);       /* путевой компьютер -> меню */
    CHECK(purrgo_app_get_state() == APP_STATE_MENU_MAIN);
    for (int i = 0; i < 3; i++) { run_ms(1000); render(&g); }

    purrgo_ui_dirty_clear(UI_DIRTY_SCREEN | UI_DIRTY_MAP | UI_DIRTY_STATUS);
    fake_power_set_supply_low(true);
    run_ms(PURRGO_SUPPLY_LOW_CONFIRM_MS + 20u);
    CHECK(purrgo_supply_storage_blocked());
    CHECK(!purrgo_app_ui_needs_render(g_now));
}

int main(void)
{
    test_map_indicator();
    test_trip_indicator();
    test_menu_not_redrawn();
    return test_finish("test_supply_ui");
}
