#include "ui_poi_results.h"
#include "purrgo/gfx_icon.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_text.h"
#include "purrgo/hardware_config.h"
#include "purrgo/menu_controller.h"
#include "purrgo/pgo_text.h"
#include "purrgo/poi_search.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/purrgo_poi_icons.h"
#include "purrgo/waypoint.h"
#include "ui_rows.h"
#include <string.h>

#define LIST_X0    10
#define LIST_Y0    25
#define ROW_H      12
#define ICON_CX    7                 /* центр иконки 11x11 по X */
#define NAME_X     14
#define NAME_CHARS 17                /* название: не больше 17 символов, длиннее - 15 и ".." */
#define RIGHT_MARGIN 3

/* Состояние, нарисованное в прошлый проход: смена (поиск -> список) обновляет экран целиком. */
#define STATE_NONE (-1)

static ui_rows_t s_rows;
static int s_last_state = STATE_NONE;

void ui_poi_results_reset(void)
{
    ui_rows_reset(&s_rows);
    s_last_state = STATE_NONE;
}

static const char* origin_text(purrgo_poi_origin_t origin)
{
    switch (origin) {
        case PURRGO_POI_ORIGIN_GNSS: return "GNSS";
        case PURRGO_POI_ORIGIN_LAST: return "LAST FIX";
        default:                     return "MAP CENTER";
    }
}

/* Название не длиннее NAME_CHARS символов. Пустое имя - "(no name)". */
static void fit_name(const char* name, char* out, size_t size)
{
    const size_t len = strlen(name);

    if (len == 0u) {
        purrgo_snprintf(out, size, "(no name)");
    }
    else if (len <= NAME_CHARS) {
        purrgo_snprintf(out, size, "%s", name);
    }
    else {
        purrgo_snprintf(out, size, "%.*s..", NAME_CHARS - 2, name);
    }
}

static void draw_row(gfx_context_t* gfx, int shown, int index, bool selected)
{
    char name[NAME_CHARS + 8];
    char dist[16];
    char right[24];
    const int16_t y = (int16_t)(LIST_Y0 + shown * ROW_H);

    /* Название уже в PurrGO-256 (так оно хранится в pois.db): перекодировать нельзя. */
    fit_name(purrgo_poi_search_name((size_t)index), name, sizeof name);
    purrgo_wpt_format_distance(dist, sizeof dist, purrgo_poi_search_distance_m((size_t)index));
    purrgo_snprintf(right, sizeof right, "%s %s", dist,
                    purrgo_wpt_cardinal(purrgo_poi_search_azimuth_deg((size_t)index)));

    gfx_set_color(gfx, selected ? WHITE : BLACK, selected ? BLACK : WHITE);
    gfx_fill_rect(gfx, 0, y, PURRGO_HW_DISPLAY_WIDTH_PX, ROW_H);

    /* Иконки цветные: на выделенной (чёрной) строке под ней светлая подложка. */
    if (selected) {
        gfx_set_color(gfx, BLACK, WHITE);
        gfx_fill_rect(gfx, 1, y, 13, ROW_H);
    }
    gfx_draw_icon_11x11(gfx, ICON_CX, (int16_t)(y + 6),
                        purrgo_poi_icons[purrgo_poi_search_type((size_t)index)]);

    gfx_set_color(gfx, selected ? WHITE : BLACK, selected ? BLACK : WHITE);
    gfx_draw_string(gfx, NAME_X, (int16_t)(y + 2), name);

    const int16_t rx = (int16_t)(PURRGO_HW_DISPLAY_WIDTH_PX - RIGHT_MARGIN - 6 * (int16_t)strlen(right));

    gfx_draw_string(gfx, rx, (int16_t)(y + 2), right);

    /* Подпись строки для частичного обновления: всё, что видно в строке, плюс выделение. */
    char sig[NAME_CHARS + 8 + 24 + 4];

    purrgo_snprintf(sig, sizeof sig, "%u|%s|%s", (unsigned)purrgo_poi_search_type((size_t)index),
                    name, right);
    ui_rows_update(&s_rows, shown, selected, sig);
}

void ui_render_poi_results(gfx_context_t* gfx)
{
    char title[48];

    gfx_set_color(gfx, BLACK, WHITE);
    gfx_clear(gfx);
    gfx_set_color(gfx, BLACK, WHITE);

    purrgo_snprintf(title, sizeof title, "%s",
                    purrgo_poi_category_title(purrgo_poi_search_category()));
    purrgo_utf8_to_pgo_inplace(title);
    gfx_draw_string(gfx, LIST_X0, 10, title);

    const purrgo_poi_search_state_t state = purrgo_poi_search_state();

    ui_rows_begin(&s_rows, LIST_Y0, ROW_H);

    int shown = 0;
    char info[40];

    if (state == PURRGO_POI_SEARCH_DONE && purrgo_poi_search_count() > 0u) {
        const int count   = menu_app_get_poi_result_count();
        const int cursor  = menu_app_get_poi_result_cursor();
        const int top     = menu_app_get_poi_result_top();
        const int visible = menu_app_get_poi_result_visible();

        for (int i = top; i < count && shown < visible; i++, shown++) {
            draw_row(gfx, shown, i, i == cursor);
        }

        gfx_set_color(gfx, BLACK, WHITE);
        purrgo_snprintf(info, sizeof info, "%d/%d %s r=%u km", cursor + 1, count,
                        origin_text(purrgo_poi_search_origin()),
                        (unsigned)purrgo_poi_search_radius_km());
        gfx_draw_string(gfx, LIST_X0, (int16_t)(LIST_Y0 + shown * ROW_H + 6), info);
    }
    else {
        const char* line1 = "";
        const char* line2 = "";

        if (state == PURRGO_POI_SEARCH_RUNNING) {
            line1 = "Searching...";
        }
        else if (state == PURRGO_POI_SEARCH_DONE) {
            line1 = "Nothing found";
            purrgo_snprintf(info, sizeof info, "within %u km",
                            (unsigned)PURRGO_POI_SEARCH_MAX_RADIUS_KM);
            line2 = info;
        }
        else {
            line1 = "Search failed";
            line2 = "Check pois.idx in map dir";
        }

        gfx_draw_string(gfx, LIST_X0, LIST_Y0, line1);
        gfx_draw_string(gfx, LIST_X0, (int16_t)(LIST_Y0 + ROW_H), line2);
        if (state == PURRGO_POI_SEARCH_RUNNING || state == PURRGO_POI_SEARCH_DONE) {
            char from[32];

            purrgo_snprintf(from, sizeof from, "From: %s", origin_text(purrgo_poi_search_origin()));
            gfx_draw_string(gfx, LIST_X0, (int16_t)(LIST_Y0 + 2 * ROW_H), from);
        }
    }

    ui_rows_end(&s_rows, shown);

    /*
     * Смена вида (поиск -> список или сообщение): текст вне строк списка тоже изменился, поэтому
     * обновляется весь экран (в 1-bit режиме это частичное обновление без мигания). Движение
     * курсора и прокрутка списка обновляют только изменившиеся строки (ui_rows.h).
     */
    if (s_last_state != (int)state) {
        s_last_state = (int)state;
        display_refresh_region(0, 0, PURRGO_HW_DISPLAY_WIDTH_PX, PURRGO_HW_DISPLAY_HEIGHT_PX);
    }
}
