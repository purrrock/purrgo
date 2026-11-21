#include "menu_internal.h"
#include "purrgo/config.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/power.h"
#include "purrgo/book_reader.h"
#include "purrgo/track_name.h"
#include "purrgo/ui/ui_dirty.h"

/*
 * Главное меню устройства: действия. Настройки (TZ, SOUND, POWER, LOG, карта и т. п.)
 * вынесены в подменю CONFIG (menu_config.c).
 *
 * Пункты описываются таблицей main_items[] ниже. Порядок в таблице - порядок
 * на экране и индекс курсора; число пунктов считается само.
 *
 * Как добавить пункт:
 *   1. Написать функцию on_select (реакция на SEL) и, если в строке нужно
 *      показывать значение, функцию format (текст строки).
 *   2. Добавить строку в main_items[] на нужное место.
 * Если пункт - настройка со значением, которое правится на месте, его место в
 * menu_config.c.
 */


/* ---------------------------------------------------------------------------
 * Пункты меню
 * ------------------------------------------------------------------------- */

/*
 * BOOKS: читалка сама останавливает трек и GNSS и выбирает, куда перейти.
 */
static void books_select(purrgo_state_t* next)
{
    *next = purrgo_book_reader_enter();
}


static void power_off_select(purrgo_state_t* next)
{
    (void)next;
    /* purrgo_system_power_off() сохраняет app_config. */
    purrgo_system_power_off();
}


/*
 * TRACKER: режим трекера с отключённым дисплеем. Запись трека как в EXPEDITION,
 * на экране предупреждение, дальше экран не обновляется. Выход - K1+K4 вместе.
 * Побочные эффекты входа (режим логгера) выполняет FSM (app_fsm.c).
 */
static void tracker_select(purrgo_state_t* next)
{
    *next = APP_STATE_TRACKER_SCREEN_OFF;
}


static void route_format(char* buf, size_t size)
{
    if (app_config.route_file[0] == '\0') {
        purrgo_snprintf(buf, size, "ROUTE: NONE");
    }
    else if (app_config.route_from_tracks) {
        /* Открытый для просмотра трек: подпись по времени старта. */
        char label[PURRGO_ROUTE_NAME_MAX];

        purrgo_track_name_label(app_config.route_file, app_config.tz_offset_minutes,
                                label, sizeof label);
        purrgo_snprintf(buf, size, "ROUTE: TRK %s", label);
    }
    else {
        purrgo_snprintf(buf, size, "ROUTE: %s", app_config.route_file);
    }
}

static void route_select(purrgo_state_t* next)
{
    menu_route_select_set_source(PURRGO_ROUTE_SRC_ROUTES);
    menu_goto(next, APP_STATE_MENU_ROUTE_SELECT);
}

static void track_open_select(purrgo_state_t* next)
{
    menu_route_select_set_source(PURRGO_ROUTE_SRC_TRACKS);
    menu_goto(next, APP_STATE_MENU_ROUTE_SELECT);
}


static void wpt_format(char* buf, size_t size)
{
    purrgo_snprintf(buf, size, "WAYPOINT: %s",
                    app_config.wpt_valid ? app_config.wpt_name : "NONE");
}

static void wpt_select(purrgo_state_t* next)
{
    menu_goto(next, APP_STATE_MENU_WPT_SELECT);
}



static void config_select(purrgo_state_t* next)
{
    menu_goto(next, APP_STATE_MENU_CONFIG);
}


/* ---------------------------------------------------------------------------
 * Таблица пунктов: порядок = порядок на экране.
 * ------------------------------------------------------------------------- */

static const menu_item_t main_items[] = {
    { .title = "CONFIG", .on_select = config_select },
    { .title = "POWER OFF", .on_select = power_off_select },
    { .title = "TRACKER (SCREEN OFF)", .on_select = tracker_select },
    { .format = route_format, .on_select = route_select },
    { .title = "OPEN TRACK", .on_select = track_open_select },
    { .format = wpt_format, .on_select = wpt_select },
    { .title = "BOOKS", .on_select = books_select },
};

#define MAIN_ITEM_COUNT ((int)(sizeof(main_items) / sizeof(main_items[0])))

static int cursor;


/* ---------------------------------------------------------------------------
 * Экран
 * ------------------------------------------------------------------------- */

static void on_enter(void)
{
    cursor = 0;
}


static bool on_button(purrgo_action_t button, purrgo_state_t* next)
{
    switch (button) {
        case PURRGO_ACTION_UP:
            cursor = menu_cursor_move(cursor, -1, MAIN_ITEM_COUNT, false);
            return true;

        case PURRGO_ACTION_DOWN:
            cursor = menu_cursor_move(cursor, +1, MAIN_ITEM_COUNT, false);
            return true;

        case PURRGO_ACTION_OK:
            if (main_items[cursor].on_select != NULL) {
                main_items[cursor].on_select(next);
            }
            return true;

        case PURRGO_ACTION_MENU:
            /* BACK: выйти на карту. */
            *next = APP_STATE_MAP;
            purrgo_ui_dirty_mark(UI_DIRTY_MAP);
            return true;

        default:
            return false;
    }
}


const menu_screen_t menu_screen_main = {
    .state     = APP_STATE_MENU_MAIN,
    .on_enter  = on_enter,
    .on_button = on_button,
};


/* ---------------------------------------------------------------------------
 * Данные для отрисовки и тестов
 * ------------------------------------------------------------------------- */

int menu_app_get_main_count(void)  { return MAIN_ITEM_COUNT; }
int menu_app_get_main_cursor(void) { return cursor; }

void menu_app_get_main_item_text(int index, char* buf, size_t size)
{
    if (size == 0) {
        return;
    }
    buf[0] = '\0';

    if (index < 0 || index >= MAIN_ITEM_COUNT) {
        return;
    }

    const menu_item_t* item = &main_items[index];

    if (item->format != NULL) {
        item->format(buf, size);
    }
    else {
        purrgo_snprintf(buf, size, "%s", item->title);
    }
}
