#include "ui_dir_select.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_text.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/fs_hal.h"
#include <stdio.h>

void ui_render_menu_dir_select(gfx_context_t* gfx) {
    char buf[PURRGO_FS_MAX_PATH + 32];
    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);
    gfx_draw_string(gfx, 10, 10, "=== SELECT DIR ===");

    purrgo_fs_dirent_t* dir_list;
    int count = purrgo_app_get_dir_list(&dir_list);
    int cursor = purrgo_app_get_dir_cursor();
    int y_pos = 25;

    int display_start = 0;
    if (cursor > 5) {
        display_start = cursor - 5;
    }
    if (display_start + 10 > count && count > 10) {
        display_start = count - 10;
    }

    for (int i = display_start; i < count && i < display_start + 10; i++) {
        if (i == cursor) {
            gfx_set_color(gfx, 3, 0); // Invert
        } else {
            gfx_set_color(gfx, 0, 3);
        }

        snprintf(buf, sizeof(buf), "[%s]", dir_list[i].name);
        gfx_draw_string(gfx, 10, y_pos, buf);
        y_pos += 12;
    }

    if (count == 0) {
        gfx_set_color(gfx, 0, 3);
        gfx_draw_string(gfx, 10, y_pos, "(No directories)");
    }
}
