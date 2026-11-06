#include "menu_internal.h"
#include "purrgo/config.h"
#include "purrgo/map_controller.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/ui/ui_dirty.h"
#include "purrgo/waypoint.h"
#include <string.h>

/*
 * Экран выбора цели навигации (WAYPOINT). Строки списка:
 *   0                 (none)       - снять цель
 *   1                 HERE         - текущая позиция GNSS (нужен фикс)
 *   2                 MAP CENTER   - центр карты
 *   3 и далее         точки <wpt> из GPX-файла, выбранного как ROUTE
 * Цель, совпадающая со строкой, помечена "*". Выбор сохраняет PURRGO.CFG и
 * возвращает в главное меню; карта при выходе перерисовывается целиком (символ цели).
 */

#define WPT_FIXED_ROWS 3
#define WPT_ROW_NONE   0
#define WPT_ROW_HERE   1
#define WPT_ROW_MAP    2

/* Строк списка на экране одновременно (не больше UI_ROWS_MAX в ui_rows.h). */
#define WPT_VISIBLE_ROWS 14

static int cursor;
static int top;     /* индекс первой видимой строки */

static int row_count(void)
{
    return WPT_FIXED_ROWS + (int)purrgo_wpt_list_count();
}

static void keep_cursor_visible(void)
{
    if (cursor < top) {
        top = cursor;
    }
    else if (cursor >= top + WPT_VISIBLE_ROWS) {
        top = cursor - WPT_VISIBLE_ROWS + 1;
    }
}

static void on_enter(void)
{
    /* Точки читаются из файла, выбранного как ROUTE (пустое имя очищает список). */
    (void)purrgo_wpt_list_load(app_config.route_file);
    cursor = 0;
    top = 0;
}

/* Цель совпадает с точкой (координаты и имя). */
static bool is_target(const purrgo_waypoint_t* w)
{
    return app_config.wpt_valid &&
           w->lat_1e7 == app_config.wpt_lat_1e7 &&
           w->lon_1e7 == app_config.wpt_lon_1e7 &&
           strcmp(w->name, app_config.wpt_name) == 0;
}

/* Сохранить выбор, обновить карту и вернуться в главное меню. */
static void commit(purrgo_state_t* next)
{
    (void)purrgo_config_save();
    purrgo_ui_dirty_mark(UI_DIRTY_MAP);
    *next = APP_STATE_MENU_MAIN;
}

static bool on_button(purrgo_action_t button, purrgo_state_t* next)
{
    switch (button) {
        case PURRGO_ACTION_UP:
            cursor = menu_cursor_move(cursor, -1, row_count(), false);
            keep_cursor_visible();
            return true;

        case PURRGO_ACTION_DOWN:
            cursor = menu_cursor_move(cursor, +1, row_count(), false);
            keep_cursor_visible();
            return true;

        case PURRGO_ACTION_OK:
            if (cursor == WPT_ROW_NONE) {
                purrgo_wpt_clear();
                commit(next);
            }
            else if (cursor == WPT_ROW_HERE) {
                const purrgo_gnss_solution_t* fix = purrgo_app_get_gnss_solution();

                /* Без фикса позиции нет: остаёмся в списке. */
                if (fix != NULL && fix->valid &&
                    purrgo_wpt_set(fix->lat_1e7, fix->lon_1e7, "HERE")) {
                    commit(next);
                }
            }
            else if (cursor == WPT_ROW_MAP) {
                if (purrgo_wpt_set(map_app_get_map_center_lat(),
                                   map_app_get_map_center_lon(), "MAP")) {
                    commit(next);
                }
            }
            else {
                const purrgo_waypoint_t* w =
                    purrgo_wpt_list_get((size_t)(cursor - WPT_FIXED_ROWS));

                if (w != NULL && purrgo_wpt_set(w->lat_1e7, w->lon_1e7, w->name)) {
                    commit(next);
                }
            }
            return true;

        case PURRGO_ACTION_MENU:
            *next = APP_STATE_MENU_MAIN;
            return true;

        default:
            return false;
    }
}

const menu_screen_t menu_screen_wpt_select = {
    .state     = APP_STATE_MENU_WPT_SELECT,
    .on_enter  = on_enter,
    .on_button = on_button,
};


/* ---------------------------------------------------------------------------
 * Данные для отрисовки и тестов
 * ------------------------------------------------------------------------- */

int menu_app_get_wpt_count(void)  { return row_count(); }
int menu_app_get_wpt_cursor(void) { return cursor; }
int menu_app_get_wpt_top(void)    { return top; }
int menu_app_get_wpt_visible(void) { return WPT_VISIBLE_ROWS; }

void menu_app_get_wpt_item_text(int index, char* buf, size_t size)
{
    if (size == 0) {
        return;
    }
    buf[0] = '\0';

    if (index == WPT_ROW_NONE) {
        purrgo_snprintf(buf, size, "(none)%s", app_config.wpt_valid ? "" : " *");
    }
    else if (index == WPT_ROW_HERE) {
        const purrgo_gnss_solution_t* fix = purrgo_app_get_gnss_solution();

        purrgo_snprintf(buf, size, "HERE%s",
                        (fix != NULL && fix->valid) ? "" : " (no fix)");
    }
    else if (index == WPT_ROW_MAP) {
        purrgo_snprintf(buf, size, "MAP CENTER");
    }
    else {
        const purrgo_waypoint_t* w = purrgo_wpt_list_get((size_t)(index - WPT_FIXED_ROWS));

        if (w != NULL) {
            purrgo_snprintf(buf, size, "%s%s", w->name, is_target(w) ? " *" : "");
        }
    }
}
