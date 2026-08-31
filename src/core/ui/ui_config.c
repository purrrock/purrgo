#include "ui_config.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_text.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/config.h"
#include "purrgo/config_controller.h"
#include "purrgo/fs_hal.h"
#include <stdio.h>


void ui_render_menu_config(gfx_context_t* gfx)
{
    char buf[PURRGO_FS_MAX_PATH];

    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);
    gfx_set_color(gfx, 0, 3);

    int16_t draft_tz =
        purrgo_app_get_draft_tz_offset();

    int cursor =
        purrgo_app_get_config_cursor();

    bool poi_enabled =
        config_app_get_draft_poi_enabled();

    purrgo_poi_label_mode_t poi_label_mode =
        config_app_get_draft_poi_label_mode();


    gfx_draw_string(
        gfx,
        10,
        10,
        "=== CONFIG ==="
    );


    /*
     * ---------------------------------------------------------------
     * TIME ZONE
     * ---------------------------------------------------------------
     */
    char sign =
        (draft_tz >= 0) ? '+' : '-';

    int16_t abs_tz =
        (draft_tz >= 0) ? draft_tz : -draft_tz;

    int hours = abs_tz / 60;
    int mins = abs_tz % 60;

    snprintf(
        buf,
        sizeof(buf),
        "TZ: UTC%c%02d:%02d",
        sign,
        hours,
        mins
    );

    if (cursor == 0) {
        gfx_set_color(gfx, 3, 0);
    }
    else {
        gfx_set_color(gfx, 0, 3);
    }

    gfx_draw_string(
        gfx,
        10,
        25,
        buf
    );


    /*
     * ---------------------------------------------------------------
     * MAP DIRECTORY
     * ---------------------------------------------------------------
     */
    if (cursor == 1) {
        gfx_set_color(gfx, 3, 0);
    }
    else {
        gfx_set_color(gfx, 0, 3);
    }

    snprintf(
        buf,
        sizeof(buf),
        "DIR: %s",
        app_config.map_dir
    );

    gfx_draw_string(
        gfx,
        10,
        40,
        buf
    );


    /*
     * ---------------------------------------------------------------
     * POI
     * ---------------------------------------------------------------
     */
    if (cursor == 2) {
        gfx_set_color(gfx, 3, 0);
    }
    else {
        gfx_set_color(gfx, 0, 3);
    }

    snprintf(
        buf,
        sizeof(buf),
        "POI: %s",
        poi_enabled ? "YES" : "NO"
    );

    gfx_draw_string(
        gfx,
        10,
        55,
        buf
    );


    /*
     * ---------------------------------------------------------------
     * POI LABELS
     * ---------------------------------------------------------------
     *
     * Этот пункт существует только если POI включены.
     */
    if (poi_enabled) {

        const char* label_text;

        switch (poi_label_mode) {

            case PURRGO_POI_LABELS_ALL:
                label_text = "ALL";
                break;

            case PURRGO_POI_LABELS_IMPORTANT:
                label_text = "IMPORTANT";
                break;

            case PURRGO_POI_LABELS_OFF:
            default:
                label_text = "OFF";
                break;
        }

        if (cursor == 3) {
            gfx_set_color(gfx, 3, 0);
        }
        else {
            gfx_set_color(gfx, 0, 3);
        }

        snprintf(
            buf,
            sizeof(buf),
            "POI LABELS: %s",
            label_text
        );

        gfx_draw_string(
            gfx,
            10,
            70,
            buf
        );
    }


    /*
     * ---------------------------------------------------------------
     * HELP
     * ---------------------------------------------------------------
     */
    gfx_set_color(gfx, 0, 3);

    gfx_draw_string(
        gfx,
        10,
        90,
        "UP/DN : Select"
    );

    gfx_draw_string(
        gfx,
        10,
        105,
        "+/- : Change"
    );

    gfx_draw_string(
        gfx,
        10,
        120,
        "OK  : Apply/Open"
    );

    gfx_draw_string(
        gfx,
        10,
        135,
        "MENU: Cancel"
    );
}