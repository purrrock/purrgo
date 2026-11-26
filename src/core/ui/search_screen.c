#include "purrgo/search_screen.h"

#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include "purrgo/fs_hal.h"
#include "purrgo/map.h"
#include "purrgo/map_controller.h"
#include "purrgo/map_search.h"
#include "purrgo/navigation.h"
#include "purrgo/pgo_text.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/system_time.h"
#include "purrgo/ui/ui_dirty.h"
#include "purrgo/waypoint.h"

#include <string.h>

/* Экран умного поиска: логика кнопок и запуск расчёта (отрисовка - ui_search.c). */

static purrgo_keyboard_t s_kb;
static bool     s_list_focus;
static int      s_cursor;
static int      s_top;
static bool     s_pending;          /* текст изменился, запрос ещё не отправлен в поиск */
static uint32_t s_deadline_ms;
static bool     s_last_running;

/*
 * Начало отсчёта: фикс GNSS, иначе последняя известная позиция, иначе центр вида карты
 * (так же, как у простого поиска по категориям).
 */
static purrgo_poi_origin_t pick_origin(int32_t *lat, int32_t *lon)
{
    const purrgo_gnss_solution_t *fix = purrgo_app_get_gnss_solution();

    if (fix != NULL && fix->valid) {
        *lat = fix->lat_1e7;
        *lon = fix->lon_1e7;
        return PURRGO_POI_ORIGIN_GNSS;
    }
    if (purrgo_app_get_last_position(lat, lon)) {
        return PURRGO_POI_ORIGIN_LAST;
    }
    *lat = map_app_get_map_center_lat();
    *lon = map_app_get_map_center_lon();
    return PURRGO_POI_ORIGIN_MAP;
}

bool purrgo_search_screen_enter(void)
{
    int32_t lat = 0;
    int32_t lon = 0;
    char path[PURRGO_FS_MAX_PATH + 48];

    /* Нет search.idx - ничего не трогаем (карта остаётся открытой). */
    if (purrgo_snprintf(path, sizeof path, "%s/search.idx", app_config.map_dir) >= (int)sizeof path ||
        !purrgo_fs_exists(path)) {
        return false;
    }

    /* Слоты файловой системы заняты слоями карты; поиску нужны свои. Карта откроется заново сама. */
    purrgo_map_shutdown();
    if (!purrgo_map_search_open(app_config.map_dir)) {
        return false;
    }
    purrgo_map_search_set_origin(pick_origin(&lat, &lon), lat, lon);

    purrgo_kbd_open(&s_kb, "SEARCH", "", PURRGO_KBD_LAYOUT_RU);
    s_list_focus = false;
    s_cursor = 0;
    s_top = 0;
    s_pending = false;
    s_last_running = false;
    return true;
}

static void leave(void)
{
    purrgo_map_search_close();
    s_pending = false;
}

static void keep_cursor_visible(void)
{
    if (s_cursor < s_top) {
        s_top = s_cursor;
    } else if (s_cursor >= s_top + PURRGO_SEARCH_VISIBLE_ROWS) {
        s_top = s_cursor - PURRGO_SEARCH_VISIBLE_ROWS + 1;
    }
}

static void move_cursor(int delta)
{
    const int count = (int)purrgo_map_search_count();

    if (count == 0) {
        return;
    }
    s_cursor += delta;
    if (s_cursor < 0) {
        s_cursor = count - 1;
    } else if (s_cursor >= count) {
        s_cursor = 0;
    }
    keep_cursor_visible();
}

/* Центр карты - в объекте; выход на карту с полной перерисовкой. */
static bool show_on_map(int index, purrgo_state_t *next)
{
    int32_t lat = 0;
    int32_t lon = 0;

    if (!purrgo_map_search_position((size_t)index, &lat, &lon)) {
        return false;
    }
    leave();
    purrgo_map_controller_center_on(lat, lon);
    *next = APP_STATE_MAP;
    purrgo_ui_dirty_mark(UI_DIRTY_MAP);
    return true;
}

static void set_target_and_show(int index, purrgo_state_t *next)
{
    int32_t lat = 0;
    int32_t lon = 0;
    char name[PURRGO_WPT_NAME_MAX];

    if (!purrgo_map_search_position((size_t)index, &lat, &lon)) {
        return;
    }
    /* Название на карте - PurrGO-256, а в настройках имя цели - UTF-8. */
    (void)purrgo_pgo_to_utf8(purrgo_map_search_name((size_t)index), name, sizeof name);
    if (name[0] == '\0') {
        (void)purrgo_pgo_to_utf8("SEARCH", name, sizeof name);
    }
    if (purrgo_wpt_set(lat, lon, name)) {
        (void)purrgo_config_save();
        (void)show_on_map(index, next);
    }
}

static bool list_button(purrgo_btn_t button, purrgo_state_t *next)
{
    switch (button) {
        case PURRGO_BTN_KEY1_SHORT:
        case PURRGO_BTN_KEY1_LONG:
            move_cursor(-1);
            return true;

        case PURRGO_BTN_KEY2_SHORT:
        case PURRGO_BTN_KEY2_LONG:
            move_cursor(+1);
            return true;

        case PURRGO_BTN_KEY3_SHORT:
            if (purrgo_map_search_count() > 0u) {
                (void)show_on_map(s_cursor, next);
            }
            return true;

        case PURRGO_BTN_KEY3_LONG:
            if (purrgo_map_search_count() > 0u) {
                set_target_and_show(s_cursor, next);
            }
            return true;

        case PURRGO_BTN_KEY4_SHORT:
            s_list_focus = false;
            return true;

        case PURRGO_BTN_KEY4_LONG:
            leave();
            *next = APP_STATE_MENU_MAIN;
            return true;

        default:
            return false;
    }
}

static bool keyboard_button(purrgo_btn_t button, purrgo_state_t *next)
{
    char before[PURRGO_KBD_TEXT_MAX + 1];

    if (button == PURRGO_BTN_KEY3_LONG) {
        /* Долгое K3 клавиатуры (принять ввод) здесь - переход в список. */
        if (purrgo_map_search_count() > 0u) {
            s_list_focus = true;
            keep_cursor_visible();
        }
        return true;
    }
    if (button == PURRGO_BTN_KEY4_LONG) {
        leave();
        *next = APP_STATE_MENU_MAIN;
        return true;
    }

    memcpy(before, s_kb.text, sizeof before);
    const bool changed = purrgo_kbd_handle_button(&s_kb, button);

    s_kb.result = PURRGO_KBD_RESULT_EDITING;   /* ввод не "принимается": поиск идёт по мере набора */
    if (strcmp(before, s_kb.text) != 0) {
        s_pending = true;
        s_deadline_ms = purrgo_system_time_ms() + PURRGO_SEARCH_DEBOUNCE_MS;
    }
    return changed;
}

bool purrgo_search_screen_handle_button(purrgo_btn_t button, purrgo_state_t *next)
{
    if (s_list_focus) {
        return list_button(button, next);
    }
    return keyboard_button(button, next);
}

bool purrgo_search_screen_tick(uint32_t now_ms)
{
    bool redraw = false;

    if (!purrgo_map_search_is_open()) {
        return false;
    }

    if (s_pending && (int32_t)(now_ms - s_deadline_ms) >= 0) {
        s_pending = false;
        purrgo_map_search_set_query(s_kb.text);
        redraw = true;   /* строка состояния: "Searching..." или результат сразу */
    }

    if (purrgo_map_search_running()) {
        (void)purrgo_map_search_step();
    }

    const bool running = purrgo_map_search_running();

    if (s_last_running && !running) {
        /* Расчёт закончен: список заменён целиком. */
        s_cursor = 0;
        s_top = 0;
        if (purrgo_map_search_count() == 0u) {
            s_list_focus = false;
        }
        redraw = true;
    }
    s_last_running = running;
    return redraw;
}

bool purrgo_search_screen_busy(void)
{
    return purrgo_map_search_is_open() && (s_pending || purrgo_map_search_running());
}

const purrgo_keyboard_t *purrgo_search_screen_keyboard(void)
{
    return &s_kb;
}

bool purrgo_search_screen_list_focus(void)
{
    return s_list_focus;
}

int purrgo_search_screen_cursor(void)
{
    return s_cursor;
}

int purrgo_search_screen_top(void)
{
    return s_top;
}
