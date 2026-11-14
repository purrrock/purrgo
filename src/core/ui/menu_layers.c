#include "menu_internal.h"
#include "purrgo/config.h"

/*
 * Экран MAP LAYERS: включение и выключение слоёв карты.
 */

static int cursor;
static uint16_t draft_layers;


static void on_enter(void)
{
    draft_layers = app_config.layers;
    cursor = 0;
}


static bool on_button(purrgo_action_t button, purrgo_state_t* next)
{
    switch (button) {
        case PURRGO_ACTION_UP:
            cursor = menu_cursor_move(cursor, -1, PURRGO_LAYER_COUNT, true);
            return true;

        case PURRGO_ACTION_DOWN:
            cursor = menu_cursor_move(cursor, +1, PURRGO_LAYER_COUNT, true);
            return true;

        case PURRGO_ACTION_OK:
            draft_layers = (uint16_t)(draft_layers ^ (1u << cursor));
            return true;

        case PURRGO_ACTION_MENU:
            app_config.layers = draft_layers;
            purrgo_config_save();
            *next = APP_STATE_MENU_MAIN;
            return true;

        default:
            return false;
    }
}


const menu_screen_t menu_screen_map_layers = {
    .state     = APP_STATE_MENU_MAP_LAYERS,
    .on_enter  = on_enter,
    .on_button = on_button,
};


int menu_app_get_map_layers_cursor(void) { return cursor; }
uint16_t menu_app_get_draft_layers(void) { return draft_layers; }
