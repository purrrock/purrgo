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

    purrgo_poi_mode_t poi_mode =
        config_app_get_draft_poi_mode();
    bool poi_enabled = (poi_mode != PURRGO_POI_MODE_NO);

    purrgo_poi_label_mode_t poi_label_mode =
        config_app_get_draft_poi_label_mode();

    track_logger_mode_t log_mode =
        config_app_get_draft_log_mode();

    bool track_display_enabled =
        config_app_get_draft_track_display_enabled();


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

    const char* poi_mode_str;
    if (poi_mode == PURRGO_POI_MODE_CIRCLES) {
        poi_mode_str = "CIRCLES";
    } else if (poi_mode == PURRGO_POI_MODE_ICONS) {
        poi_mode_str = "ICONS";
    } else {
        poi_mode_str = "NO";
    }

    snprintf(
        buf,
        sizeof(buf),
        "POI: %s",
        poi_mode_str
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
     * LOG MODE
     * ---------------------------------------------------------------
     */
    int log_mode_idx = poi_enabled ? 4 : 3;

    if (cursor == log_mode_idx) {
        gfx_set_color(gfx, 3, 0);
    }
    else {
        gfx_set_color(gfx, 0, 3);
    }

    const char* log_mode_text;
    switch (log_mode) {
        case LOGGER_MODE_EXPEDITION:
            log_mode_text = "EXPEDITION";
            break;
        case LOGGER_MODE_STANDARD:
            log_mode_text = "STANDARD";
            break;
        case LOGGER_MODE_OFF:
        default:
            log_mode_text = "OFF";
            break;
    }

    snprintf(
        buf,
        sizeof(buf),
        "LOG: %s",
        log_mode_text
    );

    gfx_draw_string(
        gfx,
        10,
        poi_enabled ? 85 : 70,
        buf
    );

    /*
     * ---------------------------------------------------------------
     * TRACK DISPLAY
     * ---------------------------------------------------------------
     */
    int track_display_idx = poi_enabled ? 5 : 4;

    if (cursor == track_display_idx) {
        gfx_set_color(gfx, 3, 0);
    }
    else {
        gfx_set_color(gfx, 0, 3);
    }

    snprintf(
        buf,
        sizeof(buf),
        "SHOW TRACK: %s",
        track_display_enabled ? "ON" : "OFF"
    );

    gfx_draw_string(
        gfx,
        10,
        poi_enabled ? 100 : 85,
        buf
    );

    /*
     * ---------------------------------------------------------------
     * HELP
     * ---------------------------------------------------------------
     */
    gfx_set_color(gfx, 0, 3);

    gfx_draw_string(
        gfx,
        10,
        poi_enabled ? 120 : 105,
        "UP/DN : Select"
    );

    gfx_draw_string(
        gfx,
        10,
        poi_enabled ? 135 : 120,
        "+/- : Change"
    );

    gfx_draw_string(
        gfx,
        10,
        poi_enabled ? 150 : 135,
        "OK  : Apply/Open"
    );

    gfx_draw_string(
        gfx,
        10,
        poi_enabled ? 165 : 150,
        "MENU: Cancel"
    );
}