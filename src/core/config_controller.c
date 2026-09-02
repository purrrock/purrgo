#include "purrgo/config_controller.h"
#include "purrgo/config.h"
#include <stdio.h>


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
 * Черновик часового пояса.
 */
static int16_t draft_tz_offset_minutes;


/*
 * Черновик настройки отображения POI.
 */
static bool draft_poi_enabled;


/*
 * Черновик режима подписей POI.
 */
static purrgo_poi_label_mode_t draft_poi_label_mode;


/*
 * Черновик режима записи трека.
 */
static track_logger_mode_t draft_log_mode;


/*
 * Черновик отображения трека.
 */
static bool draft_track_display_enabled;


/*
 * Индекс курсора в меню настроек.
 *
 * Логические позиции:
 *
 *     0 = TZ
 *     1 = DIR
 *     2 = POI
 *     3 = POI labels (только при draft_poi_enabled == true)
 *     4 или 3 = LOG_MODE (зависит от POI labels)
 *     5 или 4 = TRACK_DISPLAY (зависит от POI labels)
 */
static int config_cursor_idx = 0;


/*
 * Возвращает максимальный индекс курсора меню.
 *
 * Если POI выключены, пункт подписей отсутствует.
 */
static int get_config_last_cursor(void)
{
    if (draft_poi_enabled) {
        return 5;
    }

    return 4;
}


/*
 * Возвращает базовый каталог карт.
 */
static const char* get_maps_base_path(void)
{
    return "../../../tests/data/maps";
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
 * Возвращает true только для каталогов,
 * которые должны быть видимы пользователю.
 */
static bool is_visible_directory(
    const purrgo_fs_dirent_t* entry
)
{
    return
        entry->is_directory &&
        !is_dot_directory(entry);
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

    dir_page_start += dir_page_count;

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
}


void purrgo_config_controller_init(void)
{
    draft_tz_offset_minutes =
        app_config.tz_offset_minutes;

    draft_poi_enabled =
        app_config.poi_enabled;

    draft_poi_label_mode =
        app_config.poi_label_mode;

    config_cursor_idx = 0;
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

        draft_poi_enabled =
            app_config.poi_enabled;

        draft_poi_label_mode =
            app_config.poi_label_mode;

        draft_log_mode =
            app_config.log_mode;

        draft_track_display_enabled =
            app_config.track_display_enabled;

        config_cursor_idx = 0;
    }
    else if (state == APP_STATE_MENU_DIR_SELECT) {

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
    purrgo_btn_t button,
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
            case PURRGO_BTN_UP:

                if (config_cursor_idx > 0) {

                    config_cursor_idx--;

                    /*
                     * Если POI выключены, позиция 3 недоступна.
                     *
                     * При изменении POI с включённых на выключенные
                     * курсор может находиться на позиции 3.
                     *
                     * Поэтому дополнительно страхуем состояние.
                     */
                    if (
                        !draft_poi_enabled &&
                        config_cursor_idx > 2
                    ) {
                        config_cursor_idx = 2;
                    }
                }

                return true;


            /*
             * --------------------------------------------------------
             * DOWN
             * --------------------------------------------------------
             */
            case PURRGO_BTN_DOWN:

                if (
                    config_cursor_idx <
                    get_config_last_cursor()
                ) {
                    config_cursor_idx++;
                }

                return true;


            /*
             * --------------------------------------------------------
             * PLUS / RIGHT
             * --------------------------------------------------------
             */
            case PURRGO_BTN_PLUS:
            case PURRGO_BTN_RIGHT:

                /*
                 * Часовой пояс.
                 */
                if (config_cursor_idx == 0) {

                    if (
                        draft_tz_offset_minutes + 15 <= 840
                    ) {
                        draft_tz_offset_minutes += 15;
                    }
                }

                /*
                 * POI: Да / Нет.
                 */
                else if (config_cursor_idx == 2) {

                    draft_poi_enabled =
                        !draft_poi_enabled;

                    /*
                     * Если POI выключены, пункт подписей
                     * становится недоступным.
                     *
                     * Оставляем курсор на самом POI.
                     */
                    if (
                        !draft_poi_enabled &&
                        config_cursor_idx > 2
                    ) {
                        config_cursor_idx = 2;
                    }
                }

                /*
                 * Подписи POI.
                 *
                 * Цикл:
                 *
                 *     Все -> Важные -> Выкл -> Все
                 */
                else if (
                    config_cursor_idx == 3 &&
                    draft_poi_enabled
                ) {
                    if (
                        draft_poi_label_mode ==
                        PURRGO_POI_LABELS_OFF
                    ) {
                        draft_poi_label_mode =
                            PURRGO_POI_LABELS_ALL;
                    }
                    else {
                        draft_poi_label_mode++;
                    }
                }

                /*
                 * Режим записи трека.
                 *
                 * Цикл:
                 *
                 *     Выкл -> Стандарт -> Экспедиция -> Выкл
                 */
                else if (
                    config_cursor_idx == (draft_poi_enabled ? 4 : 3)
                ) {
                    if (
                        draft_log_mode == LOGGER_MODE_EXPEDITION
                    ) {
                        draft_log_mode = LOGGER_MODE_OFF;
                    }
                    else {
                        draft_log_mode++;
                    }
                }

                /*
                 * Отображение трека.
                 */
                else if (
                    config_cursor_idx == (draft_poi_enabled ? 5 : 4)
                ) {
                    draft_track_display_enabled =
                        !draft_track_display_enabled;
                }

                return true;


            /*
             * --------------------------------------------------------
             * MINUS / LEFT
             * --------------------------------------------------------
             */
            case PURRGO_BTN_MINUS:
            case PURRGO_BTN_LEFT:

                /*
                 * Часовой пояс.
                 */
                if (config_cursor_idx == 0) {

                    if (
                        draft_tz_offset_minutes - 15 >= -720
                    ) {
                        draft_tz_offset_minutes -= 15;
                    }
                }

                /*
                 * POI: Да / Нет.
                 */
                else if (config_cursor_idx == 2) {

                    draft_poi_enabled =
                        !draft_poi_enabled;

                    if (
                        !draft_poi_enabled &&
                        config_cursor_idx > 2
                    ) {
                        config_cursor_idx = 2;
                    }
                }

                /*
                 * Подписи POI.
                 *
                 * Обратный цикл:
                 *
                 *     Все <- Важные <- Выкл <- Все
                 */
                else if (
                    config_cursor_idx == 3 &&
                    draft_poi_enabled
                ) {
                    if (
                        draft_poi_label_mode ==
                        PURRGO_POI_LABELS_ALL
                    ) {
                        draft_poi_label_mode =
                            PURRGO_POI_LABELS_OFF;
                    }
                    else {
                        draft_poi_label_mode--;
                    }
                }

                /*
                 * Режим записи трека.
                 *
                 * Обратный цикл:
                 *
                 *     Выкл <- Стандарт <- Экспедиция <- Выкл
                 */
                else if (
                    config_cursor_idx == (draft_poi_enabled ? 4 : 3)
                ) {
                    if (
                        draft_log_mode == LOGGER_MODE_OFF
                    ) {
                        draft_log_mode = LOGGER_MODE_EXPEDITION;
                    }
                    else {
                        draft_log_mode--;
                    }
                }

                /*
                 * Отображение трека.
                 */
                else if (
                    config_cursor_idx == (draft_poi_enabled ? 5 : 4)
                ) {
                    draft_track_display_enabled =
                        !draft_track_display_enabled;
                }

                return true;


            /*
             * --------------------------------------------------------
             * OK
             * --------------------------------------------------------
             */
            case PURRGO_BTN_OK:

                /*
                 * TZ.
                 */
                if (config_cursor_idx == 0) {

                    app_config.tz_offset_minutes =
                        draft_tz_offset_minutes;

                    purrgo_config_save();

                    *next_state_out =
                        APP_STATE_MAP;

                    purrgo_app_map_mark_dirty();
                }

                /*
                 * DIR.
                 */
                else if (config_cursor_idx == 1) {

                    *next_state_out =
                        APP_STATE_MENU_DIR_SELECT;

                    purrgo_config_controller_on_enter(
                        APP_STATE_MENU_DIR_SELECT
                    );
                }

                /*
                 * POI.
                 *
                 * Настройки POI сохраняются вместе.
                 */
                else if (
                    config_cursor_idx == 2 ||
                    config_cursor_idx == 3
                ) {

                    app_config.poi_enabled =
                        draft_poi_enabled;

                    app_config.poi_label_mode =
                        draft_poi_label_mode;

                    purrgo_config_save();

                    *next_state_out =
                        APP_STATE_MAP;

                    purrgo_app_map_mark_dirty();
                }

                /*
                 * TRACK SETTINGS
                 */
                else if (
                    config_cursor_idx == (draft_poi_enabled ? 4 : 3) ||
                    config_cursor_idx == (draft_poi_enabled ? 5 : 4)
                ) {

                    app_config.log_mode =
                        draft_log_mode;

                    purrgo_logger_set_mode(
                        app_config.log_mode
                    );

                    app_config.track_display_enabled =
                        draft_track_display_enabled;

                    purrgo_config_save();

                    *next_state_out =
                        APP_STATE_MAP;

                    purrgo_app_map_mark_dirty();
                }

                return true;


            /*
             * --------------------------------------------------------
             * MENU / BACK
             * --------------------------------------------------------
             */
            case PURRGO_BTN_MENU:

                /*
                 * Черновые изменения не применяем.
                 */
                *next_state_out =
                    APP_STATE_MAP;

                purrgo_app_map_mark_dirty();

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
    if (current_state == APP_STATE_MENU_DIR_SELECT) {

        switch (button) {

            /*
             * --------------------------------------------------------
             * UP
             * --------------------------------------------------------
             */
            case PURRGO_BTN_UP:

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
            case PURRGO_BTN_DOWN:

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
            case PURRGO_BTN_OK:

                if (
                    dir_page_count > 0 &&
                    dir_page_cursor >= 0 &&
                    dir_page_cursor < dir_page_count
                ) {

                    snprintf(
                        app_config.map_dir,
                        sizeof(app_config.map_dir),
                        "../../../tests/data/maps/%s",
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
            case PURRGO_BTN_MENU:

                leave_directory_browser();

                *next_state_out =
                    APP_STATE_MENU_CONFIG;

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


bool config_app_get_draft_poi_enabled(void)
{
    return draft_poi_enabled;
}


purrgo_poi_label_mode_t
config_app_get_draft_poi_label_mode(void)
{
    return draft_poi_label_mode;
}


track_logger_mode_t
config_app_get_draft_log_mode(void)
{
    return draft_log_mode;
}


bool
config_app_get_draft_track_display_enabled(void)
{
    return draft_track_display_enabled;
}