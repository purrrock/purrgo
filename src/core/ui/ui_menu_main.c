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
#define CONFIG_ROW_Y0    25
#define CONFIG_ROW_STEP  15

void ui_menu_main_reset(void) {
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

void ui_render_menu_main(gfx_context_t* gfx)
{
    char buf[PURRGO_FS_MAX_PATH + 64];

    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);
    gfx_set_color(gfx, 0, 3);

    const int cursor = menu_app_get_main_cursor();
    const int count  = menu_app_get_main_count();

    gfx_draw_string(gfx, 10, 10, "==== MENU ====");

    /*
     * Пункты берутся из таблицы в menu_main.c: порядок строк и их число
     * определяются ей, здесь ничего не нужно менять при добавлении пункта.
     */
    ui_rows_begin(&s_rows, CONFIG_ROW_Y0, CONFIG_ROW_STEP);

    for (int row = 0; row < count; row++) {
        menu_app_get_main_item_text(row, buf, sizeof(buf));
        purrgo_utf8_to_pgo_inplace(buf);  /* имена с SD: UTF-8 -> PurrGO-256 */
        draw_item(gfx, cursor, row, buf);
    }

    /*
     * HELP: под последней строкой списка.
     */
    gfx_set_color(gfx, 0, 3);

    const int16_t help_y =
        (int16_t)(CONFIG_ROW_Y0 + count * CONFIG_ROW_STEP + 5);

    gfx_draw_string(gfx, 10, help_y, "UP/DN: Select");
    gfx_draw_string(gfx, 10, (int16_t)(help_y + 15), "SEL  : Change");
    gfx_draw_string(gfx, 10, (int16_t)(help_y + 30), "BACK : Save");

    ui_rows_end(&s_rows, count);
}
