#include "menu_internal.h"
#include "purrgo/config.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/ui/ui_dirty.h"
#include <string.h>

/*
 * Экраны выбора каталога карт (DIR) и GPX-файла маршрута (ROUTE).
 * Оба работают с одним браузером: каталог читается постранично, в RAM
 * находится только текущая страница.
 */

/* Количество записей каталога, одновременно находящихся в RAM. */
#define DIR_PAGE_SIZE 10

/* Что показывает браузер: каталоги карт или GPX-файлы маршрутов. */
typedef enum {
    BROWSE_MAP_DIRS,
    BROWSE_ROUTE_FILES
} browse_kind_t;

static browse_kind_t browse_kind = BROWSE_MAP_DIRS;

/* Текущая страница, число записей в ней и курсор внутри неё. */
static purrgo_fs_dirent_t dir_page[DIR_PAGE_SIZE];
static int dir_page_count  = 0;
static int dir_page_cursor = 0;

/* Глобальный индекс первой реальной записи текущей страницы. */
static int dir_page_start = 0;

/* Открытый каталог (NULL, если закрыт). */
static purrgo_dir_t* dir_handle = NULL;

/*
 * true - первая запись страницы синтетическая "(none)" (снять маршрут).
 * Бывает только на первой странице списка маршрутов; в счёт dir_page_start
 * не входит (он считает только реальные записи каталога).
 */
static bool dir_page_has_none = false;


/* ---------------------------------------------------------------------------
 * Фильтры записей
 * ------------------------------------------------------------------------- */

static const char* get_base_path(void)
{
    return (browse_kind == BROWSE_ROUTE_FILES)
        ? purrgo_fs_get_routes_path()
        : purrgo_fs_get_maps_path();
}

/* Запись "." или "..". */
static bool is_dot_directory(const purrgo_fs_dirent_t* entry)
{
    if (entry->name[0] != '.') {
        return false;
    }
    if (entry->name[1] == '\0') {
        return true;
    }
    return entry->name[1] == '.' && entry->name[2] == '\0';
}

/*
 * Файл маршрута: обычный файл *.gpx (регистр расширения любой), имя которого
 * помещается в app_config.route_file; скрытые файлы (".", "._") пропускаются.
 */
static bool is_route_file(const purrgo_fs_dirent_t* entry)
{
    if (entry->is_directory || entry->name[0] == '.') {
        return false;
    }

    const size_t len = strlen(entry->name);

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

/* Видна ли запись пользователю в текущем браузере. */
static bool is_visible(const purrgo_fs_dirent_t* entry)
{
    if (browse_kind == BROWSE_ROUTE_FILES) {
        return is_route_file(entry);
    }
    return entry->is_directory && !is_dot_directory(entry);
}


/* ---------------------------------------------------------------------------
 * Чтение страниц каталога
 * ------------------------------------------------------------------------- */

static void close_dir(void)
{
    if (dir_handle != NULL) {
        purrgo_fs_closedir(dir_handle);
        dir_handle = NULL;
    }
}

static void reset_page(int page_start)
{
    dir_page_count    = 0;
    dir_page_cursor   = 0;
    dir_page_start    = page_start;
    dir_page_has_none = false;
}

/* Первой страницей списка маршрутов начинается запись "(none)". */
static void add_none_entry(void)
{
    dir_page[0].name[0]      = '\0';
    dir_page[0].is_directory = false;
    dir_page_count    = 1;
    dir_page_has_none = true;
}

/* Дочитывает страницу видимыми записями из открытого каталога. */
static void fill_page(void)
{
    purrgo_fs_dirent_t entry;

    while (dir_page_count < DIR_PAGE_SIZE &&
           purrgo_fs_readdir(dir_handle, &entry)) {
        if (is_visible(&entry)) {
            dir_page[dir_page_count++] = entry;
        }
    }
}

static void load_first_page(void)
{
    close_dir();
    reset_page(0);

    if (browse_kind == BROWSE_ROUTE_FILES) {
        add_none_entry();
    }

    dir_handle = purrgo_fs_opendir(get_base_path());
    if (dir_handle != NULL) {
        fill_page();
    }
}

/* Следующая страница. false - записей больше нет, страница не изменилась. */
static bool load_next_page(void)
{
    purrgo_fs_dirent_t first;
    bool found = false;

    while (purrgo_fs_readdir(dir_handle, &first)) {
        if (is_visible(&first)) {
            found = true;
            break;
        }
    }

    if (!found) {
        return false;
    }

    const int start = dir_page_start + dir_page_count - (dir_page_has_none ? 1 : 0);

    reset_page(start);
    dir_page[dir_page_count++] = first;
    fill_page();

    return true;
}

/* Страница, начинающаяся с глобального индекса page_start. */
static bool load_page_at(int page_start)
{
    purrgo_fs_dirent_t entry;
    int skipped = 0;

    close_dir();
    reset_page(page_start);

    if (browse_kind == BROWSE_ROUTE_FILES && page_start == 0) {
        add_none_entry();
    }

    dir_handle = purrgo_fs_opendir(get_base_path());
    if (dir_handle == NULL) {
        dir_page_start = 0;
        return false;
    }

    while (skipped < page_start && purrgo_fs_readdir(dir_handle, &entry)) {
        if (is_visible(&entry)) {
            skipped++;
        }
    }

    if (skipped < page_start) {
        close_dir();
        dir_page_start = 0;
        return false;
    }

    fill_page();

    if (dir_page_count == 0) {
        close_dir();
        dir_page_start = 0;
        return false;
    }

    return true;
}

static void leave_browser(void)
{
    close_dir();
    reset_page(0);
}


/* ---------------------------------------------------------------------------
 * Кнопки
 * ------------------------------------------------------------------------- */

static void browser_up(void)
{
    if (dir_page_cursor > 0) {
        dir_page_cursor--;
        return;
    }

    if (dir_page_start > 0) {
        const int prev = (dir_page_start >= DIR_PAGE_SIZE)
            ? dir_page_start - DIR_PAGE_SIZE : 0;

        if (load_page_at(prev)) {
            dir_page_cursor = dir_page_count - 1;
        }
    }
}

static void browser_down(void)
{
    if (dir_page_cursor < dir_page_count - 1) {
        dir_page_cursor++;
        return;
    }

    /* Неполная страница - последняя. */
    if (dir_page_count < DIR_PAGE_SIZE) {
        return;
    }

    if (load_next_page()) {
        dir_page_cursor = 0;
    }
}

/*
 * SEL на маршруте: пустое имя ("(none)") снимает маршрут. Выбор файла заодно
 * включает слой Route: иначе линия не появится.
 */
static void select_route(const char* name)
{
    purrgo_snprintf(app_config.route_file, sizeof(app_config.route_file),
                    "%s", name);

    if (app_config.route_file[0] != '\0') {
        app_config.layers = (uint16_t)(app_config.layers | (1u << PURRGO_LAYER_ROUTE));
    }

    purrgo_config_save();
    purrgo_ui_dirty_mark(UI_DIRTY_MAP);
}

static void select_map_dir(const char* name)
{
    purrgo_snprintf(app_config.map_dir, sizeof(app_config.map_dir),
                    "%s/%s", purrgo_fs_get_maps_path(), name);

    purrgo_config_save();
}

static void browser_select(purrgo_state_t* next)
{
    if (dir_page_count <= 0 ||
        dir_page_cursor < 0 ||
        dir_page_cursor >= dir_page_count) {
        return;
    }

    const char* name = dir_page[dir_page_cursor].name;

    if (browse_kind == BROWSE_ROUTE_FILES) {
        select_route(name);
    }
    else {
        select_map_dir(name);
    }

    leave_browser();
    *next = APP_STATE_MENU_MAIN;
}

static bool on_button(purrgo_action_t button, purrgo_state_t* next)
{
    switch (button) {
        case PURRGO_ACTION_UP:
            browser_up();
            return true;

        case PURRGO_ACTION_DOWN:
            browser_down();
            return true;

        case PURRGO_ACTION_OK:
            browser_select(next);
            return true;

        case PURRGO_ACTION_MENU:
            leave_browser();
            *next = APP_STATE_MENU_MAIN;
            purrgo_ui_dirty_mark(UI_DIRTY_MAP);
            return true;

        default:
            return false;
    }
}


/* ---------------------------------------------------------------------------
 * Экраны
 * ------------------------------------------------------------------------- */

static void on_enter_dir(void)
{
    browse_kind = BROWSE_MAP_DIRS;
    load_first_page();
}

static void on_enter_route(void)
{
    browse_kind = BROWSE_ROUTE_FILES;
    load_first_page();
}

const menu_screen_t menu_screen_dir_select = {
    .state     = APP_STATE_MENU_DIR_SELECT,
    .on_enter  = on_enter_dir,
    .on_button = on_button,
};

const menu_screen_t menu_screen_route_select = {
    .state     = APP_STATE_MENU_ROUTE_SELECT,
    .on_enter  = on_enter_route,
    .on_button = on_button,
};


int menu_app_get_dir_list(purrgo_fs_dirent_t** list_out)
{
    *list_out = dir_page;
    return dir_page_count;
}

int menu_app_get_dir_cursor(void)
{
    return dir_page_cursor;
}
