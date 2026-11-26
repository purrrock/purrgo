#include "ui_search.h"

#include "purrgo/display_hal.h"
#include "purrgo/gfx_icon.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_text.h"
#include "purrgo/hardware_config.h"
#include "purrgo/map_search.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/purrgo_poi_icons.h"
#include "purrgo/search_screen.h"
#include "purrgo/waypoint.h"
#include "ui_keyboard.h"
#include "ui_rows.h"

#include <string.h>

#define STATUS_Y    UI_KEYBOARD_FREE_TOP_PX              /* 34 */
#define ROW_H       12
#define LIST_Y0     (STATUS_Y + ROW_H)                   /* 46 */
#define ICON_CX     7
#define NAME_X      14
#define NAME_CHARS  17
#define RIGHT_MARGIN 3
#define HINT_Y1     244
#define HINT_Y2     254
#define HINT_H      18

static ui_rows_t s_rows;
static uint32_t  s_status_sig;
static bool      s_status_valid;
static int       s_last_focus = -1;

void ui_search_reset(void)
{
    ui_rows_reset(&s_rows);
    s_status_valid = false;
    s_last_focus = -1;
    ui_keyboard_reset();
}

/* Название не длиннее NAME_CHARS символов; пустое - "(no name)". */
static void fit_name(const char *name, char *out, size_t size)
{
    const size_t len = strlen(name);

    if (len == 0u) {
        purrgo_snprintf(out, size, "(no name)");
    } else if (len <= NAME_CHARS) {
        purrgo_snprintf(out, size, "%s", name);
    } else {
        purrgo_snprintf(out, size, "%.*s..", NAME_CHARS - 2, name);
    }
}

/* Значок слоя в колонке иконки: у POI - иконка по коду Type, у остальных слоёв - знак. */
static void draw_mark(gfx_context_t *gfx, int16_t y, size_t index, bool selected)
{
    static const char k_glyph[PURRGO_MS_LAYER_COUNT] = { ' ', '=', '~', 'o', '#' };
    const purrgo_ms_layer_t layer = purrgo_map_search_layer(index);

    if (layer == PURRGO_MS_LAYER_POI) {
        /* Иконки цветные: на выделенной (чёрной) строке под ней светлая подложка. */
        if (selected) {
            gfx_set_color(gfx, BLACK, WHITE);
            gfx_fill_rect(gfx, 1, y, 13, ROW_H);
        }
        gfx_draw_icon_11x11(gfx, ICON_CX, (int16_t)(y + 6), purrgo_poi_icons[purrgo_map_search_type(index)]);
    } else {
        char g[2] = { k_glyph[layer], '\0' };

        gfx_set_color(gfx, selected ? WHITE : BLACK, selected ? BLACK : WHITE);
        gfx_draw_string(gfx, 4, (int16_t)(y + 2), g);
    }
}

static void draw_row(gfx_context_t *gfx, int shown, int index, bool selected)
{
    char name[NAME_CHARS + 8];
    char dist[16];
    char right[24];
    char sig[NAME_CHARS + 8 + 24 + 8];
    const int16_t y = (int16_t)(LIST_Y0 + shown * ROW_H);
    const size_t i = (size_t)index;

    /* Название уже в PurrGO-256 (так оно хранится в базах .db): перекодировать нельзя. */
    fit_name(purrgo_map_search_name(i), name, sizeof name);
    purrgo_wpt_format_distance(dist, sizeof dist, purrgo_map_search_distance_m(i));
    purrgo_snprintf(right, sizeof right, "%s %s", dist, purrgo_wpt_cardinal(purrgo_map_search_azimuth_deg(i)));

    gfx_set_color(gfx, selected ? WHITE : BLACK, selected ? BLACK : WHITE);
    gfx_fill_rect(gfx, 0, y, PURRGO_HW_DISPLAY_WIDTH_PX, ROW_H);
    draw_mark(gfx, y, i, selected);

    gfx_set_color(gfx, selected ? WHITE : BLACK, selected ? BLACK : WHITE);
    gfx_draw_string(gfx, NAME_X, (int16_t)(y + 2), name);
    gfx_draw_string(gfx, (int16_t)(PURRGO_HW_DISPLAY_WIDTH_PX - RIGHT_MARGIN - 6 * (int16_t)strlen(right)),
                    (int16_t)(y + 2), right);

    purrgo_snprintf(sig, sizeof sig, "%u|%u|%s|%s", (unsigned)purrgo_map_search_layer(i),
                    (unsigned)purrgo_map_search_type(i), name, right);
    ui_rows_update(&s_rows, shown, selected, sig);
}

static const char *origin_text(purrgo_poi_origin_t origin)
{
    switch (origin) {
        case PURRGO_POI_ORIGIN_GNSS: return "GNSS";
        case PURRGO_POI_ORIGIN_LAST: return "LAST FIX";
        default:                     return "MAP";
    }
}

static void draw_status(gfx_context_t *gfx, int cursor, bool list_focus)
{
    char text[32];
    const size_t count = purrgo_map_search_count();
    const purrgo_ms_state_t state = purrgo_map_search_state();

    if (state == PURRGO_MS_RUNNING) {
        purrgo_snprintf(text, sizeof text, "Searching...");
    } else if (state == PURRGO_MS_ERROR) {
        purrgo_snprintf(text, sizeof text, "Search error: see log");
    } else if (count > 0u) {
        if (list_focus) {
            purrgo_snprintf(text, sizeof text, "%d/%u %s", cursor + 1, (unsigned)count,
                            origin_text(purrgo_map_search_origin()));
        } else {
            purrgo_snprintf(text, sizeof text, "%u found, from %s", (unsigned)count,
                            origin_text(purrgo_map_search_origin()));
        }
    } else {
        switch (purrgo_map_search_kind()) {
            case PURRGO_MS_KIND_EMPTY: purrgo_snprintf(text, sizeof text, "Type name or category"); break;
            case PURRGO_MS_KIND_SHORT: purrgo_snprintf(text, sizeof text, "Type 2+ letters"); break;
            default:                   purrgo_snprintf(text, sizeof text, "Nothing found"); break;
        }
    }

    gfx_set_color(gfx, BLACK, WHITE);
    gfx_draw_string(gfx, 4, (int16_t)(STATUS_Y + 2), text);

    const uint32_t sig = ui_rows_signature(false, text);

    if (s_status_valid && sig != s_status_sig) {
        display_refresh_region(0, STATUS_Y, PURRGO_HW_DISPLAY_WIDTH_PX, ROW_H);
    }
    s_status_sig = sig;
    s_status_valid = true;
}

static void draw_hints(gfx_context_t *gfx, bool list_focus)
{
    gfx_set_color(gfx, BLACK, WHITE);
    gfx_fill_rect(gfx, 0, HINT_Y1, PURRGO_HW_DISPLAY_WIDTH_PX, HINT_H);
    if (list_focus) {
        gfx_draw_string(gfx, 2, HINT_Y1, "K1\x18 K2\x19 K3 MAP K4 KEYS");
        gfx_draw_string(gfx, 2, HINT_Y2, "hold K3 TARGET K4 ESC");
    } else {
        gfx_draw_string(gfx, 2, HINT_Y1, "K1\x1b K2\x1a K3 PRESS K4\x19");
        gfx_draw_string(gfx, 2, HINT_Y2, "hold K1 BS K3 LIST K4 ESC");
    }
    if (s_last_focus >= 0 && s_last_focus != (int)list_focus) {
        display_refresh_region(0, HINT_Y1, PURRGO_HW_DISPLAY_WIDTH_PX, HINT_H);
    }
    s_last_focus = (int)list_focus;
}

void ui_render_search(gfx_context_t *gfx)
{
    const bool list_focus = purrgo_search_screen_list_focus();
    const int cursor = purrgo_search_screen_cursor();
    const int top = purrgo_search_screen_top();
    const int count = (int)purrgo_map_search_count();

    /* Клавиатура очищает экран и оставляет свободную область пустой. */
    ui_render_keyboard(gfx, purrgo_search_screen_keyboard());

    draw_status(gfx, cursor, list_focus);

    ui_rows_begin(&s_rows, LIST_Y0, ROW_H);

    int shown = 0;

    for (int i = top; i < count && shown < PURRGO_SEARCH_VISIBLE_ROWS; i++, shown++) {
        draw_row(gfx, shown, i, list_focus && i == cursor);
    }
    ui_rows_end(&s_rows, shown);

    draw_hints(gfx, list_focus);
}
