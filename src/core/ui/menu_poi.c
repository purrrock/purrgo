#include "menu_internal.h"
#include "purrgo/config.h"
#include "purrgo/map_controller.h"
#include "purrgo/pgo_text.h"
#include "purrgo/poi_search.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/ui/ui_dirty.h"
#include "purrgo/waypoint.h"

/*
 * Поиск объектов (пункт SEARCH главного меню), docs/design/poi-search.md:
 *   APP_STATE_MENU_POI_CATEGORY - список категорий (Жильё / Lodging, Еда / Food, ...);
 *   APP_STATE_MENU_POI_RESULTS  - ближайшие объекты выбранной категории по возрастанию расстояния.
 *
 * Выбор категории запускает поиск (poi_search.c), сам он идёт в такте приложения (app_fsm.c):
 * по проходу индекса за такт, пока не наберётся список. На экране результатов:
 *   UP / DOWN  - строка списка;
 *   SEL        - карта центрируется на объекте (автоследование выключено, в центре перекрестие);
 *   долгий SEL - объект становится целью навигации (WAYPOINT), карта центрируется на нём;
 *   BACK       - к списку категорий (поиск отменяется).
 */

/* ---------------------------------------------------------------------------
 * Категории
 * ------------------------------------------------------------------------- */

static int cat_cursor;

static int cat_count(void)
{
    return (int)purrgo_poi_category_count();
}

/*
 * Начало отсчёта: фикс GNSS, иначе последняя известная позиция, иначе центр вида карты.
 */
static purrgo_poi_origin_t pick_origin(int32_t* lat, int32_t* lon)
{
    const purrgo_gnss_solution_t* fix = purrgo_app_get_gnss_solution();

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

static void cat_on_enter(void)
{
    cat_cursor = 0;
}

static bool cat_on_button(purrgo_action_t button, purrgo_state_t* next)
{
    switch (button) {
        case PURRGO_ACTION_UP:
            cat_cursor = menu_cursor_move(cat_cursor, -1, cat_count(), false);
            return true;

        case PURRGO_ACTION_DOWN:
            cat_cursor = menu_cursor_move(cat_cursor, +1, cat_count(), false);
            return true;

        case PURRGO_ACTION_OK: {
            int32_t lat = 0;
            int32_t lon = 0;
            const purrgo_poi_origin_t origin = pick_origin(&lat, &lon);

            /* Ошибка запуска (нет каталога карты) видна на экране результатов. */
            (void)purrgo_poi_search_start(app_config.map_dir, (size_t)cat_cursor, origin, lat, lon);
            menu_goto(next, APP_STATE_MENU_POI_RESULTS);
            return true;
        }

        case PURRGO_ACTION_MENU:
            *next = APP_STATE_MENU_MAIN;
            return true;

        default:
            return false;
    }
}

const menu_screen_t menu_screen_poi_category = {
    .state     = APP_STATE_MENU_POI_CATEGORY,
    .on_enter  = cat_on_enter,
    .on_button = cat_on_button,
};

int menu_app_get_poi_category_count(void)  { return cat_count(); }
int menu_app_get_poi_category_cursor(void) { return cat_cursor; }

void menu_app_get_poi_category_text(int index, char* buf, size_t size)
{
    if (size == 0) {
        return;
    }
    buf[0] = '\0';

    if (index >= 0 && index < cat_count()) {
        purrgo_snprintf(buf, size, "%s", purrgo_poi_category_title((size_t)index));
    }
}

/* ---------------------------------------------------------------------------
 * Результаты
 * ------------------------------------------------------------------------- */

/* Строк списка на экране одновременно (не больше UI_ROWS_MAX в ui_rows.h). */
#define POI_VISIBLE_ROWS 14

static int res_cursor;
static int res_top;

static int res_count(void)
{
    return (int)purrgo_poi_search_count();
}

static void res_keep_cursor_visible(void)
{
    if (res_cursor < res_top) {
        res_top = res_cursor;
    }
    else if (res_cursor >= res_top + POI_VISIBLE_ROWS) {
        res_top = res_cursor - POI_VISIBLE_ROWS + 1;
    }
}

static void res_on_enter(void)
{
    res_cursor = 0;
    res_top = 0;
}

/* Центр карты - в объекте; выход на карту с полной перерисовкой. */
static bool show_on_map(int index, purrgo_state_t* next)
{
    int32_t lat = 0;
    int32_t lon = 0;

    if (!purrgo_poi_search_position((size_t)index, &lat, &lon)) {
        return false;
    }
    purrgo_map_controller_center_on(lat, lon);
    purrgo_poi_search_cancel();
    *next = APP_STATE_MAP;
    purrgo_ui_dirty_mark(UI_DIRTY_MAP);
    return true;
}

static bool res_on_button(purrgo_action_t button, purrgo_state_t* next)
{
    switch (button) {
        case PURRGO_ACTION_UP:
            res_cursor = menu_cursor_move(res_cursor, -1, res_count(), false);
            res_keep_cursor_visible();
            return true;

        case PURRGO_ACTION_DOWN:
            res_cursor = menu_cursor_move(res_cursor, +1, res_count(), false);
            res_keep_cursor_visible();
            return true;

        case PURRGO_ACTION_OK:
            /* Пока идёт поиск или список пуст, SEL ничего не делает. */
            if (res_count() > 0) {
                (void)show_on_map(res_cursor, next);
            }
            return true;

        case PURRGO_ACTION_OK_LONG:
            if (res_count() > 0) {
                int32_t lat = 0;
                int32_t lon = 0;

                if (purrgo_poi_search_position((size_t)res_cursor, &lat, &lon)) {
                    char name[PURRGO_WPT_NAME_MAX];

                    /* Имя на карте - PurrGO-256, а в настройках имя цели - UTF-8. */
                    (void)purrgo_pgo_to_utf8(purrgo_poi_search_name((size_t)res_cursor),
                                             name, sizeof name);
                    if (purrgo_wpt_set(lat, lon, name)) {
                        (void)purrgo_config_save();
                        (void)show_on_map(res_cursor, next);
                    }
                }
            }
            return true;

        case PURRGO_ACTION_MENU:
            purrgo_poi_search_cancel();
            *next = APP_STATE_MENU_POI_CATEGORY;
            return true;

        default:
            return false;
    }
}

const menu_screen_t menu_screen_poi_results = {
    .state     = APP_STATE_MENU_POI_RESULTS,
    .on_enter  = res_on_enter,
    .on_button = res_on_button,
};

int menu_app_get_poi_result_count(void)   { return res_count(); }
int menu_app_get_poi_result_cursor(void)  { return res_cursor; }
int menu_app_get_poi_result_top(void)     { return res_top; }
int menu_app_get_poi_result_visible(void) { return POI_VISIBLE_ROWS; }
