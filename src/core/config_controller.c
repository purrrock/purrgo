#include "purrgo/ui/ui_dirty.h"
#include "purrgo/config_controller.h"
#include "purrgo/map_controller.h"
#include "purrgo/config.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/power.h"
#include <string.h>


/*
 * Количество элементов каталога, которые одновременно находятся в RAM.
 */
#define DIR_PAGE_SIZE 10


/*
 * Текущая страница каталога.
 */
static purrgo_fs_dirent_t dir_page[DIR_PAGE_SIZE];


/*
 * Количество элементов в текущей странице.
 */
static int dir_page_count = 0;


/*
 * Курсор внутри текущей страницы.
 */
static int dir_page_cursor = 0;


/*
 * Глобальный индекс первого элемента текущей страницы.
 */
static int dir_page_start = 0;


/*
 * Открытый каталог.
 */
static purrgo_dir_t* dir_handle = NULL;


/*
 * Что показывает браузер: каталоги карт (DIR) или GPX-файлы маршрутов (ROUTE).
 */
typedef enum {
    BROWSE_MAP_DIRS,
    BROWSE_ROUTE_FILES
} browse_kind_t;

static browse_kind_t browse_kind = BROWSE_MAP_DIRS;


/*
 * true - первая запись текущей страницы синтетическая "(none)" (снять маршрут).
 * Бывает только на первой странице списка маршрутов; в счёт dir_page_start
 * не входит (он считает только реальные записи каталога).
 */
static bool dir_page_has_none = false;


/*
 * Черновик часового пояса.
 */
static int16_t draft_tz_offset_minutes;


/*
 * Черновик настройки отображения POI.
 */
static purrgo_poi_mode_t draft_poi_mode;


/*
 * Черновик режима подписей POI.
 */
/*
 * Черновик режима записи трека.
 */
static track_logger_mode_t draft_log_mode;


/*
 * Черновик отображения трека.
 */
static purrgo_map_details_t draft_map_details;


/*
 * Черновик источника питания.
 */
static purrgo_power_source_t draft_power_source;


/*
 * Индекс курсора в меню настроек (порядок совпадает с порядком пунктов на
 * экране, см. ui_config.c).
 */
#define CONFIG_CURSOR_POWER_SRC     0
#define CONFIG_CURSOR_POWER_OFF     1
#define CONFIG_CURSOR_TZ            2
#define CONFIG_CURSOR_DIR           3
#define CONFIG_CURSOR_ROUTE         4
#define CONFIG_CURSOR_POI           5
#define CONFIG_CURSOR_LOG_MODE      6
#define CONFIG_CURSOR_MAP_DETAILS   7
#define CONFIG_CURSOR_MAP_LAYERS    8

_Static_assert(CONFIG_CURSOR_MAP_LAYERS + 1 == PURRGO_CONFIG_MENU_ITEMS,
               "PURRGO_CONFIG_MENU_ITEMS: число пунктов меню CONFIG");

#define MAP_LAYERS_COUNT PURRGO_LAYER_COUNT

/* Timezone Configuration Boundaries */
#define CONFIG_TZ_STEP_MINS         15
#define CONFIG_TZ_MIN_MINS          -720
#define CONFIG_TZ_MAX_MINS          840

static int config_cursor_idx = CONFIG_CURSOR_POWER_SRC;

/*
 * Состояние редактирования слоев карты.
 */
static int map_layers_cursor_idx = 0;
static uint16_t draft_layers;

/*
 * Возвращает максимальный индекс курсора меню.
 */
static int get_config_last_cursor(void)
{
    return PURRGO_CONFIG_MENU_ITEMS - 1;
}


/*
 * Возвращает базовый каталог карт.
 */
static const char* get_maps_base_path(void)
{
    return (browse_kind == BROWSE_ROUTE_FILES)
        ? purrgo_fs_get_routes_path()
        : purrgo_fs_get_maps_path();
}


/*
 * Проверяет, является ли запись "." или "..".
 */
static bool is_dot_directory(
    const purrgo_fs_dirent_t* entry
)
{
    if (entry->name[0] != '.') {
        return false;
    }

    if (entry->name[1] == '\0') {
        return true;
    }

    if (
        entry->name[1] == '.' &&
        entry->name[2] == '\0'
    ) {
        return true;
    }

    return false;
}


/*
 * Файл маршрута: обычный файл *.gpx (регистр расширения любой), имя которого
 * помещается в app_config.route_file; скрытые файлы (".", "._") пропускаются.
 */
static bool is_route_file(
    const purrgo_fs_dirent_t* entry
)
{
    if (entry->is_directory || entry->name[0] == '.') {
        return false;
    }

    size_t len = strlen(entry->name);

    if (len < 5 || len >= PURRGO_ROUTE_NAME_MAX) {
        return false;
    }

    const char* ext = entry->name + len - 4;

    return
        ext[0] == '.' &&
        (ext[1] == 'g' || ext[1] == 'G') &&
        (ext[2] == 'p' || ext[2] == 'P') &&
        (ext[3] == 'x' || ext[3] == 'X');
}


/*
 * Возвращает true только для записей,
 * которые должны быть видимы пользователю в текущем браузере.
 */
static bool is_visible_directory(
    const purrgo_fs_dirent_t* entry
)
{
    if (browse_kind == BROWSE_ROUTE_FILES) {
        return is_route_file(entry);
    }

    return
        entry->is_directory &&
        !is_dot_directory(entry);
}


/*
 * Первой страницей списка маршрутов начинается синтетическая запись "(none)".
 */
static void add_none_entry(void)
{
    dir_page[0].name[0] = '\0';
    dir_page[0].is_directory = false;
    dir_page_count = 1;
    dir_page_has_none = true;
}


/*
 * Закрывает открытый каталог.
 */
static void close_directory_browser(void)
{
    if (dir_handle != NULL) {
        purrgo_fs_closedir(dir_handle);
        dir_handle = NULL;
    }
}


/*
 * Загружает первую страницу каталога.
 */
static void load_first_directory_page(void)
{
    purrgo_fs_dirent_t entry;

    close_directory_browser();

    dir_page_count = 0;
    dir_page_cursor = 0;
    dir_page_start = 0;
    dir_page_has_none = false;

    if (browse_kind == BROWSE_ROUTE_FILES) {
        add_none_entry();
    }

    dir_handle = purrgo_fs_opendir(
        get_maps_base_path()
    );

    if (dir_handle == NULL) {
        return;
    }

    while (
        dir_page_count < DIR_PAGE_SIZE &&
        purrgo_fs_readdir(dir_handle, &entry)
    ) {
        if (!is_visible_directory(&entry)) {
            continue;
        }

        dir_page[dir_page_count] = entry;
        dir_page_count++;
    }
}


/*
 * Загружает следующую страницу каталога.
 */
static bool load_next_directory_page(void)
{
    purrgo_fs_dirent_t entry;
    purrgo_fs_dirent_t first_entry;
    bool found = false;

    while (purrgo_fs_readdir(dir_handle, &entry)) {
        if (is_visible_directory(&entry)) {
            first_entry = entry;
            found = true;
            break;
        }
    }

    if (!found) {
        return false;
    }

    dir_page_start += dir_page_count - (dir_page_has_none ? 1 : 0);

    dir_page_has_none = false;
    dir_page_count = 0;
    dir_page_cursor = 0;

    dir_page[dir_page_count] = first_entry;
    dir_page_count++;

    while (
        dir_page_count < DIR_PAGE_SIZE &&
        purrgo_fs_readdir(dir_handle, &entry)
    ) {
        if (!is_visible_directory(&entry)) {
            continue;
        }

        dir_page[dir_page_count] = entry;
        dir_page_count++;
    }

    return true;
}


/*
 * Загружает страницу, начинающуюся с глобального индекса page_start.
 */
static bool load_directory_page(int page_start)
{
    purrgo_fs_dirent_t entry;
    int skipped = 0;

    close_directory_browser();

    dir_page_count = 0;
    dir_page_cursor = 0;
    dir_page_start = page_start;
    dir_page_has_none = false;

    if (browse_kind == BROWSE_ROUTE_FILES && page_start == 0) {
        add_none_entry();
    }

    dir_handle = purrgo_fs_opendir(
        get_maps_base_path()
    );

    if (dir_handle == NULL) {
        dir_page_start = 0;
        return false;
    }

    while (
        skipped < page_start &&
        purrgo_fs_readdir(dir_handle, &entry)
    ) {
        if (!is_visible_directory(&entry)) {
            continue;
        }

        skipped++;
    }

    if (skipped < page_start) {
        close_directory_browser();
        dir_page_start = 0;
        return false;
    }

    while (
        dir_page_count < DIR_PAGE_SIZE &&
        purrgo_fs_readdir(dir_handle, &entry)
    ) {
        if (!is_visible_directory(&entry)) {
            continue;
        }

        dir_page[dir_page_count] = entry;
        dir_page_count++;
    }

    if (dir_page_count == 0) {
        close_directory_browser();
        dir_page_start = 0;
        return false;
    }

    return true;
}


/*
 * Полностью завершает работу браузера каталогов.
 */
static void leave_directory_browser(void)
{
    close_directory_browser();

    dir_page_count = 0;
    dir_page_cursor = 0;
    dir_page_start = 0;
    dir_page_has_none = false;
}


void purrgo_config_controller_init(void)
{
    draft_tz_offset_minutes =
        app_config.tz_offset_minutes;

    draft_poi_mode =
        app_config.poi_mode;

    draft_power_source =
        app_config.power_source;

    config_cursor_idx = CONFIG_CURSOR_POWER_SRC;
}


void purrgo_config_controller_on_enter(
    purrgo_state_t state
)
{
    if (state == APP_STATE_MENU_CONFIG) {

        /*
         * При каждом входе в меню создаётся новый черновик.
         */
        draft_tz_offset_minutes =
            app_config.tz_offset_minutes;

        draft_poi_mode =
            app_config.poi_mode;

        draft_log_mode =
            app_config.log_mode;

        draft_map_details =
            app_config.map_details;

        draft_power_source =
            app_config.power_source;

        config_cursor_idx = CONFIG_CURSOR_POWER_SRC;
    }
    else if (state == APP_STATE_MENU_MAP_LAYERS) {
        draft_layers = app_config.layers;
        map_layers_cursor_idx = 0;
    }
    else if (state == APP_STATE_MENU_DIR_SELECT) {

        browse_kind = BROWSE_MAP_DIRS;
        load_first_directory_page();
    }
    else if (state == APP_STATE_MENU_ROUTE_SELECT) {

        browse_kind = BROWSE_ROUTE_FILES;
        load_first_directory_page();
    }
}


void purrgo_config_controller_update(
    const purrgo_gnss_solution_t* current_fix
)
{
    /*
     * Контроллер настроек не требует периодического обновления.
     */
    (void)current_fix;
}


bool purrgo_config_controller_handle_button(
    purrgo_state_t current_state,
    purrgo_action_t button,
    purrgo_state_t* next_state_out
)
{
    /*
     * По умолчанию остаёмся в текущем состоянии.
     */
    *next_state_out = current_state;


    /*
     * ================================================================
     * CONFIGURATION MENU
     * ================================================================
     */
    if (current_state == APP_STATE_MENU_CONFIG) {

        switch (button) {

            /*
             * --------------------------------------------------------
             * UP
             * --------------------------------------------------------
             */
            case PURRGO_ACTION_UP:

                if (config_cursor_idx > 0) {

                    config_cursor_idx--;


                }

                return true;


            /*
             * --------------------------------------------------------
             * DOWN
             * --------------------------------------------------------
             */
            case PURRGO_ACTION_DOWN:

                if (
                    config_cursor_idx <
                    get_config_last_cursor()
                ) {
                    config_cursor_idx++;
                }

                return true;


            /*
             * --------------------------------------------------------
             * OK
             * --------------------------------------------------------
             */
            case PURRGO_ACTION_OK:

                /*
                 * POWER SOURCE.
                 */
                if (config_cursor_idx == CONFIG_CURSOR_POWER_SRC) {
                    if (draft_power_source + 1 >= PURRGO_POWER_SRC_COUNT) {
                        draft_power_source = (purrgo_power_source_t)0;
                    }
                    else {
                        draft_power_source =
                            (purrgo_power_source_t)(draft_power_source + 1);
                    }
                }

                /*
                 * TZ.
                 */
                else if (config_cursor_idx == CONFIG_CURSOR_TZ) {
                    if (
                        draft_tz_offset_minutes + CONFIG_TZ_STEP_MINS <= CONFIG_TZ_MAX_MINS
                    ) {
                        draft_tz_offset_minutes += CONFIG_TZ_STEP_MINS;
                    }
                    else {
                        draft_tz_offset_minutes = CONFIG_TZ_MIN_MINS;
                    }
                }

                /*
                 * DIR.
                 */
                else if (config_cursor_idx == CONFIG_CURSOR_DIR) {

                    *next_state_out =
                        APP_STATE_MENU_DIR_SELECT;

                    purrgo_config_controller_on_enter(
                        APP_STATE_MENU_DIR_SELECT
                    );
                }

                /*
                 * ROUTE.
                 */
                else if (config_cursor_idx == CONFIG_CURSOR_ROUTE) {

                    *next_state_out =
                        APP_STATE_MENU_ROUTE_SELECT;

                    purrgo_config_controller_on_enter(
                        APP_STATE_MENU_ROUTE_SELECT
                    );
                }

                /*
                 * POI
                 */
                else if (
                    config_cursor_idx == CONFIG_CURSOR_POI
                ) {
                    if (draft_poi_mode == PURRGO_POI_MODE_ICONS) {
                        draft_poi_mode = PURRGO_POI_MODE_NO;
                    }
                    else {
                        draft_poi_mode++;
                    }
                }

                /*
                 * LOG_MODE
                 */
                else if (
                    config_cursor_idx == CONFIG_CURSOR_LOG_MODE
                ) {
                    if (draft_log_mode == LOGGER_MODE_OFF) {
                        draft_log_mode = LOGGER_MODE_STANDARD;
                    }
                    else if (draft_log_mode == LOGGER_MODE_STANDARD) {
                        draft_log_mode = LOGGER_MODE_EXPEDITION;
                    }
                    else {
                        draft_log_mode = LOGGER_MODE_OFF;
                    }
                }

                /*
                 * MAP DETAILS
                 */
                else if (
                    config_cursor_idx == CONFIG_CURSOR_MAP_DETAILS
                ) {
                    if (draft_map_details == PURRGO_MAP_DETAILS_HIGH) {
                        draft_map_details = PURRGO_MAP_DETAILS_LOW;
                    }
                    else {
                        draft_map_details = PURRGO_MAP_DETAILS_HIGH;
                    }
                }

                /*
                 * MAP LAYERS
                 */
                else if (
                    config_cursor_idx == CONFIG_CURSOR_MAP_LAYERS
                ) {

                    *next_state_out =
                        APP_STATE_MENU_MAP_LAYERS;

                    purrgo_config_controller_on_enter(
                        APP_STATE_MENU_MAP_LAYERS
                    );
                }

                /*
                 * POWER OFF
                 */
                else if (
                    config_cursor_idx == CONFIG_CURSOR_POWER_OFF
                ) {
                    purrgo_system_power_off();
                }

                return true;


            /*
             * --------------------------------------------------------
             * MENU / BACK
             * --------------------------------------------------------
             */
            case PURRGO_ACTION_MENU:

                /*
                 * Применяем все изменения.
                 */
                app_config.tz_offset_minutes =
                    draft_tz_offset_minutes;

                app_config.poi_mode =
                    draft_poi_mode;

                app_config.log_mode =
                    draft_log_mode;

                purrgo_logger_set_mode(
                    app_config.log_mode
                );

                app_config.map_details =
                    draft_map_details;

                app_config.power_source =
                    draft_power_source;

                purrgo_config_save();

                *next_state_out =
                    APP_STATE_MAP;

                purrgo_ui_dirty_mark(UI_DIRTY_MAP);

                return true;


            default:
                return false;
        }
    }


    /*
     * ================================================================
     * MAP LAYERS
     * ================================================================
     */
    if (current_state == APP_STATE_MENU_MAP_LAYERS) {

        switch (button) {
            case PURRGO_ACTION_UP:
                if (map_layers_cursor_idx > 0) {
                    map_layers_cursor_idx--;
                }
                else {
                    map_layers_cursor_idx = MAP_LAYERS_COUNT - 1;
                }
                return true;

            case PURRGO_ACTION_DOWN:
                if (map_layers_cursor_idx < MAP_LAYERS_COUNT - 1) {
                    map_layers_cursor_idx++;
                }
                else {
                    map_layers_cursor_idx = 0;
                }
                return true;

            case PURRGO_ACTION_OK:
                draft_layers = (uint16_t)(draft_layers ^ (1u << map_layers_cursor_idx));
                return true;

            case PURRGO_ACTION_MENU:
                app_config.layers = draft_layers;

                purrgo_config_save();
                *next_state_out = APP_STATE_MENU_CONFIG;
                return true;

            default:
                return false;
        }
    }


    /*
     * ================================================================
     * DIRECTORY SELECTOR
     * ================================================================
     */
    if (
        current_state == APP_STATE_MENU_DIR_SELECT ||
        current_state == APP_STATE_MENU_ROUTE_SELECT
    ) {

        switch (button) {

            /*
             * --------------------------------------------------------
             * UP
             * --------------------------------------------------------
             */
            case PURRGO_ACTION_UP:

                if (dir_page_cursor > 0) {

                    dir_page_cursor--;

                    return true;
                }

                if (dir_page_start > 0) {

                    int previous_page_start;

                    if (
                        dir_page_start >= DIR_PAGE_SIZE
                    ) {
                        previous_page_start =
                            dir_page_start -
                            DIR_PAGE_SIZE;
                    }
                    else {
                        previous_page_start = 0;
                    }

                    if (
                        load_directory_page(
                            previous_page_start
                        )
                    ) {
                        dir_page_cursor =
                            dir_page_count - 1;
                    }
                }

                return true;


            /*
             * --------------------------------------------------------
             * DOWN
             * --------------------------------------------------------
             */
            case PURRGO_ACTION_DOWN:

                if (
                    dir_page_cursor <
                    dir_page_count - 1
                ) {
                    dir_page_cursor++;

                    return true;
                }

                if (
                    dir_page_count <
                    DIR_PAGE_SIZE
                ) {
                    return true;
                }

                if (load_next_directory_page()) {
                    dir_page_cursor = 0;
                }

                return true;


            /*
             * --------------------------------------------------------
             * OK
             * --------------------------------------------------------
             */
            case PURRGO_ACTION_OK:

                if (
                    dir_page_count > 0 &&
                    dir_page_cursor >= 0 &&
                    dir_page_cursor < dir_page_count &&
                    browse_kind == BROWSE_ROUTE_FILES
                ) {

                    /*
                     * Пустое имя ("(none)") снимает маршрут. Выбор файла
                     * заодно включает слой Route: иначе линия не появится.
                     */
                    purrgo_snprintf(
                        app_config.route_file,
                        sizeof(app_config.route_file),
                        "%s",
                        dir_page[dir_page_cursor].name
                    );

                    if (app_config.route_file[0] != '\0') {
                        app_config.layers = (uint16_t)(
                            app_config.layers | (1u << PURRGO_LAYER_ROUTE)
                        );
                    }

                    purrgo_config_save();

                    leave_directory_browser();

                    *next_state_out =
                        APP_STATE_MENU_CONFIG;

                    purrgo_ui_dirty_mark(UI_DIRTY_MAP);
                }
                else if (
                    dir_page_count > 0 &&
                    dir_page_cursor >= 0 &&
                    dir_page_cursor < dir_page_count
                ) {

                    purrgo_snprintf(
                        app_config.map_dir,
                        sizeof(app_config.map_dir),
                        "%s/%s",
                        purrgo_fs_get_maps_path(),
                        dir_page[
                            dir_page_cursor
                        ].name
                    );

                    purrgo_config_save();

                    leave_directory_browser();

                    *next_state_out =
                        APP_STATE_MENU_CONFIG;
                }

                return true;


            /*
             * --------------------------------------------------------
             * MENU / BACK
             * --------------------------------------------------------
             */
            case PURRGO_ACTION_MENU:

                leave_directory_browser();

                *next_state_out =
                    APP_STATE_MENU_CONFIG;

                purrgo_ui_dirty_mark(UI_DIRTY_MAP);

                return true;


            default:
                return false;
        }
    }


    return false;
}


int16_t config_app_get_draft_tz_offset(void)
{
    return draft_tz_offset_minutes;
}


int config_app_get_config_cursor(void)
{
    return config_cursor_idx;
}


int config_app_get_dir_list(
    purrgo_fs_dirent_t** list_out
)
{
    *list_out = dir_page;

    return dir_page_count;
}


int config_app_get_dir_cursor(void)
{
    return dir_page_cursor;
}


purrgo_poi_mode_t config_app_get_draft_poi_mode(void)
{
    return draft_poi_mode;
}


track_logger_mode_t
config_app_get_draft_log_mode(void)
{
    return draft_log_mode;
}


purrgo_map_details_t
config_app_get_draft_map_details(void)
{
    return draft_map_details;
}

purrgo_power_source_t
config_app_get_draft_power_source(void)
{
    return draft_power_source;
}

int config_app_get_map_layers_cursor(void)
{
    return map_layers_cursor_idx;
}

uint16_t config_app_get_draft_layers(void) { return draft_layers; }
