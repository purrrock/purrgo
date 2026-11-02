#include "ui_map_layers.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_text.h"
#include "purrgo/config_controller.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/display_hal.h"
#include "purrgo/hardware_config.h"
#include "ui_rows.h"

#define LAYER_LIST_Y0 30
#define LAYER_ROW_H   15

static ui_rows_t s_rows;

void ui_menu_map_layers_reset(void) {
    ui_rows_reset(&s_rows);
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

    /* Переключение галочки (SEL) меняет текст при том же курсоре. */
    ui_rows_update(&s_rows, index, cursor == index, buf);
}

void ui_render_menu_map_layers(gfx_context_t* gfx) {
    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);
    gfx_set_color(gfx, 0, 3);

    int cursor = config_app_get_map_layers_cursor();

    gfx_draw_string(gfx, 10, 10, "MAP LAYERS");

    const uint16_t layers = config_app_get_draft_layers();

    ui_rows_begin(&s_rows, LAYER_LIST_Y0, LAYER_ROW_H);

    for (int i = 0; i < PURRGO_LAYER_COUNT; i++) {
        draw_layer_item(gfx, LAYER_LIST_Y0 + i * LAYER_ROW_H, i, cursor,
                        purrgo_layer_on(layers, (purrgo_layer_t)i),
                        purrgo_layer_title((purrgo_layer_t)i));
    }

    ui_rows_end(&s_rows, PURRGO_LAYER_COUNT);
}
