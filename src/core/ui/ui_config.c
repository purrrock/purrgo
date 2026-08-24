#include "ui_common.h"

void ui_render_menu_config(gfx_context_t* gfx) {
    char buf[PURRGO_FS_MAX_PATH];
    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);
    gfx_set_color(gfx, 0, 3);

    int16_t draft_tz = purrgo_app_get_draft_tz_offset();
    int cursor = purrgo_app_get_config_cursor();

    gfx_draw_string(gfx, 10, 10, "=== CONFIG ===");

    char sign = (draft_tz >= 0) ? '+' : '-';
    int16_t abs_tz = (draft_tz >= 0) ? draft_tz : -draft_tz;
    int hours = abs_tz / 60;
    int mins = abs_tz % 60;

    snprintf(buf, sizeof(buf), "TZ: UTC%c%02d:%02d", sign, hours, mins);

    if (cursor == 0) {
        gfx_set_color(gfx, 3, 0); // Invert
    } else {
        gfx_set_color(gfx, 0, 3);
    }

    gfx_draw_string(gfx, 10, 25, buf);

    if (cursor == 1) {
        gfx_set_color(gfx, 3, 0); // Invert
    } else {
        gfx_set_color(gfx, 0, 3);
    }

    snprintf(buf, sizeof(buf), "DIR: %s", app_config.map_dir);

    gfx_draw_string(gfx, 10, 40, buf);

    gfx_set_color(gfx, 0, 3);

    gfx_draw_string(gfx, 10, 60, "UP/DN : Select");
    gfx_draw_string(gfx, 10, 75, "+/- : Change");
    gfx_draw_string(gfx, 10, 90, "OK  : Apply/Open");
    gfx_draw_string(gfx, 10, 105, "MENU: Cancel");
}
