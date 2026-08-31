#include "purrgo/config_controller.h"
#include "purrgo/config.h"
#include <stdio.h>

/*
 * Количество элементов каталога, которые одновременно находятся в RAM.
 *
 * Это намеренно связано с размером страницы интерфейса, а не с общим
 * количеством директорий на SD-карте.
 *
 * При текущем purrgo_fs_dirent_t:
 *
 *     name[256] + is_directory
 *
 * один элемент занимает примерно 257 байт (с учётом возможного
 * выравнивания структуры).
 *
 * Поэтому 10 элементов имеют предсказуемый и ограниченный объём RAM.
 */
#define DIR_PAGE_SIZE 10

/*
 * Текущая страница каталога.
 *
 * В отличие от старого dir_list[], здесь никогда не хранится весь
 * каталог. В массиве находятся только элементы текущей страницы.
 */
static purrgo_fs_dirent_t dir_page[DIR_PAGE_SIZE];

/*
 * Фактическое количество элементов в текущей странице.
 *
 * Для полной страницы:
 *     0 .. DIR_PAGE_SIZE
 *
 * Последняя страница каталога может быть неполной.
 */
static int dir_page_count = 0;

/*
 * Положение курсора внутри текущей страницы.
 *
 * Диапазон:
 *
 *     0 .. dir_page_count - 1
 */
static int dir_page_cursor = 0;

/*
 * Глобальный индекс первого элемента текущей страницы.
 *
 * Например:
 *
 *     0   -> элементы 0..9
 *     10  -> элементы 10..19
 *     20  -> элементы 20..29
 */
static int dir_page_start = 0;

/*
 * Открытый каталог.
 *
 * Пока пользователь находится в браузере, каталог остаётся открытым,
 * что позволяет последовательно читать следующие страницы через
 * purrgo_fs_readdir().
 *
 * Для перехода назад каталог приходится открыть заново и пропустить
 * нужное количество записей, поскольку текущий fs_hal не предоставляет
 * rewind/seek для каталога.
 */
static purrgo_dir_t* dir_handle = NULL;

static int16_t draft_tz_offset_minutes;
static int config_cursor_idx = 0;


/*
 * Возвращает базовый каталог карт.
 */
static const char* get_maps_base_path(void)
{
    return "../../../tests/data/maps";
}


/*
 * Проверяет, является ли запись "." или "..".
 *
 * Эти две записи не должны отображаться пользователю.
 */
static bool is_dot_directory(const purrgo_fs_dirent_t* entry)
{
    if (entry->name[0] != '.') {
        return false;
    }

    /*
     * "."
     */
    if (entry->name[1] == '\0') {
        return true;
    }

    /*
     * ".."
     */
    if (entry->name[1] == '.' && entry->name[2] == '\0') {
        return true;
    }

    return false;
}


/*
 * Возвращает true только для каталогов, которые должны быть видимы
 * в пользовательском интерфейсе.
 */
static bool is_visible_directory(const purrgo_fs_dirent_t* entry)
{
    return entry->is_directory && !is_dot_directory(entry);
}


/*
 * Закрывает открытый каталог.
 *
 * Это отдельная функция, чтобы гарантировать отсутствие утечки
 * файлового ресурса при выходе из браузера или повторном открытии.
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
 *
 * После выполнения:
 *
 *     dir_page_start  == 0
 *     dir_page_cursor == 0
 *
 * В RAM будут находиться только первые DIR_PAGE_SIZE каталогов.
 */
static void load_first_directory_page(void)
{
    purrgo_fs_dirent_t entry;

    /*
     * На всякий случай закрываем прежнее состояние браузера.
     */
    close_directory_browser();

    dir_page_count = 0;
    dir_page_cursor = 0;
    dir_page_start = 0;

    /*
     * Открываем каталог.
     */
    dir_handle = purrgo_fs_opendir(get_maps_base_path());

    if (dir_handle == NULL) {
        return;
    }

    /*
     * Читаем только одну страницу.
     *
     * Любые остальные записи каталога до следующего вызова
     * purrgo_fs_readdir() в RAM не сохраняются.
     */
    while (dir_page_count < DIR_PAGE_SIZE &&
           purrgo_fs_readdir(dir_handle, &entry)) {

        if (!is_visible_directory(&entry)) {
            continue;
        }

        dir_page[dir_page_count] = entry;
        dir_page_count++;
    }
}


/*
 * Загружает следующую страницу каталога.
 *
 * Функция используется только после полностью заполненной текущей
 * страницы.
 *
 * Важный момент:
 * сначала ищется первый элемент следующей страницы во временной
 * переменной first_entry. Только после того, как такой элемент найден,
 * текущая страница заменяется новой.
 *
 * Поэтому при достижении конца каталога текущая страница не теряется.
 */
static bool load_next_directory_page(void)
{
    purrgo_fs_dirent_t entry;
    purrgo_fs_dirent_t first_entry;
    bool found = false;

    /*
     * Ищем первый реальный каталог следующей страницы.
     *
     * Все записи между текущей страницей и следующим каталогом
     * не сохраняются.
     */
    while (purrgo_fs_readdir(dir_handle, &entry)) {
        if (is_visible_directory(&entry)) {
            first_entry = entry;
            found = true;
            break;
        }
    }

    /*
     * Дошли до конца каталога.
     */
    if (!found) {
        return false;
    }

    /*
     * Текущая страница полная, поэтому переход происходит на
     * следующий блок DIR_PAGE_SIZE элементов.
     */
    dir_page_start += dir_page_count;

    dir_page_count = 0;
    dir_page_cursor = 0;

    /*
     * Первый элемент уже найден выше.
     */
    dir_page[dir_page_count] = first_entry;
    dir_page_count++;

    /*
     * Дочитываем остальные элементы новой страницы.
     */
    while (dir_page_count < DIR_PAGE_SIZE &&
           purrgo_fs_readdir(dir_handle, &entry)) {

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
 *
 * Эта функция нужна в первую очередь для движения назад.
 *
 * Так как fs_hal не предоставляет seek/rewind каталога, каталог
 * открывается заново, после чего первые page_start видимых каталогов
 * отбрасываются.
 *
 * В RAM при этом хранится только:
 *
 *     - одна временная запись entry;
 *     - текущая страница dir_page[].
 */
static bool load_directory_page(int page_start)
{
    purrgo_fs_dirent_t entry;
    int skipped = 0;

    /*
     * Начинаем новый проход с начала каталога.
     */
    close_directory_browser();

    dir_page_count = 0;
    dir_page_cursor = 0;
    dir_page_start = page_start;

    dir_handle = purrgo_fs_opendir(get_maps_base_path());

    if (dir_handle == NULL) {
        dir_page_start = 0;
        return false;
    }

    /*
     * Пропускаем page_start видимых каталогов.
     *
     * Они не записываются в память.
     */
    while (skipped < page_start &&
           purrgo_fs_readdir(dir_handle, &entry)) {

        if (!is_visible_directory(&entry)) {
            continue;
        }

        skipped++;
    }

    /*
     * До требуемой позиции не дошли.
     *
     * Например, каталог содержал только 15 элементов, а была
     * запрошена страница с индексом 20.
     */
    if (skipped < page_start) {
        close_directory_browser();
        dir_page_start = 0;
        return false;
    }

    /*
     * Загружаем только текущую страницу.
     */
    while (dir_page_count < DIR_PAGE_SIZE &&
           purrgo_fs_readdir(dir_handle, &entry)) {

        if (!is_visible_directory(&entry)) {
            continue;
        }

        dir_page[dir_page_count] = entry;
        dir_page_count++;
    }

    /*
     * Запрошенная страница оказалась пустой.
     */
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
    draft_tz_offset_minutes = app_config.tz_offset_minutes;
    config_cursor_idx = 0;
}


void purrgo_config_controller_on_enter(purrgo_state_t state)
{
    if (state == APP_STATE_MENU_CONFIG) {
        draft_tz_offset_minutes = app_config.tz_offset_minutes;
        config_cursor_idx = 0;
    }
    else if (state == APP_STATE_MENU_DIR_SELECT) {
        /*
         * При входе в файловый браузер начинаем с первой страницы.
         */
        load_first_directory_page();
    }
}


void purrgo_config_controller_update(
    const purrgo_gnss_solution_t* current_fix)
{
    /*
     * В настоящее время контроллер не требует периодического обновления.
     */
    (void)current_fix;
}


bool purrgo_config_controller_handle_button(
    purrgo_state_t current_state,
    purrgo_btn_t button,
    purrgo_state_t* next_state_out)
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

            case PURRGO_BTN_UP:
                if (config_cursor_idx > 0) {
                    config_cursor_idx--;
                }
                return true;


            case PURRGO_BTN_DOWN:
                if (config_cursor_idx < 1) {
                    config_cursor_idx++;
                }
                return true;


            case PURRGO_BTN_PLUS:
            case PURRGO_BTN_RIGHT:
                if (config_cursor_idx == 0) {
                    if (draft_tz_offset_minutes + 15 <= 840) {
                        draft_tz_offset_minutes += 15;
                    }
                }
                return true;


            case PURRGO_BTN_MINUS:
            case PURRGO_BTN_LEFT:
                if (config_cursor_idx == 0) {
                    if (draft_tz_offset_minutes - 15 >= -720) {
                        draft_tz_offset_minutes -= 15;
                    }
                }
                return true;


            case PURRGO_BTN_OK:

                /*
                 * Сохранение часового пояса.
                 */
                if (config_cursor_idx == 0) {
                    app_config.tz_offset_minutes = draft_tz_offset_minutes;

                    purrgo_config_save();

                    *next_state_out = APP_STATE_MAP;
                    purrgo_app_map_mark_dirty();
                }

                /*
                 * Переход в браузер каталогов.
                 */
                else if (config_cursor_idx == 1) {
                    *next_state_out = APP_STATE_MENU_DIR_SELECT;

                    purrgo_config_controller_on_enter(
                        APP_STATE_MENU_DIR_SELECT);
                }

                return true;


            case PURRGO_BTN_MENU:
                *next_state_out = APP_STATE_MAP;
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

                /*
                 * Есть предыдущий элемент внутри текущей страницы.
                 */
                if (dir_page_cursor > 0) {
                    dir_page_cursor--;
                    return true;
                }

                /*
                 * Мы уже на первом элементе текущей страницы.
                 *
                 * Если есть предыдущая страница, загружаем её.
                 */
                if (dir_page_start > 0) {

                    int previous_page_start;

                    /*
                     * В обычном случае:
                     *
                     *     20 -> 10
                     *     10 -> 0
                     */
                    if (dir_page_start >= DIR_PAGE_SIZE) {
                        previous_page_start =
                            dir_page_start - DIR_PAGE_SIZE;
                    }
                    else {
                        previous_page_start = 0;
                    }

                    if (load_directory_page(previous_page_start)) {

                        /*
                         * После перехода назад выбираем последний
                         * каталог предыдущей страницы.
                         */
                        dir_page_cursor = dir_page_count - 1;
                    }
                }

                return true;


            /*
             * --------------------------------------------------------
             * DOWN
             * --------------------------------------------------------
             */
            case PURRGO_BTN_DOWN:

                /*
                 * Есть следующий элемент внутри текущей страницы.
                 */
                if (dir_page_cursor < dir_page_count - 1) {
                    dir_page_cursor++;
                    return true;
                }

                /*
                 * Если текущая страница неполная, значит это последняя
                 * страница каталога.
                 *
                 * Ничего больше загружать не нужно.
                 */
                if (dir_page_count < DIR_PAGE_SIZE) {
                    return true;
                }

                /*
                 * Текущая страница полная.
                 *
                 * Пробуем перейти к следующей странице.
                 */
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

                if (dir_page_count > 0 &&
                    dir_page_cursor >= 0 &&
                    dir_page_cursor < dir_page_count) {

                    /*
                     * В конфигурацию записывается только выбранный
                     * каталог.
                     */
                    snprintf(
                        app_config.map_dir,
                        sizeof(app_config.map_dir),
                        "../../../tests/data/maps/%s",
                        dir_page[dir_page_cursor].name
                    );

                    purrgo_config_save();

                    /*
                     * Каталог нам больше не нужен.
                     */
                    leave_directory_browser();

                    *next_state_out = APP_STATE_MENU_CONFIG;
                }

                return true;


            /*
             * --------------------------------------------------------
             * MENU / BACK
             * --------------------------------------------------------
             */
            case PURRGO_BTN_MENU:

                leave_directory_browser();

                *next_state_out = APP_STATE_MENU_CONFIG;

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


/*
 * Возвращает только текущую страницу каталога.
 *
 * Важно:
 * это больше НЕ весь список директорий.
 */
int config_app_get_dir_list(purrgo_fs_dirent_t** list_out)
{
    *list_out = dir_page;
    return dir_page_count;
}


/*
 * Возвращает позицию курсора внутри текущей страницы.
 */
int config_app_get_dir_cursor(void)
{
    return dir_page_cursor;
}