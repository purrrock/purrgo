#include "menu_internal.h"
#include "purrgo/config.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/power.h"
#include "purrgo/track_logger.h"
#include "purrgo/logger.h"
#include "purrgo/sound.h"

/*
 * Подменю CONFIG: настройки устройства, логов и карты. Действия (книги, трекер,
 * выбор маршрута и цели, выключение) остаются в главном меню (menu_main.c).
 *
 * Пункты описываются таблицей config_items[] ниже. Порядок в таблице - порядок
 * на экране и индекс курсора; число пунктов считается само.
 *
 * Как добавить настройку:
 *   1. Завести черновик (static переменная), скопировать его из app_config в
 *      reset_drafts() и обратно в apply_drafts().
 *   2. Написать функции format (текст строки) и on_select (реакция на SEL).
 *   3. Добавить строку в config_items[] на нужное место.
 *
 * Черновики применяются и сохраняются по BACK (возврат в главное меню). Подэкраны
 * (MAP DIR, MAP LAYERS) сохраняют своё значение сами и возвращаются сюда, не
 * сбрасывая черновики.
 */


/* ---------------------------------------------------------------------------
 * Черновики настроек
 * ------------------------------------------------------------------------- */

static int16_t               draft_tz_offset_minutes;
static purrgo_poi_mode_t     draft_poi_mode;
static track_logger_mode_t   draft_log_mode;
static purrgo_map_details_t  draft_map_details;
static purrgo_power_source_t draft_power_source;
static bool                  draft_sd_log;
static purrgo_sound_level_t  draft_sound_level;

static void reset_drafts(void)
{
    draft_tz_offset_minutes = app_config.tz_offset_minutes;
    draft_poi_mode          = app_config.poi_mode;
    draft_log_mode          = app_config.log_mode;
    draft_map_details       = app_config.map_details;
    draft_power_source      = app_config.power_source;
    draft_sd_log            = app_config.sd_log;
    draft_sound_level       = app_config.sound_level;
}

static void apply_drafts(void)
{
    app_config.tz_offset_minutes = draft_tz_offset_minutes;
    app_config.poi_mode          = draft_poi_mode;
    app_config.log_mode          = draft_log_mode;
    app_config.map_details       = draft_map_details;
    app_config.power_source      = draft_power_source;
    app_config.sd_log            = draft_sd_log;
    app_config.sound_level       = draft_sound_level;

    purrgo_logger_set_mode(app_config.log_mode);
    purrgo_logger_sd_set_enabled(app_config.sd_log);
    purrgo_config_save();
}


/* ---------------------------------------------------------------------------
 * Пункты меню
 * ------------------------------------------------------------------------- */

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


/*
 * SD LOG: дублировать диагностический лог (UART) в файл LOG.TXT на SD (для разбора без подключённого
 * терминала). Включение применяется по BACK, как остальные настройки.
 */
static void sd_log_format(char* buf, size_t size)
{
    purrgo_snprintf(buf, size, "SD LOG: %s", draft_sd_log ? "ON" : "OFF");
}

static void sd_log_select(purrgo_state_t* next)
{
    (void)next;
    draft_sd_log = !draft_sd_log;
}


/*
 * SOUND: громкость звуковых сигналов OFF -> LOW -> HIGH (пьезоизлучатель, purrgo/sound.h).
 * В файл PURRGO.CFG сохраняется по BACK, как остальные настройки, но действует сразу, в
 * отличие от них: иначе после выбора OFF кнопки продолжали бы щёлкать до выхода из меню.
 * При выборе LOW/HIGH звучит короткий проверочный сигнал этой громкости, при выборе OFF
 * звук обрывается.
 */
static void sound_format(char* buf, size_t size)
{
    purrgo_snprintf(buf, size, "SOUND: %s", purrgo_sound_level_name(draft_sound_level));
}

static void sound_select(purrgo_state_t* next)
{
    (void)next;
    draft_sound_level = (purrgo_sound_level_t)(((unsigned)draft_sound_level + 1u) %
                                               (unsigned)PURRGO_SOUND_LEVEL_COUNT);
    app_config.sound_level = draft_sound_level;
    if (draft_sound_level == PURRGO_SOUND_LEVEL_OFF) {
        purrgo_sound_stop();   /* щелчок этого же нажатия, если он ещё звучит */
    } else {
        purrgo_sound_play(PURRGO_SOUND_INFO);
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

static const menu_item_t config_items[] = {
    { .format = power_src_format, .on_select = power_src_select },
    { .format = tz_format, .on_select = tz_select },
    { .format = sound_format, .on_select = sound_select },
    { .format = log_format, .on_select = log_select },
    { .format = sd_log_format, .on_select = sd_log_select },
    { .format = map_dir_format, .on_select = map_dir_select },
    { .format = map_details_format, .on_select = map_details_select },
    { .format = poi_format, .on_select = poi_select },
    { .title = "MAP LAYERS", .on_select = map_layers_select },
};

#define CONFIG_ITEM_COUNT ((int)(sizeof(config_items) / sizeof(config_items[0])))

static int cursor;


/* ---------------------------------------------------------------------------
 * Экран
 * ------------------------------------------------------------------------- */

static void on_enter(void)
{
    /* При каждом входе в CONFIG из главного меню создаётся новый черновик. */
    reset_drafts();
    cursor = 0;
}


static bool on_button(purrgo_action_t button, purrgo_state_t* next)
{
    switch (button) {
        case PURRGO_ACTION_UP:
            cursor = menu_cursor_move(cursor, -1, CONFIG_ITEM_COUNT, false);
            return true;

        case PURRGO_ACTION_DOWN:
            cursor = menu_cursor_move(cursor, +1, CONFIG_ITEM_COUNT, false);
            return true;

        case PURRGO_ACTION_OK:
            if (config_items[cursor].on_select != NULL) {
                config_items[cursor].on_select(next);
            }
            return true;

        case PURRGO_ACTION_MENU:
            /* BACK: применить все изменения, сохранить и вернуться в главное меню. */
            apply_drafts();
            *next = APP_STATE_MENU_MAIN;
            return true;

        default:
            return false;
    }
}


const menu_screen_t menu_screen_config = {
    .state     = APP_STATE_MENU_CONFIG,
    .on_enter  = on_enter,
    .on_button = on_button,
};


/* ---------------------------------------------------------------------------
 * Данные для отрисовки и тестов
 * ------------------------------------------------------------------------- */

int menu_app_get_config_count(void)  { return CONFIG_ITEM_COUNT; }
int menu_app_get_config_cursor(void) { return cursor; }

void menu_app_get_config_item_text(int index, char* buf, size_t size)
{
    if (size == 0) {
        return;
    }
    buf[0] = '\0';

    if (index < 0 || index >= CONFIG_ITEM_COUNT) {
        return;
    }

    const menu_item_t* item = &config_items[index];

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

bool menu_app_get_draft_sd_log(void)
{
    return draft_sd_log;
}

purrgo_sound_level_t menu_app_get_draft_sound_level(void)
{
    return draft_sound_level;
}
