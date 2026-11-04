#include "ui_book.h"
#include "ui_rows.h"

#include "purrgo/battery.h"
#include "purrgo/book_reader.h"
#include "purrgo/display_hal.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_text.h"
#include "purrgo/hardware_config.h"
#include "purrgo/pgo_text.h"
#include "purrgo/purrgo_format.h"

#include <string.h>

/* Ячейка символа 6x8, как в gfx_text.c и PGB-format.md. */
#define CELL_W 6
#define CELL_H 8

/* Список книг. */
#define LIST_Y0       25
#define LIST_ROW_H    12
#define LIST_VISIBLE  16
#define LIST_X        4
#define DETAIL_Y      (PURRGO_HW_DISPLAY_HEIGHT_PX - 8 - 2 * LIST_ROW_H)   /* над подсказками кнопок */

static ui_rows_t s_rows;
static int s_top;   /* первая видимая книга */

void ui_book_list_reset(void)
{
    ui_rows_reset(&s_rows);
    s_top = 0;
}

void ui_render_book_list(gfx_context_t* gfx)
{
    char buf[40];
    const int count = purrgo_book_list_count();
    const int cursor = purrgo_book_list_cursor();

    gfx_set_color(gfx, BLACK, WHITE);
    gfx_clear(gfx);
    gfx_draw_string(gfx, 10, 10, "=== BOOKS ===");

    if (cursor < s_top) {
        s_top = cursor;
    }
    if (cursor >= s_top + LIST_VISIBLE) {
        s_top = cursor - LIST_VISIBLE + 1;
    }
    if (s_top < 0) {
        s_top = 0;
    }

    ui_rows_begin(&s_rows, LIST_Y0, LIST_ROW_H);

    int rows = 0;
    for (int i = 0; i < LIST_VISIBLE && s_top + i < count; i++, rows++) {
        const purrgo_book_item_t* it = purrgo_book_list_get(s_top + i);
        const bool selected = (s_top + i == cursor);

        /* "*" - последняя открытая книга, "!" - файл не читается. */
        purrgo_snprintf(buf, sizeof(buf), "%c %s",
                        it->ok ? (purrgo_book_list_is_last(s_top + i) ? '*' : ' ') : '!',
                        it->title);

        gfx_set_color(gfx, selected ? WHITE : BLACK, selected ? BLACK : WHITE);
        gfx_draw_string(gfx, LIST_X, (int16_t)(LIST_Y0 + i * LIST_ROW_H), buf);
        ui_rows_update(&s_rows, i, selected, buf);
    }
    ui_rows_end(&s_rows, rows);

    gfx_set_color(gfx, BLACK, WHITE);
    if (count == 0) {
        gfx_draw_string(gfx, LIST_X, LIST_Y0, "(No books)");
        gfx_draw_string(gfx, LIST_X, (int16_t)(LIST_Y0 + LIST_ROW_H), "Put *.PGB into");
        gfx_draw_string(gfx, LIST_X, (int16_t)(LIST_Y0 + 2 * LIST_ROW_H), "PURRGO/BOOKS");
    } else {
        /* Подробности выбранной книги: автор и число страниц. */
        const purrgo_book_item_t* it = purrgo_book_list_get(cursor);
        gfx_draw_string(gfx, 2, DETAIL_Y, it->ok ? it->author : "(bad file)");
        if (it->ok) {
            purrgo_snprintf(buf, sizeof(buf), "%u pages", (unsigned)it->page_count);
            gfx_draw_string(gfx, 2, (int16_t)(DETAIL_Y + LIST_ROW_H), buf);
        }
        /* Строка с автором/страницами меняется вместе с курсором: обновляется всегда. */
        display_refresh_region(0, DETAIL_Y, PURRGO_HW_DISPLAY_WIDTH_PX, 2 * LIST_ROW_H);
    }
}

/* Строка статуса: инверсная, во всю ширину. */
static void draw_status(gfx_context_t* gfx, const char* text)
{
    gfx_set_color(gfx, WHITE, BLACK);
    gfx_fill_rect(gfx, 0, 0, PURRGO_HW_DISPLAY_WIDTH_PX, CELL_H);
    gfx_draw_string(gfx, 1, 0, text);
    gfx_set_color(gfx, BLACK, WHITE);
}

/* Текст страницы: строки через LF, 6x8 пикселей на символ, начиная под строкой статуса. */
static void draw_page_text(gfx_context_t* gfx, const uint8_t* text, size_t len)
{
    int col = 0;
    int row = 0;

    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\n') {
            row++;
            col = 0;
            continue;
        }
        gfx_draw_char(gfx, (int16_t)(1 + col * CELL_W), (int16_t)(CELL_H + row * CELL_H), (char)text[i]);
        col++;
    }
}

static void draw_message(gfx_context_t* gfx, int row, const char* text)
{
    gfx_draw_string(gfx, 1, (int16_t)(CELL_H + row * CELL_H), text);
}

/* Что нарисовано на экране чтения в прошлый раз. */
static struct {
    bool     valid;
    uint32_t body_hash;    /* всё, кроме строки статуса */
    char     status[48];
} s_read_frame;

void ui_book_read_reset(void)
{
    s_read_frame.valid = false;
}

static uint32_t fnv1a(uint32_t h, const void* data, size_t len)
{
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < len; i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

/* Хэш всего, что рисуется ниже строки статуса (текст, ошибки, подсказка перехода). */
static uint32_t body_hash(const purrgo_book_view_t* v)
{
    uint32_t h = 2166136261u;
    const uint8_t flags[4] = { (uint8_t)v->open, (uint8_t)v->error, (uint8_t)v->page_error,
                               (uint8_t)v->jump };

    h = fnv1a(h, flags, sizeof(flags));
    if (!v->open) {
        h = fnv1a(h, v->file, strlen(v->file));
    } else if (v->text != NULL) {
        h = fnv1a(h, v->text, v->text_len);
    } else {
        h = fnv1a(h, &v->page, sizeof(v->page));   /* текст страницы недоступен: сообщение с номером */
    }
    return h;
}

void ui_render_book_read(gfx_context_t* gfx)
{
    purrgo_book_view_t v;
    char status[sizeof(s_read_frame.status)];
    char buf[48];

    purrgo_book_reader_get_view(&v);

    /* Строка статуса. */
    if (!v.open) {
        purrgo_snprintf(status, sizeof(status), "BOOK ERROR");
    } else if (v.jump) {
        /* Переход по процентам: что получится и как подтвердить. */
        purrgo_snprintf(status, sizeof(status), "GO TO %u%% (p.%u/%u)",
                        (unsigned)v.jump_percent, (unsigned)(v.jump_page + 1u),
                        (unsigned)v.page_count);
    } else {
        /* Слева страница/всего, справа заряд батареи (общий монитор); процент прочитанного не выводится. */
        char left[24];
        const char* bat = purrgo_battery_status_text();
        purrgo_snprintf(left, sizeof(left), "%u/%u", (unsigned)(v.page + 1u), (unsigned)v.page_count);
        const int pad = (int)PGB_SCREEN_COLS - (int)strlen(left) - (int)strlen(bat);
        purrgo_snprintf(status, sizeof(status), "%s%*s%s", left, (pad > 1) ? pad : 1, "", bat);
    }

    const uint32_t hash = body_hash(&v);
    if (s_read_frame.valid && hash == s_read_frame.body_hash) {
        if (strcmp(status, s_read_frame.status) == 0) {
            return;   /* на экране ничего не изменилось: ни рисования, ни обновления дисплея */
        }
        /* Изменилась только строка статуса (заряд, процент перехода): обновляется одна строка. */
        draw_status(gfx, status);
        memcpy(s_read_frame.status, status, sizeof(status));
        display_refresh_region(0, 0, PURRGO_HW_DISPLAY_WIDTH_PX, CELL_H);
        return;
    }

    gfx_set_color(gfx, BLACK, WHITE);
    gfx_clear(gfx);
    draw_status(gfx, status);

    if (!v.open) {
        /* Книга не открылась: причина и подсказка кнопок. */
        purrgo_snprintf(buf, sizeof(buf), "%s", v.file);
        purrgo_utf8_to_pgo_inplace(buf);
        buf[PURRGO_BOOK_AUTHOR_COLS] = '\0';
        draw_message(gfx, 1, buf);
        draw_message(gfx, 3, pgb_status_text(v.error));
        draw_message(gfx, 6, "K3 - book list");
        draw_message(gfx, 7, "K4 - exit");
    } else {
        if (v.text != NULL) {
            draw_page_text(gfx, v.text, v.text_len);
        } else {
            purrgo_snprintf(buf, sizeof(buf), "PAGE %u: %s", (unsigned)(v.page + 1u),
                            pgb_status_text(v.page_error));
            draw_message(gfx, 1, buf);
        }
        if (v.jump) {
            /* Экран занят текстом, поэтому подсказка поверх нижней строки страницы. */
            gfx_set_color(gfx, WHITE, BLACK);
            gfx_fill_rect(gfx, 0, (int16_t)(PURRGO_HW_DISPLAY_HEIGHT_PX - CELL_H),
                          PURRGO_HW_DISPLAY_WIDTH_PX, CELL_H);
            gfx_draw_string(gfx, 1, (int16_t)(PURRGO_HW_DISPLAY_HEIGHT_PX - CELL_H),
                            "K1/K2 -/+  K3 GO  K4 NO");
            gfx_set_color(gfx, BLACK, WHITE);
        }
    }

    s_read_frame.valid = true;
    s_read_frame.body_hash = hash;
    memcpy(s_read_frame.status, status, sizeof(status));

    /* Текст меняется по всей площади: обновляется весь экран (1 bit - частичное обновление). */
    display_refresh_region(0, 0, PURRGO_HW_DISPLAY_WIDTH_PX, PURRGO_HW_DISPLAY_HEIGHT_PX);
}
