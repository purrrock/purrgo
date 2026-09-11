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
    char buf[PURRGO_FS_MAX_PATH + 64];

    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);
    gfx_set_color(gfx, 0, 3);

    int16_t draft_tz =
        purrgo_app_get_draft_tz_offset();

    int cursor =
        purrgo_app_get_config_cursor();

    purrgo_poi_mode_t poi_mode =
        config_app_get_draft_poi_mode();


    track_logger_mode_t log_mode =
        config_app_get_draft_log_mode();

    purrgo_map_details_t map_details =
        config_app_get_draft_map_details();


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
     * LOG MODE
     * ---------------------------------------------------------------
     */
    if (cursor == 3) {
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
        70,
        buf
    );

    /*
     * ---------------------------------------------------------------
     * MAP DETAILS
     * ---------------------------------------------------------------
     */
    if (cursor == 4) {
        gfx_set_color(gfx, 3, 0);
    }
    else {
        gfx_set_color(gfx, 0, 3);
    }

    const char* map_details_str;
    if (map_details == PURRGO_MAP_DETAILS_HIGH) {
        map_details_str = "HIGH";
    } else {
        map_details_str = "LOW";
    }

    snprintf(
        buf,
        sizeof(buf),
        "MAP DETAILS: %s",
        map_details_str
    );

    gfx_draw_string(
        gfx,
        10,
        85,
        buf
    );

    /*
     * ---------------------------------------------------------------
     * MAP LAYERS
     * ---------------------------------------------------------------
     */
    if (cursor == 5) {
        gfx_set_color(gfx, 3, 0);
    }
    else {
        gfx_set_color(gfx, 0, 3);
    }

    gfx_draw_string(
        gfx,
        10,
        100,
        "MAP LAYERS"
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
        120,
        "UP/DN : Select"
    );

    gfx_draw_string(
        gfx,
        10,
        135,
        "+/- : Change"
    );

    gfx_draw_string(
        gfx,
        10,
        150,
        "OK  : Apply/Open"
    );

    gfx_draw_string(
        gfx,
        10,
        165,
        "MENU: Cancel"
    );
}