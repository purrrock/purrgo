#include "ui_config.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_text.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/config.h"
#include "purrgo/config_controller.h"
#include "purrgo/fs_hal.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/display_hal.h"
#include "purrgo/hardware_config.h"

static int prev_config_cursor = -1;

void ui_render_menu_config(gfx_context_t* gfx)
{
    char buf[PURRGO_FS_MAX_PATH + 64];

    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);
    gfx_set_color(gfx, 0, 3);

    int16_t draft_tz =
        config_app_get_draft_tz_offset();

    int cursor =
        config_app_get_config_cursor();

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

    purrgo_snprintf(
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

    purrgo_snprintf(
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

    purrgo_snprintf(
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

    purrgo_snprintf(
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

    purrgo_snprintf(
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
     * POWER OFF
     * ---------------------------------------------------------------
     */
    if (cursor == 6) {
        gfx_set_color(gfx, 3, 0);
    }
    else {
        gfx_set_color(gfx, 0, 3);
    }

    gfx_draw_string(
        gfx,
        10,
        115,
        "POWER OFF"
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
        135,
        "UP/DN: Select"
    );

    gfx_draw_string(
        gfx,
        10,
        150,
        "SEL  : Change"
    );

    gfx_draw_string(
        gfx,
        10,
        165,
        "BACK : Save"
    );

    if (prev_config_cursor == -1) {
        /* First render, the full screen refresh is handled by state change,
           just remember the cursor. */
        prev_config_cursor = cursor;
    } else if (cursor != prev_config_cursor) {
        if (prev_config_cursor >= 0 && prev_config_cursor <= 6) {
            int prev_y = 25 + prev_config_cursor * 15;
            display_refresh_region(0, prev_y, PURRGO_HW_DISPLAY_WIDTH_PX, 15);
        }
        if (cursor >= 0 && cursor <= 6) {
            int curr_y = 25 + cursor * 15;
            display_refresh_region(0, curr_y, PURRGO_HW_DISPLAY_WIDTH_PX, 15);
        }
        prev_config_cursor = cursor;
    }
}