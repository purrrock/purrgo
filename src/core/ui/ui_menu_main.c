#include "ui_menu_main.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_text.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/config.h"
#include "purrgo/menu_controller.h"
#include "purrgo/fs_hal.h"
#include "purrgo/pgo_text.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/display_hal.h"
#include "purrgo/hardware_config.h"
#include "ui_rows.h"

static ui_rows_t s_rows;

/* Первая строка списка и шаг строк, px. */
#define MENU_ROW_Y0    25
#define MENU_ROW_STEP  15

void ui_menu_main_reset(void) {
    ui_rows_reset(&s_rows);
}

void ui_menu_config_reset(void) {
    ui_rows_reset(&s_rows);
}

void ui_menu_poi_category_reset(void) {
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
        (int16_t)(MENU_ROW_Y0 + row * MENU_ROW_STEP),
        text
    );

    ui_rows_update(&s_rows, row, cursor == row, text);
}

typedef void (*item_text_fn)(int index, char* buf, size_t size);

/*
 * Общая отрисовка списка пунктов (главное меню и CONFIG): пункты берутся из таблиц
 * в menu_main.c и menu_config.c, порядок строк и их число определяются ими, здесь
 * ничего не нужно менять при добавлении пункта.
 */
static void render_list(gfx_context_t* gfx, const char* title, int cursor, int count,
                        item_text_fn item_text)
{
    char buf[PURRGO_FS_MAX_PATH + 64];

    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);
    gfx_set_color(gfx, 0, 3);

    gfx_draw_string(gfx, 10, 10, title);

    ui_rows_begin(&s_rows, MENU_ROW_Y0, MENU_ROW_STEP);

    for (int row = 0; row < count; row++) {
        item_text(row, buf, sizeof(buf));
        purrgo_utf8_to_pgo_inplace(buf);  /* имена с SD: UTF-8 -> PurrGO-256 */
        draw_item(gfx, cursor, row, buf);
    }

    ui_rows_end(&s_rows, count);
}

void ui_render_menu_main(gfx_context_t* gfx)
{
    render_list(gfx, "==== MENU ====", menu_app_get_main_cursor(),
                menu_app_get_main_count(), menu_app_get_main_item_text);
}

void ui_render_menu_config(gfx_context_t* gfx)
{
    render_list(gfx, "==== CONFIG ====", menu_app_get_config_cursor(),
                menu_app_get_config_count(), menu_app_get_config_item_text);
}

void ui_render_menu_poi_category(gfx_context_t* gfx)
{
    render_list(gfx, "==== SEARCH ====", menu_app_get_poi_category_cursor(),
                menu_app_get_poi_category_count(), menu_app_get_poi_category_text);
}
