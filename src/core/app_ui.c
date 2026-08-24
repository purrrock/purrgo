#include "purrgo/app_ui.h"
#include "purrgo/app_fsm.h"
#include "ui/ui_map.h"
#include "ui/ui_trip.h"
#include "ui/ui_config.h"
#include "ui/ui_dir_select.h"

int dbg_map_render_calls = 0;

void purrgo_app_ui_render(
    gfx_context_t* gfx,
    const purrgo_gnss_solution_t* gnss,
    const purrgo_sun_info_t* sun
) {
    switch (purrgo_app_get_state()) {
        case APP_STATE_MENU_CONFIG:
            ui_render_menu_config(gfx);
            break;
        case APP_STATE_TRIP_COMPUTER:
            ui_render_trip_computer(gfx, gnss, sun);
            break;
        case APP_STATE_MAP:
            ui_render_map(gfx, gnss, sun);
            break;
        case APP_STATE_MENU_DIR_SELECT:
            ui_render_menu_dir_select(gfx);
            break;
        default:
            break;
    }
}
