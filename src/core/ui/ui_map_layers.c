#include "ui_map_layers.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_text.h"
#include "purrgo/config_controller.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/display_hal.h"
#include "purrgo/hardware_config.h"

#define LAYER_LIST_Y0 30
#define LAYER_ROW_H   15

static int prev_map_layers_cursor = -1;

void ui_menu_map_layers_reset(void) {
    prev_map_layers_cursor = -1;
}

static void draw_layer_item(gfx_context_t* gfx, int y, int index, int cursor, bool enabled, const char* label) {
    if (cursor == index) {
        gfx_set_color(gfx, 3, 0);
    } else {
        gfx_set_color(gfx, 0, 3);
    }

    char buf[64];

    /*
     * Используем символ 'x' как фактически поддерживаемый
     * текущим embedded-шрифтом (вместо юникодной галочки ✓).
     */
    purrgo_snprintf(buf, sizeof(buf), "%s %s", enabled ? "x" : " ", label);

    gfx_draw_string(gfx, 10, (int16_t)y, buf);
}

void ui_render_menu_map_layers(gfx_context_t* gfx) {
    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);
    gfx_set_color(gfx, 0, 3);

    int cursor = config_app_get_map_layers_cursor();

    gfx_draw_string(gfx, 10, 10, "MAP LAYERS");

    const uint16_t layers = config_app_get_draft_layers();

    for (int i = 0; i < PURRGO_LAYER_COUNT; i++) {
        draw_layer_item(gfx, LAYER_LIST_Y0 + i * LAYER_ROW_H, i, cursor,
                        purrgo_layer_on(layers, (purrgo_layer_t)i),
                        purrgo_layer_title((purrgo_layer_t)i));
    }

    if (prev_map_layers_cursor == -1) {
        prev_map_layers_cursor = cursor;
    } else if (cursor != prev_map_layers_cursor) {
        if (prev_map_layers_cursor >= 0 && prev_map_layers_cursor < PURRGO_LAYER_COUNT) {
            int16_t prev_y = (int16_t)(LAYER_LIST_Y0 + prev_map_layers_cursor * LAYER_ROW_H);
            display_refresh_region(0, prev_y, PURRGO_HW_DISPLAY_WIDTH_PX, LAYER_ROW_H);
        }
        if (cursor >= 0 && cursor < PURRGO_LAYER_COUNT) {
            int16_t curr_y = (int16_t)(LAYER_LIST_Y0 + cursor * LAYER_ROW_H);
            display_refresh_region(0, curr_y, PURRGO_HW_DISPLAY_WIDTH_PX, LAYER_ROW_H);
        }
        prev_map_layers_cursor = cursor;
    }
}
