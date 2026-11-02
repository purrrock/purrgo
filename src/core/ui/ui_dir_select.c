#include "ui_dir_select.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_text.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/config.h"
#include "purrgo/config_controller.h"
#include <string.h>
#include "purrgo/fs_hal.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/display_hal.h"
#include "purrgo/hardware_config.h"
#include "ui_rows.h"

#define DIR_LIST_Y0 25
#define DIR_ROW_H   12

static ui_rows_t s_rows;

void ui_menu_dir_select_reset(void) {
    ui_rows_reset(&s_rows);
}

/*
 * Общий список для выбора каталога карты и файла маршрута.
 * route_mode: без скобок, пустое имя - "(none)", активный маршрут помечен "*".
 */
static void render_browser(gfx_context_t* gfx, const char* title, bool route_mode)
{
    /*
     * Буфер используется только для формирования строки одного
     * отображаемого элемента.
     *
     * Он не зависит от количества директорий на SD.
     */
    char buf[PURRGO_FS_MAX_PATH + 32];

    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);

    gfx_draw_string(gfx, 10, 10, title);


    /*
     * Контроллер возвращает только текущую страницу.
     *
     * Поэтому UI больше не вычисляет display_start и не предполагает,
     * что массив содержит весь каталог.
     */
    purrgo_fs_dirent_t* dir_list;

    int count = config_app_get_dir_list(&dir_list);
    int cursor = config_app_get_dir_cursor();

    int16_t y_pos = DIR_LIST_Y0;

    ui_rows_begin(&s_rows, DIR_LIST_Y0, DIR_ROW_H);


    /*
     * Рисуем элементы только текущей страницы.
     */
    for (int i = 0; i < count; i++) {

        /*
         * Выбранный элемент отображается инверсией.
         */
        if (i == cursor) {
            gfx_set_color(gfx, 3, 0);
        }
        else {
            gfx_set_color(gfx, 0, 3);
        }

        if (!route_mode) {
            purrgo_snprintf(buf, sizeof(buf), "[%s]", dir_list[i].name);
        }
        else if (dir_list[i].name[0] == '\0') {
            purrgo_snprintf(buf, sizeof(buf), "(none)%s",
                            (app_config.route_file[0] == '\0') ? " *" : "");
        }
        else {
            purrgo_snprintf(buf, sizeof(buf), "%s%s", dir_list[i].name,
                            (strcmp(dir_list[i].name, app_config.route_file) == 0) ? " *" : "");
        }

        gfx_draw_string(
            gfx,
            10,
            y_pos,
            buf
        );

        /* Смена страницы списка и перемещение курсора меняют текст/выделение строк. */
        ui_rows_update(&s_rows, i, i == cursor, buf);

        y_pos = (int16_t)(y_pos + DIR_ROW_H);
    }


    /*
     * Пустой каталог.
     */
    if (count == 0) {
        gfx_set_color(gfx, 0, 3);

        gfx_draw_string(
            gfx,
            10,
            y_pos,
            route_mode ? "(No routes)" : "(No directories)"
        );
    }

    ui_rows_end(&s_rows, count);
}

void ui_render_menu_dir_select(gfx_context_t* gfx)
{
    render_browser(gfx, "=== SELECT DIR ===", false);
}

void ui_render_menu_route_select(gfx_context_t* gfx)
{
    render_browser(gfx, "= SELECT ROUTE =", true);
}
