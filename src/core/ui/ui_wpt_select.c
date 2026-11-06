#include "ui_wpt_select.h"
#include "purrgo/gfx_text.h"
#include "purrgo/config.h"
#include "purrgo/menu_controller.h"
#include "purrgo/pgo_text.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/waypoint.h"
#include "ui_rows.h"

#define WPT_LIST_Y0 25
#define WPT_ROW_H   12

static ui_rows_t s_rows;

void ui_menu_wpt_select_reset(void)
{
    ui_rows_reset(&s_rows);
}

void ui_render_menu_wpt_select(gfx_context_t* gfx)
{
    char buf[PURRGO_WPT_NAME_MAX + 16];

    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);

    gfx_draw_string(gfx, 10, 10, "= WAYPOINT =");

    const int count   = menu_app_get_wpt_count();
    const int cursor  = menu_app_get_wpt_cursor();
    const int top     = menu_app_get_wpt_top();
    const int visible = menu_app_get_wpt_visible();

    ui_rows_begin(&s_rows, WPT_LIST_Y0, WPT_ROW_H);

    int shown = 0;

    for (int i = top; i < count && shown < visible; i++, shown++) {
        menu_app_get_wpt_item_text(i, buf, sizeof(buf));
        purrgo_utf8_to_pgo_inplace(buf);     /* имена из GPX: UTF-8 -> PurrGO-256 */

        gfx_set_color(gfx, (i == cursor) ? 3 : 0, (i == cursor) ? 0 : 3);
        gfx_draw_string(gfx, 10, (int16_t)(WPT_LIST_Y0 + shown * WPT_ROW_H), buf);

        ui_rows_update(&s_rows, shown, i == cursor, buf);
    }

    /* Подсказка под списком: откуда берутся точки файла. */
    gfx_set_color(gfx, 0, 3);

    int16_t y = (int16_t)(WPT_LIST_Y0 + shown * WPT_ROW_H + 6);

    if (app_config.route_file[0] == '\0') {
        gfx_draw_string(gfx, 10, y, "Points: set ROUTE (GPX)");
    }
    else if (purrgo_wpt_list_count() == 0) {
        gfx_draw_string(gfx, 10, y, "No <wpt> in ROUTE file");
    }
    else if (purrgo_wpt_list_truncated()) {
        gfx_draw_string(gfx, 10, y, "List truncated");
    }

    ui_rows_end(&s_rows, shown);
}
