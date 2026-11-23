#include "ui/app_keymap.h"

#include <stddef.h>

typedef struct {
    purrgo_btn_t button;
    purrgo_action_t action;
} keymap_entry_t;

/*
 * Кнопки без записи в таблице экрана означают PURRGO_ACTION_NONE. Явные записи, а
 * не плотный массив: нулевое значение purrgo_action_t - UP, а не NONE.
 */

/* Карта: K1..K4 - панорамирование, длинные - масштаб, OK и меню. */
static const keymap_entry_t k_map_keys[] = {
    { PURRGO_BTN_KEY1_SHORT, PURRGO_ACTION_LEFT  },
    { PURRGO_BTN_KEY2_SHORT, PURRGO_ACTION_RIGHT },
    { PURRGO_BTN_KEY3_SHORT, PURRGO_ACTION_UP    },
    { PURRGO_BTN_KEY4_SHORT, PURRGO_ACTION_DOWN  },
    { PURRGO_BTN_KEY1_LONG,  PURRGO_ACTION_MINUS },
    { PURRGO_BTN_KEY2_LONG,  PURRGO_ACTION_PLUS  },
    { PURRGO_BTN_KEY3_LONG,  PURRGO_ACTION_OK    },
    { PURRGO_BTN_KEY4_LONG,  PURRGO_ACTION_MENU  },
};

/* Путевой компьютер: только K4 (короткое и длинное) - следующая страница. */
static const keymap_entry_t k_trip_keys[] = {
    { PURRGO_BTN_KEY4_SHORT, PURRGO_ACTION_MENU },
    { PURRGO_BTN_KEY4_LONG,  PURRGO_ACTION_MENU },
};

/* Меню: короткое и длинное нажатие равнозначны. K1 - вверх, K2 - вниз, K3 - выбор, K4 - назад/дальше. */
static const keymap_entry_t k_menu_keys[] = {
    { PURRGO_BTN_KEY1_SHORT, PURRGO_ACTION_UP   },
    { PURRGO_BTN_KEY1_LONG,  PURRGO_ACTION_UP   },
    { PURRGO_BTN_KEY2_SHORT, PURRGO_ACTION_DOWN },
    { PURRGO_BTN_KEY2_LONG,  PURRGO_ACTION_DOWN },
    { PURRGO_BTN_KEY3_SHORT, PURRGO_ACTION_OK   },
    { PURRGO_BTN_KEY3_LONG,  PURRGO_ACTION_OK   },
    { PURRGO_BTN_KEY4_SHORT, PURRGO_ACTION_MENU },
    { PURRGO_BTN_KEY4_LONG,  PURRGO_ACTION_MENU },
};

/* Меню, где долгий выбор отличается от короткого: короткое K3 - OK, долгое - OK_LONG. */
static const keymap_entry_t k_menu_ok_long_keys[] = {
    { PURRGO_BTN_KEY1_SHORT, PURRGO_ACTION_UP      },
    { PURRGO_BTN_KEY1_LONG,  PURRGO_ACTION_UP      },
    { PURRGO_BTN_KEY2_SHORT, PURRGO_ACTION_DOWN    },
    { PURRGO_BTN_KEY2_LONG,  PURRGO_ACTION_DOWN    },
    { PURRGO_BTN_KEY3_SHORT, PURRGO_ACTION_OK      },
    { PURRGO_BTN_KEY3_LONG,  PURRGO_ACTION_OK_LONG },
    { PURRGO_BTN_KEY4_SHORT, PURRGO_ACTION_MENU    },
    { PURRGO_BTN_KEY4_LONG,  PURRGO_ACTION_MENU    },
};

typedef struct {
    const keymap_entry_t *entries;
    size_t count;
} keymap_t;

#define KEYMAP(arr) { (arr), sizeof(arr) / sizeof((arr)[0]) }

/* Индекс - app_keymap_screen_t. */
static const keymap_t k_keymaps[APP_KEYMAP_SCREEN_COUNT] = {
    [APP_KEYMAP_MAP]  = KEYMAP(k_map_keys),
    [APP_KEYMAP_TRIP] = KEYMAP(k_trip_keys),
    [APP_KEYMAP_MENU] = KEYMAP(k_menu_keys),
    [APP_KEYMAP_MENU_OK_LONG] = KEYMAP(k_menu_ok_long_keys),
};

purrgo_action_t purrgo_app_keymap_action(app_keymap_screen_t screen, purrgo_btn_t button) {
    if ((unsigned)screen >= (unsigned)APP_KEYMAP_SCREEN_COUNT) {
        return PURRGO_ACTION_NONE;
    }

    const keymap_t *map = &k_keymaps[screen];
    for (size_t i = 0; i < map->count; i++) {
        if (map->entries[i].button == button) {
            return map->entries[i].action;
        }
    }
    return PURRGO_ACTION_NONE;
}
