#include "ui_dir_select.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_text.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/config_controller.h"
#include "purrgo/fs_hal.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/display_hal.h"
#include "purrgo/hardware_config.h"

static int prev_dir_cursor = -1;

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

    int count = config_app_get_dir_list(&dir_list);
    int cursor = config_app_get_dir_cursor();

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

        purrgo_snprintf(
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

    if (cursor != prev_dir_cursor) {
        if (prev_dir_cursor >= 0 && prev_dir_cursor < count) {
            int prev_y = 25 + prev_dir_cursor * 12;
            display_refresh_region(0, prev_y, PURRGO_HW_DISPLAY_WIDTH_PX, 12);
        }
        if (cursor >= 0 && cursor < count) {
            int curr_y = 25 + cursor * 12;
            display_refresh_region(0, curr_y, PURRGO_HW_DISPLAY_WIDTH_PX, 12);
        }
        prev_dir_cursor = cursor;
    }
}