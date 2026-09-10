#include "ui_map_layers.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_text.h"
#include "purrgo/config_controller.h"
#include <stdio.h>

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
    snprintf(buf, sizeof(buf), "%s %s", enabled ? "x" : " ", label);

    gfx_draw_string(gfx, 10, y, buf);
}

void ui_render_menu_map_layers(gfx_context_t* gfx) {
    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);
    gfx_set_color(gfx, 0, 3);

    int cursor = config_app_get_map_layers_cursor();

    gfx_draw_string(gfx, 10, 10, "MAP LAYERS");

    draw_layer_item(gfx, 30, 0, cursor, config_app_get_draft_layer_landuse(), "Landuse");
    draw_layer_item(gfx, 45, 1, cursor, config_app_get_draft_layer_water(), "Water");
    draw_layer_item(gfx, 60, 2, cursor, config_app_get_draft_layer_landuse_labels(), "Landuse labels");
    draw_layer_item(gfx, 75, 3, cursor, config_app_get_draft_layer_water_labels(), "Water labels");
    draw_layer_item(gfx, 90, 4, cursor, config_app_get_draft_layer_roads(), "Roads");
    draw_layer_item(gfx, 105, 5, cursor, config_app_get_draft_layer_poi(), "POI");
    draw_layer_item(gfx, 120, 6, cursor, config_app_get_draft_layer_poi_labels(), "POI labels");
    draw_layer_item(gfx, 135, 7, cursor, config_app_get_draft_layer_route(), "Route");
    draw_layer_item(gfx, 150, 8, cursor, config_app_get_draft_layer_track(), "Track");
}
