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
#include "ui_rows.h"

static ui_rows_t s_rows;

/* Первая строка списка и шаг строк, px. */
#define CONFIG_ROW_Y0    25
#define CONFIG_ROW_STEP  15

void ui_menu_config_reset(void) {
    ui_rows_reset(&s_rows);
}

/*
 * Рисует пункт меню в строке row: выбранный (row == cursor) - инверсно.
 * Изменение текста или выделения строки регистрирует её область обновления
 * (при смене значения кнопкой SEL курсор остаётся на месте, меняется текст).
 */
static void draw_item(gfx_context_t* gfx, int cursor, int row, const char* text)
{
    if (cursor == row) {
        gfx_set_color(gfx, 3, 0);
    }
    else {
        gfx_set_color(gfx, 0, 3);
    }

    gfx_draw_string(
        gfx,
        10,
        (int16_t)(CONFIG_ROW_Y0 + row * CONFIG_ROW_STEP),
        text
    );

    ui_rows_update(&s_rows, row, cursor == row, text);
}

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

    purrgo_power_source_t power_src =
        config_app_get_draft_power_source();


    gfx_draw_string(
        gfx,
        10,
        10,
        "=== CONFIG ==="
    );

    /*
     * Порядок строк совпадает с CONFIG_CURSOR_* в config_controller.c:
     * 0 POWER SRC, 1 POWER OFF, 2 TZ, 3 DIR, 4 ROUTE, 5 POI, 6 LOG,
     * 7 MAP DETAILS, 8 MAP LAYERS.
     */
    int row = 0;

    ui_rows_begin(&s_rows, CONFIG_ROW_Y0, CONFIG_ROW_STEP);

    /* POWER SOURCE */
    purrgo_snprintf(
        buf,
        sizeof(buf),
        "POWER: %s",
        purrgo_power_source_title(power_src)
    );
    draw_item(gfx, cursor, row++, buf);

    /* POWER OFF */
    draw_item(gfx, cursor, row++, "POWER OFF");

    /* TIME ZONE */
    char sign =
        (draft_tz >= 0) ? '+' : '-';

    int16_t abs_tz =
        (int16_t)((draft_tz >= 0) ? draft_tz : -draft_tz);

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
    draw_item(gfx, cursor, row++, buf);

    /* MAP DIRECTORY */
    purrgo_snprintf(
        buf,
        sizeof(buf),
        "DIR: %s",
        app_config.map_dir
    );
    draw_item(gfx, cursor, row++, buf);

    /* ROUTE (GPX-файл из каталога ROUTES) */
    purrgo_snprintf(
        buf,
        sizeof(buf),
        "ROUTE: %s",
        (app_config.route_file[0] != '\0') ? app_config.route_file : "NONE"
    );
    draw_item(gfx, cursor, row++, buf);

    /* POI */
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
    draw_item(gfx, cursor, row++, buf);

    /* LOG MODE */
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
    draw_item(gfx, cursor, row++, buf);

    /* MAP DETAILS */
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
    draw_item(gfx, cursor, row++, buf);

    /* MAP LAYERS */
    draw_item(gfx, cursor, row++, "MAP LAYERS");

    /*
     * HELP: под последней строкой списка.
     */
    gfx_set_color(gfx, 0, 3);

    const int16_t help_y =
        (int16_t)(CONFIG_ROW_Y0 + row * CONFIG_ROW_STEP + 5);

    gfx_draw_string(
        gfx,
        10,
        help_y,
        "UP/DN: Select"
    );

    gfx_draw_string(
        gfx,
        10,
        (int16_t)(help_y + 15),
        "SEL  : Change"
    );

    gfx_draw_string(
        gfx,
        10,
        (int16_t)(help_y + 30),
        "BACK : Save"
    );

    ui_rows_end(&s_rows, row);
}
