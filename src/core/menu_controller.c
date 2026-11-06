#include "menu_internal.h"

/*
 * Диспетчер меню: находит экран по состоянию FSM и передаёт ему события.
 * Вся логика конкретных экранов - в menu_main.c, menu_layers.c и
 * menu_dir_browser.c.
 */

static const menu_screen_t* const screens[] = {
    &menu_screen_main,
    &menu_screen_map_layers,
    &menu_screen_dir_select,
    &menu_screen_route_select,
    &menu_screen_wpt_select,
};

#define SCREEN_COUNT (sizeof(screens) / sizeof(screens[0]))


static const menu_screen_t* find_screen(purrgo_state_t state)
{
    for (size_t i = 0; i < SCREEN_COUNT; i++) {
        if (screens[i]->state == state) {
            return screens[i];
        }
    }
    return NULL;
}


bool purrgo_menu_controller_owns_state(purrgo_state_t state)
{
    return find_screen(state) != NULL;
}


void purrgo_menu_controller_init(void)
{
    purrgo_menu_controller_on_enter(APP_STATE_MENU_MAIN);
}


void purrgo_menu_controller_on_enter(purrgo_state_t state)
{
    const menu_screen_t* screen = find_screen(state);

    if (screen != NULL && screen->on_enter != NULL) {
        screen->on_enter();
    }
}


bool purrgo_menu_controller_handle_button(
    purrgo_state_t current_state,
    purrgo_action_t button,
    purrgo_state_t* next_state_out
)
{
    /* По умолчанию остаёмся в текущем состоянии. */
    *next_state_out = current_state;

    const menu_screen_t* screen = find_screen(current_state);

    if (screen == NULL || screen->on_button == NULL) {
        return false;
    }

    return screen->on_button(button, next_state_out);
}


void menu_goto(purrgo_state_t* next, purrgo_state_t state)
{
    *next = state;
    purrgo_menu_controller_on_enter(state);
}
