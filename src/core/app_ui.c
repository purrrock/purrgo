#include "purrgo/app_ui.h"
#include "purrgo/app_fsm.h"
#include "ui/ui_map.h"
#include "ui/ui_trip.h"
#include "ui/ui_config.h"
#include "ui/ui_dir_select.h"
#include "ui/ui_map_layers.h"
#include "purrgo/ui/ui_dirty.h"

#include "purrgo/gfx_text.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/hardware_config.h"
#include "purrgo/display_hal.h"
#include <stdbool.h>
#include <string.h>

int dbg_map_render_calls = 0;

static bool s_full_refresh_requested = false;

/* Данные последнего purrgo_app_ui_render() для финального экрана выключения. */
static gfx_context_t* s_last_gfx = NULL;
static purrgo_gnss_solution_t s_last_gnss;
static purrgo_sun_info_t s_last_sun;
static bool s_power_off_screen = false;
static purrgo_state_t prev_state = APP_STATE_MAP;

#define POWER_OFF_TEXT "POWER OFF"

/*
 * Режим дисплея по экрану: карта - 4 уровня серого, меню и маршрутный
 * компьютер - 1 бит (быстрое частичное обновление).
 */
static display_mode_t ui_display_mode(purrgo_state_t state)
{
    return (state == APP_STATE_MAP) ? DISPLAY_MODE_GRAY4 : DISPLAY_MODE_MONO;
}

bool purrgo_app_ui_is_power_off(void)
{
    return s_power_off_screen;
}

void purrgo_app_ui_request_full_refresh(void)
{
    s_full_refresh_requested = true;
}

static void ui_draw_key_hints(gfx_context_t* gfx, purrgo_state_t state, bool mark_region)
{
    int16_t y = PURRGO_HW_DISPLAY_HEIGHT_PX - 8;

    /* Clear the bottom area */
    if (s_power_off_screen) {
        /*
         * Инверсная нижняя строка вместо подсказок кнопок. Высота 9 строк
         * (как у верхней строки статуса): карта заканчивается на y = H - 9.
         * Область не регистрируется: экран выключения обновляется целиком.
         */
        int16_t bar_y = (int16_t)(PURRGO_HW_DISPLAY_HEIGHT_PX - 9);
        int16_t text_w = (int16_t)(6 * (sizeof(POWER_OFF_TEXT) - 1));

        gfx_set_color(gfx, WHITE, BLACK);
        gfx_fill_rect(gfx, 0, bar_y, PURRGO_HW_DISPLAY_WIDTH_PX, 9);
        gfx_draw_string(gfx, (int16_t)((PURRGO_HW_DISPLAY_WIDTH_PX - text_w) / 2),
                        (int16_t)(y + 1), POWER_OFF_TEXT);
        gfx_set_color(gfx, BLACK, WHITE);
        (void)mark_region;
        return;
    }

    gfx_set_color(gfx, BLACK, WHITE);
    gfx_fill_rect(gfx, 0, y, PURRGO_HW_DISPLAY_WIDTH_PX, 8);

    gfx_set_color(gfx, BLACK, WHITE);

    const char* k1 = "";
    const char* k2 = "";
    const char* k3 = "";
    const char* k4 = "";

    switch (state) {
        case APP_STATE_MAP:
            k1 = "\x1B" "/-";
            k2 = "\x1A" "/+";
            k3 = "\x18" "/CNT";
            k4 = "\x19" "/NXT";
            break;

        case APP_STATE_TRIP_COMPUTER:
            k4 = "NXT";
            break;

        case APP_STATE_MENU_CONFIG:
        case APP_STATE_MENU_DIR_SELECT:
        case APP_STATE_MENU_MAP_LAYERS:
            k1 = "\x18";
            k2 = "\x19";
            k3 = "SEL";
            k4 = "BCK";
            break;

        default:
            break;
    }

    int16_t step = PURRGO_HW_DISPLAY_WIDTH_PX / 4;

    if (k1[0]) gfx_draw_string(gfx, 2, (int16_t)(y + 1), k1);
    if (k2[0]) gfx_draw_string(gfx, (int16_t)(2 + step), (int16_t)(y + 1), k2);
    if (k3[0]) gfx_draw_string(gfx, (int16_t)(2 + step * 2), (int16_t)(y + 1), k3);
    if (k4[0]) gfx_draw_string(gfx, (int16_t)(2 + step * 3), (int16_t)(y + 1), k4);

    /*
     * Подсказки статичны для данного экрана, поэтому на экране карты
     * область помечается как изменённая только при смене экрана: все
     * остальные обновления карты (маркер, трек, строка статуса) сами
     * регистрируют свои области. Раньше любой проход render (например,
     * смена скорости) через эту отметку превращался в физический
     * refresh (на E-Ink - полный).
     *
     * На остальных экранах (например, Trip computer) модули не
     * регистрируют области сами, поэтому отметка сохраняется.
     */
    if (mark_region) {
        display_refresh_region(
            0,
            y,
            PURRGO_HW_DISPLAY_WIDTH_PX,
            8
        );
    }
}

void purrgo_app_ui_render(
    gfx_context_t* gfx,
    const purrgo_gnss_solution_t* gnss,
    const purrgo_sun_info_t* sun
) {
    purrgo_state_t current_state = purrgo_app_get_state();

    s_last_gfx = gfx;
    if (gnss != NULL) {
        s_last_gnss = *gnss;
    }
    if (sun != NULL) {
        s_last_sun = *sun;
    }

    /*
     * Общий флаг забирается до отрисовки: всё, что изменится во время
     * прохода, взведёт его заново и приведёт к следующему проходу.
     */
    purrgo_ui_dirty_clear(UI_DIRTY_SCREEN);

    /*
     * Режим дисплея выбирается до отрисовки и до refresh/flush: при смене
     * режима платформа сама сделает полное обновление.
     */
    display_set_mode(ui_display_mode(current_state));

    switch (current_state) {
        case APP_STATE_MENU_CONFIG:
            if (prev_state != APP_STATE_MENU_CONFIG) {
                ui_menu_config_reset();
            }
            ui_render_menu_config(gfx);
            break;
        case APP_STATE_TRIP_COMPUTER:
            if (prev_state != APP_STATE_TRIP_COMPUTER) {
                ui_trip_render_grid(gfx);
            }
            ui_trip_render_values(gfx, gnss, sun);
            /*
             * Экран не регистрирует изменённые области сам: значения
             * меняются по всей площади, поэтому обновляется весь экран.
             * В 1-bit режиме это частичное обновление без мигания.
             */
            display_refresh_region(0, 0, PURRGO_HW_DISPLAY_WIDTH_PX,
                                   PURRGO_HW_DISPLAY_HEIGHT_PX);
            break;
        case APP_STATE_MAP:
            ui_render_map(gfx, gnss, sun);
            break;
        case APP_STATE_MENU_DIR_SELECT:
            if (prev_state != APP_STATE_MENU_DIR_SELECT) {
                ui_menu_dir_select_reset();
            }
            ui_render_menu_dir_select(gfx);
            break;
        case APP_STATE_MENU_MAP_LAYERS:
            if (prev_state != APP_STATE_MENU_MAP_LAYERS) {
                ui_menu_map_layers_reset();
            }
            ui_render_menu_map_layers(gfx);
            break;
        default:
            break;
    }

    ui_draw_key_hints(gfx, current_state,
                      (current_state != prev_state) ||
                      (current_state != APP_STATE_MAP));

    if (current_state != prev_state || s_full_refresh_requested) {
        display_refresh();
    } else {
        display_flush();
    }

    s_full_refresh_requested = false;

    prev_state = current_state;
}

bool purrgo_app_ui_render_power_off(void)
{
    if (s_last_gfx == NULL) {
        return false;
    }

    s_power_off_screen = true;

    /* Экран выключения - это карта: режим 4 gray (не зависит от текущего экрана). */
    display_set_mode(DISPLAY_MODE_GRAY4);

    /*
     * Показываем карту независимо от текущего экрана (выключение из меню):
     * карта и строка статуса перерисовываются целиком.
     */
    purrgo_ui_dirty_mark(UI_DIRTY_MAP);
    purrgo_ui_dirty_mark(UI_DIRTY_STATUS);
    ui_render_map(s_last_gfx, &s_last_gnss, &s_last_sun);
    ui_draw_key_hints(s_last_gfx, APP_STATE_MAP, false);

    /* Платформа сделает полный refresh сама. */
    s_full_refresh_requested = false;
    return true;
}

void purrgo_app_ui_reset(void)
{
    s_last_gfx = NULL;
    memset(&s_last_gnss, 0, sizeof(s_last_gnss));
    memset(&s_last_sun, 0, sizeof(s_last_sun));
    s_power_off_screen = false;
    s_full_refresh_requested = false;
    prev_state = APP_STATE_MAP;
    ui_map_reset();
    purrgo_ui_dirty_mark(UI_DIRTY_MAP);
    purrgo_ui_dirty_mark(UI_DIRTY_STATUS);
}
