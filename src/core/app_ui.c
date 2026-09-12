#include "purrgo/app_ui.h"
#include "purrgo/app_fsm.h"
#include "ui/ui_map.h"
#include "ui/ui_trip.h"
#include "ui/ui_config.h"
#include "ui/ui_dir_select.h"
#include "ui/ui_map_layers.h"

#include "purrgo/gfx_text.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/hardware_config.h"

int dbg_map_render_calls = 0;

static void ui_draw_key_hints(gfx_context_t* gfx, purrgo_state_t state) {
    int y = PURRGO_HW_DISPLAY_HEIGHT_PX - 8;

    // Clear the bottom area
    gfx_set_color(gfx, BLACK, WHITE);
    gfx_fill_rect(gfx, 0, y, PURRGO_HW_DISPLAY_WIDTH_PX, 8);

    gfx_set_color(gfx, BLACK, WHITE);

    const char* k1 = "";
    const char* k2 = "";
    const char* k3 = "";
    const char* k4 = "";

    switch (state) {
        case APP_STATE_MAP:
            k1 = "\x1B" "/-"; // стрелка влево
            k2 = "\x1A" "/+"; // стрелка вправо
            k3 = "\x18" "/CNT"; // стрелка вниз
            k4 = "\x19" "/NXT"; // стрелка вверх
            break;
        case APP_STATE_TRIP_COMPUTER:
            k1 = "";
            k2 = "";
            k3 = "";
            k4 = "NXT";
            break;
        case APP_STATE_MENU_CONFIG:
        case APP_STATE_MENU_DIR_SELECT:
        case APP_STATE_MENU_MAP_LAYERS:
            k1 = "\x18"; // стрелка вниз
            k2 = "\x19"; // стрелка вверх
            k3 = "SEL";
            k4 = "BCK";
            break;
        default:
            break;
    }

    int step = PURRGO_HW_DISPLAY_WIDTH_PX / 4;
    if (k1[0]) gfx_draw_string(gfx, 2, y + 1, k1);
    if (k2[0]) gfx_draw_string(gfx, 2 + step, y + 1, k2);
    if (k3[0]) gfx_draw_string(gfx, 2 + step * 2, y + 1, k3);
    if (k4[0]) gfx_draw_string(gfx, 2 + step * 3, y + 1, k4);
}

void purrgo_app_ui_render(
    gfx_context_t* gfx,
    const purrgo_gnss_solution_t* gnss,
    const purrgo_sun_info_t* sun
) {
    static purrgo_state_t prev_state = APP_STATE_MAP;
    purrgo_state_t current_state = purrgo_app_get_state();

    switch (current_state) {
        case APP_STATE_MENU_CONFIG:
            ui_render_menu_config(gfx);
            break;
        case APP_STATE_TRIP_COMPUTER:
            if (prev_state != APP_STATE_TRIP_COMPUTER) {
                ui_trip_render_grid(gfx);
            }
            ui_trip_render_values(gfx, gnss, sun);
            break;
        case APP_STATE_MAP:
            ui_render_map(gfx, gnss, sun);
            break;
        case APP_STATE_MENU_DIR_SELECT:
            ui_render_menu_dir_select(gfx);
            break;
        case APP_STATE_MENU_MAP_LAYERS:
            ui_render_menu_map_layers(gfx);
            break;
        default:
            break;
    }

    ui_draw_key_hints(gfx, current_state);

    prev_state = current_state;
}
