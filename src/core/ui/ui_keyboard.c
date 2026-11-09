#include "ui_keyboard.h"

#include <string.h>

#include "purrgo/display_hal.h"
#include "purrgo/font5x7.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_text.h"
#include "purrgo/hardware_config.h"

#define SCREEN_W PURRGO_HW_DISPLAY_WIDTH_PX
#define SCREEN_H PURRGO_HW_DISPLAY_HEIGHT_PX

#define STATUS_Y     2
#define STATUS_H     12
#define INPUT_Y      14
#define INPUT_H      16
#define INPUT_X      2
#define INPUT_TEXT_X 5
#define INPUT_TEXT_Y 18
/* Символов в поле, включая место под курсор (внутренняя ширина (176 - 2 * 2 - 2 * 3) / 6). */
#define INPUT_CHARS  27

#define GRID_Y   UI_KEYBOARD_GRID_TOP_PX
#define HINT_Y1  244
#define HINT_Y2  254

#define CHAR_W 6   /* ячейка символа 6x8, как в gfx_text.c */

/* Состояние прошлого кадра для частичного обновления. */
typedef struct {
    bool     valid;
    uint8_t  layout;
    uint8_t  shift;
    uint8_t  row;
    uint8_t  col;
    uint32_t text_hash;
    uint32_t title_hash;
} kbd_frame_t;

static kbd_frame_t s_frame;

void ui_keyboard_reset(void)
{
    memset(&s_frame, 0, sizeof(s_frame));
}

static uint32_t hash_str(const char *s)
{
    uint32_t h = 2166136261u;

    for (; *s != '\0'; s++) {
        h = (h ^ (uint8_t)*s) * 16777619u;
    }
    return h;
}

/* Заливка прямоугольника цветом: gfx_fill_rect заполняет цветом фона. */
static void fill(gfx_context_t *gfx, int x, int y, int w, int h, gfx_color_t color)
{
    gfx_color_t fg;
    gfx_color_t bg;

    gfx_get_color(gfx, &fg, &bg);
    gfx_set_color(gfx, color, color);
    gfx_fill_rect(gfx, (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h);
    gfx_set_color(gfx, fg, bg);
}

/* Глиф стандартного шрифта 5x7 (ячейка 6x8) цветом ink на фоне bg. */
static void draw_glyph(gfx_context_t *gfx, int x, int y, uint8_t code,
                       gfx_color_t ink, gfx_color_t bg)
{
    gfx_color_t fg;
    gfx_color_t old_bg;

    gfx_get_color(gfx, &fg, &old_bg);
    gfx_set_color(gfx, ink, bg);
    gfx_draw_char(gfx, (int16_t)x, (int16_t)y, (char)code);
    gfx_set_color(gfx, fg, old_bg);
}

/* Строка шрифтом 5x7 по центру клавиши. */
static void draw_label(gfx_context_t *gfx, int x, int y, int w, const char *s,
                       gfx_color_t ink, gfx_color_t bg)
{
    const int tw = (int)strlen(s) * CHAR_W - 1;

    for (int cx = x + (w - tw) / 2; *s != '\0'; s++, cx += CHAR_W) {
        draw_glyph(gfx, cx, y, (uint8_t)*s, ink, bg);
    }
}

static const char *layout_name(uint8_t layout)
{
    switch (layout) {
    case PURRGO_KBD_LAYOUT_RU:  return "RU";
    case PURRGO_KBD_LAYOUT_SYM: return "#+=";
    default:                    return "EN";
    }
}

/* Подпись Shift в строке состояния: abc / Abc / ABC (в RU - кириллицей). */
static void shift_label(const purrgo_keyboard_t *kb, char out[4])
{
    static const char en[3][4] = { "abc", "Abc", "ABC" };
    /* а б в = A0 A1 A2; А б в = 80 A1 A2; А Б В = 80 81 82 */
    static const char ru[3][4] = { "\xA0\xA1\xA2", "\x80\xA1\xA2", "\x80\x81\x82" };
    const unsigned s = (kb->shift <= PURRGO_KBD_SHIFT_LOCK) ? kb->shift : 0u;

    if (kb->layout == PURRGO_KBD_LAYOUT_SYM) {
        out[0] = '\0';
        return;
    }
    memcpy(out, (kb->alpha == PURRGO_KBD_LAYOUT_RU) ? ru[s] : en[s], 4);
}

static void draw_status(gfx_context_t *gfx, const purrgo_keyboard_t *kb)
{
    char shift[4];
    char right[16];
    size_t n = 0;

    fill(gfx, 0, 0, SCREEN_W, INPUT_Y, WHITE);

    gfx_set_color(gfx, BLACK, WHITE);
    gfx_draw_string(gfx, INPUT_X + 2, STATUS_Y, kb->title);

    shift_label(kb, shift);
    const char *name = layout_name(kb->layout);

    memcpy(right, name, strlen(name));
    n = strlen(name);
    if (shift[0] != '\0') {
        right[n++] = ' ';
        memcpy(right + n, shift, 3);
        n += 3;
    }
    right[n] = '\0';

    gfx_draw_string(gfx, (int16_t)(SCREEN_W - INPUT_X - 2 - (int)n * CHAR_W), STATUS_Y, right);
}

static void draw_input(gfx_context_t *gfx, const purrgo_keyboard_t *kb)
{
    char buf[INPUT_CHARS + 1];
    const int len = kb->len;
    int shown = len;
    int x;

    fill(gfx, 0, INPUT_Y, SCREEN_W, INPUT_H + 2, WHITE);
    gfx_set_color(gfx, BLACK, WHITE);
    gfx_draw_rect(gfx, INPUT_X, INPUT_Y, (int16_t)(SCREEN_W - 2 * INPUT_X), INPUT_H);

    /* Влезает INPUT_CHARS - 1 символов и курсор; длиннее - хвост строки с маркой '<'. */
    const int room = INPUT_CHARS - 1;

    if (len > room) {
        buf[0] = '<';
        memcpy(buf + 1, kb->text + (len - (room - 1)), (size_t)(room - 1));
        shown = room;
    } else {
        memcpy(buf, kb->text, (size_t)len);
    }
    buf[shown] = '\0';

    gfx_draw_string(gfx, INPUT_TEXT_X, INPUT_TEXT_Y, buf);

    /* Курсор ввода: полоса под местом следующего символа. */
    x = INPUT_TEXT_X + shown * CHAR_W;
    fill(gfx, x, INPUT_TEXT_Y + 9, 5, 2, BLACK);
}

/* Содержимое клавиши внутри рамки (x, y - линия слева/сверху, w - шаг до линии справа). */
static void draw_key_face(gfx_context_t *gfx, const purrgo_keyboard_t *kb,
                          const purrgo_kbd_key_t *key, int x, int y, int w,
                          gfx_color_t ink, gfx_color_t bg)
{
    const int gy = y + (PURRGO_KBD_ROW_PITCH - 8) / 2 + 1;   /* глиф 5x7 (ячейка 6x8) по центру */
    const int gx = x + (w - 5) / 2 + 1;

    switch (key->kind) {
    case PURRGO_KBD_KEY_LETTER:
    case PURRGO_KBD_KEY_CHAR:
        draw_glyph(gfx, gx, gy, purrgo_kbd_key_char(kb, key), ink, bg);
        break;

    case PURRGO_KBD_KEY_SHIFT:
        /* выключен - стрелка вверх, один раз и Caps - закрашенный треугольник; Caps - полоса снизу */
        draw_glyph(gfx, gx, gy - (kb->shift == PURRGO_KBD_SHIFT_LOCK ? 1 : 0),
                   (kb->shift == PURRGO_KBD_SHIFT_OFF) ? 0x18u : 0x1Eu, ink, bg);
        if (kb->shift == PURRGO_KBD_SHIFT_LOCK) {
            fill(gfx, gx, y + PURRGO_KBD_ROW_PITCH - 3, 5, 1, ink);
        }
        break;

    case PURRGO_KBD_KEY_BACKSPACE:
        draw_glyph(gfx, gx, gy, 0x1Bu, ink, bg);
        break;

    case PURRGO_KBD_KEY_LANG:
        draw_label(gfx, x + 1, gy, w, (kb->alpha == PURRGO_KBD_LAYOUT_RU) ? "RU" : "EN", ink, bg);
        break;

    case PURRGO_KBD_KEY_SYM:
        draw_label(gfx, x + 1, gy, w, (kb->layout == PURRGO_KBD_LAYOUT_SYM) ? "ABC" : "#+=", ink, bg);
        break;

    case PURRGO_KBD_KEY_SPACE:
        draw_label(gfx, x + 1, gy, w, "SPACE", ink, bg);
        break;

    case PURRGO_KBD_KEY_OK:
        draw_label(gfx, x + 1, gy, w, "OK", ink, bg);
        break;

    default:
        break;
    }
}

/*
 * Одна клавиша. Рамка рисуется от линии x до линии x + w включительно, поэтому
 * соседние клавиши (по горизонтали и вертикали) делят одну общую линию, а ряды
 * не сдвинуты друг относительно друга. Клавиша под курсором закрашивается
 * внутри рамки (инверсия).
 */
static void draw_key(gfx_context_t *gfx, const purrgo_keyboard_t *kb, int row, int col)
{
    const purrgo_kbd_layout_t layout = (purrgo_kbd_layout_t)kb->layout;
    const purrgo_kbd_key_t *key = purrgo_kbd_key(layout, row, col);

    if (key == NULL) {
        return;
    }

    const int x = purrgo_kbd_key_x(layout, row, col);
    const int y = GRID_Y + row * PURRGO_KBD_ROW_PITCH;
    const int w = purrgo_kbd_key_w(layout, row, col);
    const bool cursor = (row == kb->row && col == kb->col);

    fill(gfx, x + 1, y + 1, w - 1, PURRGO_KBD_ROW_PITCH - 1, cursor ? BLACK : WHITE);
    gfx_set_color(gfx, BLACK, WHITE);
    gfx_draw_rect(gfx, (int16_t)x, (int16_t)y, (int16_t)(w + 1), (int16_t)(PURRGO_KBD_ROW_PITCH + 1));
    draw_key_face(gfx, kb, key, x, y, w, cursor ? WHITE : BLACK, cursor ? BLACK : WHITE);
}

static void draw_grid(gfx_context_t *gfx, const purrgo_keyboard_t *kb)
{
    const purrgo_kbd_layout_t layout = (purrgo_kbd_layout_t)kb->layout;

    for (int row = 0; row < PURRGO_KBD_ROWS; row++) {
        const int n = purrgo_kbd_row_len(layout, row);

        for (int col = 0; col < n; col++) {
            draw_key(gfx, kb, row, col);
        }
    }
}

/*
 * Подсказки кнопок (не более 29 символов в строке). Стрелки - глифы шрифта:
 * 0x1B влево, 0x1A вправо, 0x19 вниз.
 */
static void draw_hints(gfx_context_t *gfx)
{
    gfx_set_color(gfx, BLACK, WHITE);
    gfx_draw_string(gfx, 2, HINT_Y1, "K1\x1b K2\x1a K3 PRESS K4\x19");
    gfx_draw_string(gfx, 2, HINT_Y2, "hold K1 BS K2 UP K3 OK K4 ESC");
}

static void mark(int x, int y, int w, int h)
{
    display_refresh_region((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h);
}

/* Клавиша (row, col) вместе с рамкой: прямоугольник для частичного обновления. */
static void mark_key(uint8_t layout, int row, int col)
{
    const purrgo_kbd_key_t *key = purrgo_kbd_key((purrgo_kbd_layout_t)layout, row, col);

    if (key != NULL) {
        mark(purrgo_kbd_key_x((purrgo_kbd_layout_t)layout, row, col),
             GRID_Y + row * PURRGO_KBD_ROW_PITCH,
             purrgo_kbd_key_w((purrgo_kbd_layout_t)layout, row, col) + 1, PURRGO_KBD_ROW_PITCH + 1);
    }
}

void ui_render_keyboard(gfx_context_t *gfx, const purrgo_keyboard_t *kb)
{
    if (gfx == NULL || kb == NULL) {
        return;
    }

    gfx_reset_clip(gfx);
    gfx_set_color(gfx, BLACK, WHITE);
    gfx_clear(gfx);

    draw_status(gfx, kb);
    draw_input(gfx, kb);
    draw_grid(gfx, kb);
    draw_hints(gfx);

    const uint32_t text_hash = hash_str(kb->text);
    const uint32_t title_hash = hash_str(kb->title);

    if (s_frame.valid) {
        const bool layout_changed = (s_frame.layout != kb->layout) || (s_frame.shift != kb->shift);

        if (layout_changed) {
            /* Меняются подписи всех клавиш и строка состояния. */
            mark(0, 0, SCREEN_W, STATUS_H);
            mark(0, GRID_Y, SCREEN_W, PURRGO_KBD_ROWS * PURRGO_KBD_ROW_PITCH + 1);
        } else {
            if (title_hash != s_frame.title_hash) {
                mark(0, 0, SCREEN_W, STATUS_H);
            }
            if (s_frame.row != kb->row || s_frame.col != kb->col) {
                mark_key(kb->layout, s_frame.row, s_frame.col);
                mark_key(kb->layout, kb->row, kb->col);
            }
        }
        if (text_hash != s_frame.text_hash) {
            mark(0, INPUT_Y, SCREEN_W, INPUT_H + 2);
        }
    }

    s_frame.valid = true;
    s_frame.layout = kb->layout;
    s_frame.shift = kb->shift;
    s_frame.row = kb->row;
    s_frame.col = kb->col;
    s_frame.text_hash = text_hash;
    s_frame.title_hash = title_hash;
}
