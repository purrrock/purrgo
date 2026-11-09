#include "ui_book.h"
#include "ui_keyboard.h"
#include "ui_rows.h"

#include "purrgo/battery.h"
#include "purrgo/book_reader.h"
#include "purrgo/display_hal.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_text.h"
#include "purrgo/hardware_config.h"
#include "purrgo/pgb.h"
#include "purrgo/pgo_text.h"
#include "purrgo/purrgo_format.h"

#include <string.h>

/* Ячейка символа 6x8, как в gfx_text.c и PGB-format.md; шаг строк книги и положение текста - в pgb.h. */
#define CELL_W 6
#define CELL_H 8
#define LINE_PITCH ((int)PGB_LINE_PITCH_PX)
#define TEXT_TOP   ((int)PGB_TEXT_TOP_PX)

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

/*
 * Текст страницы: строки через LF, ячейка 6x8 px, шаг строк 10 px, первая строка на TEXT_TOP.
 * Байты [hl_start, hl_start + hl_len) выводятся инверсией (найденный текст); 1 px над ячейкой
 * тоже закрашивается, чтобы выделение не выглядело обрезанным сверху.
 */
static void draw_page_text(gfx_context_t* gfx, const uint8_t* text, size_t len,
                           uint32_t hl_start, uint32_t hl_len)
{
    int col = 0;
    int row = 0;

    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\n') {
            row++;
            col = 0;
            continue;
        }
        const int16_t x = (int16_t)(1 + col * CELL_W);
        const int16_t y = (int16_t)(TEXT_TOP + row * LINE_PITCH);
        const bool hl = (hl_len > 0u && i >= hl_start && i - hl_start < hl_len);

        if (hl) {
            gfx_set_color(gfx, WHITE, BLACK);
            gfx_fill_rect(gfx, x, (int16_t)(y - 1), CELL_W, 1);
            gfx_draw_char(gfx, x, y, (char)text[i]);
            gfx_set_color(gfx, BLACK, WHITE);
        } else {
            gfx_draw_char(gfx, x, y, (char)text[i]);
        }
        col++;
    }
}

/*
 * Страница-картинка: 1 bit на пиксель (1 - чёрный), по центру окна 176x256 под строкой
 * статуса. Экран уже очищен в белый, поэтому рисуются только чёрные пиксели.
 */
static void draw_page_image(gfx_context_t* gfx, const uint8_t* bits, uint16_t w, uint16_t h)
{
    const int x0 = ((int)PGB_IMG_WINDOW_W - (int)w) / 2;
    const int y0 = (int)PGB_STATUS_H_PX + ((int)PGB_IMG_WINDOW_H - (int)h) / 2;
    const uint32_t row_bytes = PGB_IMG_ROW_BYTES(w);

    gfx_set_color(gfx, BLACK, WHITE);
    for (uint16_t y = 0; y < h; y++) {
        const uint8_t* row = bits + (size_t)y * row_bytes;
        for (uint16_t bx = 0; bx < row_bytes; bx++) {
            uint8_t b = row[bx];
            if (b == 0u) {
                continue;
            }
            for (int bit = 0; bit < 8; bit++) {
                if (b & (0x80u >> bit)) {
                    gfx_draw_pixel(gfx, (int16_t)(x0 + bx * 8 + bit), (int16_t)(y0 + y));
                }
            }
        }
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

/* Хэш всего, что рисуется ниже строки статуса (текст, картинка, ошибки, подсказка перехода). */
static uint32_t body_hash(const purrgo_book_view_t* v)
{
    uint32_t h = 2166136261u;
    const uint8_t flags[4] = { (uint8_t)v->open, (uint8_t)v->error, (uint8_t)v->page_error,
                               (uint8_t)v->jump };
    const uint32_t hl[2] = { v->hl_start, v->hl_len };

    h = fnv1a(h, flags, sizeof(flags));
    h = fnv1a(h, hl, sizeof(hl));   /* подсветка найденного: сама по себе меняет экран */
    if (!v->open) {
        h = fnv1a(h, v->file, strlen(v->file));
    } else if (v->text != NULL) {
        h = fnv1a(h, v->text, v->text_len);
    } else if (v->image != NULL) {
        const uint16_t dim[2] = { v->image_w, v->image_h };
        h = fnv1a(h, dim, sizeof(dim));
        h = fnv1a(h, v->image, (size_t)PGB_IMG_ROW_BYTES(v->image_w) * v->image_h);
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
    } else if (v.find == PURRGO_BOOK_FIND_RUNNING) {
        purrgo_snprintf(status, sizeof(status), "SEARCH %u%%  4:STOP", (unsigned)v.find_percent);
    } else if (v.find == PURRGO_BOOK_FIND_FOUND) {
        /* Страница/всего слева, подсказка кнопок справа: в режиме поиска K1/K2 листают совпадения. */
        char left[24];
        const char* keys = "3:NEW 4:END";
        purrgo_snprintf(left, sizeof(left), "FIND %u/%u", (unsigned)(v.page + 1u), (unsigned)v.page_count);
        const int pad = (int)PGB_SCREEN_COLS - (int)strlen(left) - (int)strlen(keys);
        purrgo_snprintf(status, sizeof(status), "%s%*s%s", left, (pad > 1) ? pad : 1, "", keys);
    } else if (v.find == PURRGO_BOOK_FIND_NOT_FOUND) {
        purrgo_snprintf(status, sizeof(status), "NOT FOUND  3:EDIT 4:OK");
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
            draw_page_text(gfx, v.text, v.text_len, v.hl_start, v.hl_len);
        } else if (v.image != NULL) {
            draw_page_image(gfx, v.image, v.image_w, v.image_h);
        } else {
            purrgo_snprintf(buf, sizeof(buf), "PAGE %u: %s", (unsigned)(v.page + 1u),
                            pgb_status_text(v.page_error));
            draw_message(gfx, 1, buf);
        }
        if (v.jump) {
            /* Экран занят текстом или картинкой, поэтому подсказка поверх нижней строки страницы. */
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

    /* Страница меняется по всей площади: обновляется весь экран (1 bit - частичное обновление). */
    display_refresh_region(0, 0, PURRGO_HW_DISPLAY_WIDTH_PX, PURRGO_HW_DISPLAY_HEIGHT_PX);
}

/* ---- Ввод запроса поиска ------------------------------------------------------- */

void ui_book_search_reset(void)
{
    ui_keyboard_reset();
}

void ui_render_book_search(gfx_context_t* gfx)
{
    ui_render_keyboard(gfx, purrgo_book_reader_get_keyboard());

    /*
     * Свободная область между полем ввода и клавишами: неизменный текст, поэтому его область
     * не регистрируется для частичного обновления (первый кадр экрана обновляется целиком).
     * \x85 = Е, \xF0 = Ё в PurrGO-256.
     */
    gfx_set_color(gfx, BLACK, WHITE);
    gfx_draw_string(gfx, 4, (int16_t)(UI_KEYBOARD_FREE_TOP_PX + 8), "Search from this page,");
    gfx_draw_string(gfx, 4, (int16_t)(UI_KEYBOARD_FREE_TOP_PX + 20), "then from the book start.");
    gfx_draw_string(gfx, 4, (int16_t)(UI_KEYBOARD_FREE_TOP_PX + 40), "Case is ignored, \x85 = \xF0");
}
