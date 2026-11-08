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

/* Глиф 5x8 шрифта в двойном масштабе (10x16 px), прозрачный фон. */
static void draw_glyph2x(gfx_context_t *gfx, int x, int y, uint8_t code, gfx_color_t ink)
{
    const unsigned char *bitmap = font5x7[code];

    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            if ((bitmap[col] >> row) & 1) {
                fill(gfx, x + col * 2, y + row * 2, 2, 2, ink);
            }
        }
    }
}

/* Строка двойным масштабом, шаг 11 px (глиф 10 px и 1 px зазор). */
static int text2x_width(const char *s)
{
    return (int)strlen(s) * 11 - 1;
}

static void draw_text2x(gfx_context_t *gfx, int x, int y, const char *s, gfx_color_t ink)
{
    for (; *s != '\0'; s++, x += 11) {
        draw_glyph2x(gfx, x, y, (uint8_t)*s, ink);
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

/* Подпись служебной клавиши внутри рамки (x, y, w - ширина клавиши без зазора). */
static void draw_key_face(gfx_context_t *gfx, const purrgo_keyboard_t *kb,
                          const purrgo_kbd_key_t *key, int x, int y, int w, gfx_color_t ink)
{
    const int gx = x + (w - 10) / 2;   /* глиф 10 px по центру */
    const int gy = y + (PURRGO_KBD_KEY_H - 16) / 2;   /* глиф 5x8 в двойном масштабе - 16 px */

    switch (key->kind) {
    case PURRGO_KBD_KEY_LETTER:
    case PURRGO_KBD_KEY_CHAR:
        draw_glyph2x(gfx, gx, gy, purrgo_kbd_key_char(kb, key), ink);
        break;

    case PURRGO_KBD_KEY_SHIFT:
        /* выключен - стрелка вверх, один раз и Caps - закрашенный треугольник; Caps - ещё полоса снизу */
        draw_glyph2x(gfx, gx, gy - (kb->shift == PURRGO_KBD_SHIFT_LOCK ? 1 : 0),
                     (kb->shift == PURRGO_KBD_SHIFT_OFF) ? 0x18u : 0x1Eu, ink);
        if (kb->shift == PURRGO_KBD_SHIFT_LOCK) {
            fill(gfx, x + 3, y + PURRGO_KBD_KEY_H - 3, w - 6, 2, ink);
        }
        break;

    case PURRGO_KBD_KEY_BACKSPACE:
        draw_glyph2x(gfx, gx, gy, 0x1Bu, ink);
        break;

    case PURRGO_KBD_KEY_LANG: {
        const char *s = (kb->alpha == PURRGO_KBD_LAYOUT_RU) ? "RU" : "EN";

        draw_text2x(gfx, x + (w - text2x_width(s)) / 2, gy, s, ink);
        break;
    }

    case PURRGO_KBD_KEY_SYM: {
        const char *s = (kb->layout == PURRGO_KBD_LAYOUT_SYM) ? "ABC" : "#+=";
        const int tw = (int)strlen(s) * CHAR_W - 1;
        gfx_color_t fg;
        gfx_color_t bg;

        gfx_get_color(gfx, &fg, &bg);
        gfx_set_color(gfx, ink, (ink == BLACK) ? WHITE : BLACK);
        gfx_draw_string(gfx, (int16_t)(x + (w - tw) / 2), (int16_t)(y + (PURRGO_KBD_KEY_H - 8) / 2), s);
        gfx_set_color(gfx, fg, bg);
        break;
    }

    case PURRGO_KBD_KEY_SPACE:
        /* скобка-пробел: горизонтальная линия с короткими засечками вверх */
        fill(gfx, x + 12, y + 11, w - 24, 2, ink);
        fill(gfx, x + 12, y + 7, 2, 6, ink);
        fill(gfx, x + w - 14, y + 7, 2, 6, ink);
        break;

    case PURRGO_KBD_KEY_OK:
        draw_text2x(gfx, x + (w - text2x_width("OK")) / 2, gy, "OK", ink);
        break;

    default:
        break;
    }
}

/* Одна клавиша (ширина без 1 px зазора): у клавиши под курсором - инверсия. */
static void draw_key(gfx_context_t *gfx, const purrgo_keyboard_t *kb, int row, int col)
{
    const purrgo_kbd_layout_t layout = (purrgo_kbd_layout_t)kb->layout;
    const purrgo_kbd_key_t *key = purrgo_kbd_key(layout, row, col);

    if (key == NULL) {
        return;
    }

    const int x = purrgo_kbd_key_x(layout, row, col);
    const int y = GRID_Y + row * PURRGO_KBD_ROW_PITCH;
    const int w = key->width - 1;
    const bool cursor = (row == kb->row && col == kb->col);
    const gfx_color_t ink = cursor ? WHITE : BLACK;

    /* Буквы, цифры и знаки - без рамки (чище сетка); служебные клавиши обведены. */
    const bool framed = (key->kind != PURRGO_KBD_KEY_LETTER && key->kind != PURRGO_KBD_KEY_CHAR);

    fill(gfx, x, y, w, PURRGO_KBD_KEY_H, cursor ? BLACK : WHITE);
    if (!cursor && framed) {
        gfx_set_color(gfx, BLACK, WHITE);
        gfx_draw_rect(gfx, (int16_t)x, (int16_t)y, (int16_t)w, PURRGO_KBD_KEY_H);
    }
    draw_key_face(gfx, kb, key, x, y, w, ink);
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

/* Клавиша (row, col) вместе с зазором: прямоугольник для частичного обновления. */
static void mark_key(uint8_t layout, int row, int col)
{
    const purrgo_kbd_key_t *key = purrgo_kbd_key((purrgo_kbd_layout_t)layout, row, col);

    if (key != NULL) {
        mark(purrgo_kbd_key_x((purrgo_kbd_layout_t)layout, row, col),
             GRID_Y + row * PURRGO_KBD_ROW_PITCH, key->width, PURRGO_KBD_ROW_PITCH);
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
            mark(0, GRID_Y, SCREEN_W, PURRGO_KBD_ROWS * PURRGO_KBD_ROW_PITCH);
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
