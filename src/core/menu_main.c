#include "menu_internal.h"
#include "purrgo/config.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/power.h"
#include "purrgo/book_reader.h"
#include "purrgo/track_logger.h"
#include "purrgo/ui/ui_dirty.h"

/*
 * Главное меню устройства.
 *
 * Пункты описываются таблицей main_items[] ниже. Порядок в таблице - порядок
 * на экране и индекс курсора; число пунктов считается само.
 *
 * Как добавить пункт:
 *   1. Если это настройка, значение которой правится в меню: завести черновик
 *      (static переменная), скопировать его из app_config в reset_drafts() и
 *      обратно в apply_drafts().
 *   2. Написать функцию on_select (реакция на SEL) и, если в строке нужно
 *      показывать значение, функцию format (текст строки).
 *   3. Добавить строку в main_items[] на нужное место.
 */

typedef struct {
    /* Заголовок. Показывается, если format == NULL. */
    const char* title;

    /* Формирует текст строки со значением (UTF-8). Может быть NULL. */
    void (*format)(char* buf, size_t size);

    /*
     * Нажатие SEL. Для перехода на другой экран вызвать menu_goto(next, ...).
     * Может быть NULL (пункт только показывает значение).
     */
    void (*on_select)(purrgo_state_t* next);
} menu_item_t;


/* ---------------------------------------------------------------------------
 * Черновики настроек: меняются в меню, применяются и сохраняются по BACK.
 * ------------------------------------------------------------------------- */

static int16_t               draft_tz_offset_minutes;
static purrgo_poi_mode_t     draft_poi_mode;
static track_logger_mode_t   draft_log_mode;
static purrgo_map_details_t  draft_map_details;
static purrgo_power_source_t draft_power_source;

static void reset_drafts(void)
{
    draft_tz_offset_minutes = app_config.tz_offset_minutes;
    draft_poi_mode          = app_config.poi_mode;
    draft_log_mode          = app_config.log_mode;
    draft_map_details       = app_config.map_details;
    draft_power_source      = app_config.power_source;
}

static void apply_drafts(void)
{
    app_config.tz_offset_minutes = draft_tz_offset_minutes;
    app_config.poi_mode          = draft_poi_mode;
    app_config.log_mode          = draft_log_mode;
    app_config.map_details       = draft_map_details;
    app_config.power_source      = draft_power_source;

    purrgo_logger_set_mode(app_config.log_mode);
    purrgo_config_save();
}


/* ---------------------------------------------------------------------------
 * Пункты меню
 * ------------------------------------------------------------------------- */

/* BOOKS: читалка сама останавливает трек и GNSS и выбирает, куда перейти. */
static void books_select(purrgo_state_t* next)
{
    *next = purrgo_book_reader_enter();
}


/* POWER SOURCE: перебор по кругу. */
static void power_src_format(char* buf, size_t size)
{
    purrgo_snprintf(buf, size, "POWER: %s",
                    purrgo_power_source_title(draft_power_source));
}

static void power_src_select(purrgo_state_t* next)
{
    (void)next;
    draft_power_source = (purrgo_power_source_t)
        ((draft_power_source + 1) % PURRGO_POWER_SRC_COUNT);
}


static void power_off_select(purrgo_state_t* next)
{
    (void)next;
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


/* TZ: шаг 15 минут, после максимума - снова минимум. */
#define TZ_STEP_MINS   15
#define TZ_MIN_MINS    -720
#define TZ_MAX_MINS    840

static void tz_format(char* buf, size_t size)
{
    const int abs_tz = (draft_tz_offset_minutes >= 0)
        ? draft_tz_offset_minutes : -draft_tz_offset_minutes;

    purrgo_snprintf(buf, size, "TZ: UTC%c%02d:%02d",
                    (draft_tz_offset_minutes >= 0) ? '+' : '-',
                    abs_tz / 60, abs_tz % 60);
}

static void tz_select(purrgo_state_t* next)
{
    (void)next;
    if (draft_tz_offset_minutes + TZ_STEP_MINS <= TZ_MAX_MINS) {
        draft_tz_offset_minutes = (int16_t)(draft_tz_offset_minutes + TZ_STEP_MINS);
    }
    else {
        draft_tz_offset_minutes = TZ_MIN_MINS;
    }
}


static void map_dir_format(char* buf, size_t size)
{
    purrgo_snprintf(buf, size, "MAP DIR: %s", app_config.map_dir);
}

static void map_dir_select(purrgo_state_t* next)
{
    menu_goto(next, APP_STATE_MENU_DIR_SELECT);
}


static void route_format(char* buf, size_t size)
{
    purrgo_snprintf(buf, size, "ROUTE: %s",
                    (app_config.route_file[0] != '\0')
                        ? app_config.route_file : "NONE");
}

static void route_select(purrgo_state_t* next)
{
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


static void poi_format(char* buf, size_t size)
{
    const char* name;

    switch (draft_poi_mode) {
        case PURRGO_POI_MODE_CIRCLES: name = "CIRCLES"; break;
        case PURRGO_POI_MODE_ICONS:   name = "ICONS";   break;
        default:                      name = "NO";      break;
    }
    purrgo_snprintf(buf, size, "POI: %s", name);
}

static void poi_select(purrgo_state_t* next)
{
    (void)next;
    draft_poi_mode = (draft_poi_mode == PURRGO_POI_MODE_ICONS)
        ? PURRGO_POI_MODE_NO
        : (purrgo_poi_mode_t)(draft_poi_mode + 1);
}


static void log_format(char* buf, size_t size)
{
    const char* name;

    switch (draft_log_mode) {
        case LOGGER_MODE_EXPEDITION: name = "EXPEDITION"; break;
        case LOGGER_MODE_STANDARD:   name = "STANDARD";   break;
        default:                     name = "OFF";        break;
    }
    purrgo_snprintf(buf, size, "LOG: %s", name);
}

/* OFF -> STANDARD -> EXPEDITION -> OFF */
static void log_select(purrgo_state_t* next)
{
    (void)next;
    switch (draft_log_mode) {
        case LOGGER_MODE_OFF:      draft_log_mode = LOGGER_MODE_STANDARD;   break;
        case LOGGER_MODE_STANDARD: draft_log_mode = LOGGER_MODE_EXPEDITION; break;
        default:                   draft_log_mode = LOGGER_MODE_OFF;        break;
    }
}


static void map_details_format(char* buf, size_t size)
{
    purrgo_snprintf(buf, size, "MAP DETAILS: %s",
                    (draft_map_details == PURRGO_MAP_DETAILS_HIGH) ? "HIGH" : "LOW");
}

static void map_details_select(purrgo_state_t* next)
{
    (void)next;
    draft_map_details = (draft_map_details == PURRGO_MAP_DETAILS_HIGH)
        ? PURRGO_MAP_DETAILS_LOW
        : PURRGO_MAP_DETAILS_HIGH;
}


static void map_layers_select(purrgo_state_t* next)
{
    menu_goto(next, APP_STATE_MENU_MAP_LAYERS);
}


/* ---------------------------------------------------------------------------
 * Таблица пунктов: порядок = порядок на экране.
 * ------------------------------------------------------------------------- */

static const menu_item_t main_items[] = {
    { .title = "BOOKS", .on_select = books_select },
    { .format = power_src_format, .on_select = power_src_select },
    { .title = "POWER OFF", .on_select = power_off_select },
    { .title = "TRACKER (SCREEN OFF)", .on_select = tracker_select },
    { .format = tz_format, .on_select = tz_select },
    { .format = map_dir_format, .on_select = map_dir_select },
    { .format = route_format, .on_select = route_select },
    { .format = wpt_format, .on_select = wpt_select },
    { .format = poi_format, .on_select = poi_select },
    { .format = log_format, .on_select = log_select },
    { .format = map_details_format, .on_select = map_details_select },
    { .title = "MAP LAYERS", .on_select = map_layers_select },
};

#define MAIN_ITEM_COUNT ((int)(sizeof(main_items) / sizeof(main_items[0])))

static int cursor;


/* ---------------------------------------------------------------------------
 * Экран
 * ------------------------------------------------------------------------- */

static void on_enter(void)
{
    /* При каждом входе в меню создаётся новый черновик. */
    reset_drafts();
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
            /* BACK: применить все изменения, сохранить и выйти на карту. */
            apply_drafts();
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

int16_t menu_app_get_draft_tz_offset(void)
{
    return draft_tz_offset_minutes;
}

purrgo_power_source_t menu_app_get_draft_power_source(void)
{
    return draft_power_source;
}
