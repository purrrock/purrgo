#include "ui_dir_select.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_text.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/fs_hal.h"
#include <stdio.h>


void ui_render_menu_dir_select(gfx_context_t* gfx)
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

    gfx_draw_string(gfx, 10, 10, "=== SELECT DIR ===");


    /*
     * Контроллер возвращает только текущую страницу.
     *
     * Поэтому UI больше не вычисляет display_start и не предполагает,
     * что массив содержит весь каталог.
     */
    purrgo_fs_dirent_t* dir_list;

    int count = purrgo_app_get_dir_list(&dir_list);
    int cursor = purrgo_app_get_dir_cursor();

    int y_pos = 25;


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

        snprintf(
            buf,
            sizeof(buf),
            "[%s]",
            dir_list[i].name
        );

        gfx_draw_string(
            gfx,
            10,
            y_pos,
            buf
        );

        y_pos += 12;
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
            "(No directories)"
        );
    }
}