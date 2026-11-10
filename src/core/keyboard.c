#include "purrgo/keyboard.h"

#include <string.h>

#include "purrgo/hardware_config.h"

/*
 * Таблицы раскладок. Коды букв - заглавные в PurrGO-256:
 * латиница ASCII, кириллица 0x80..0x9F (Ё - 0xF0), см. docs/font-encoding.md.
 * Строка 0 - цифры, строки 1..3 - буквы, строка 4 - служебные клавиши.
 */

/* Локальные макросы таблиц (снимаются ниже); последнее число - ширина в ячейках. */
#define L(c)      { PURRGO_KBD_KEY_LETTER, (uint8_t)(c), 1 }
#define C(c)      { PURRGO_KBD_KEY_CHAR,   (uint8_t)(c), 1 }
#define SHIFT(w)  { PURRGO_KBD_KEY_SHIFT,     0, (w) }
#define BKSP(w)   { PURRGO_KBD_KEY_BACKSPACE, 0, (w) }

#define DIGITS_ROW \
    C('1'), C('2'), C('3'), C('4'), C('5'), C('6'), C('7'), C('8'), C('9'), C('0')

#define BOTTOM_ROW(space_w) \
    { PURRGO_KBD_KEY_LANG,  0,   2 }, \
    { PURRGO_KBD_KEY_SYM,   0,   2 }, \
    { PURRGO_KBD_KEY_SPACE, ' ', (space_w) }, \
    { PURRGO_KBD_KEY_OK,    0,   2 }

static const purrgo_kbd_key_t s_en_row0[] = { DIGITS_ROW };
static const purrgo_kbd_key_t s_en_row1[] = {
    L('Q'), L('W'), L('E'), L('R'), L('T'), L('Y'), L('U'), L('I'), L('O'), L('P')
};
static const purrgo_kbd_key_t s_en_row2[] = {
    L('A'), L('S'), L('D'), L('F'), L('G'), L('H'), L('J'), L('K'), L('L')
};
static const purrgo_kbd_key_t s_en_row3[] = {
    SHIFT(1), L('Z'), L('X'), L('C'), L('V'), L('B'), L('N'), L('M'), BKSP(2)
};

/* Кириллица: А=0x80 Б=0x81 В=0x82 Г=0x83 Д=0x84 Е=0x85 Ж=0x86 З=0x87 И=0x88 Й=0x89
 * К=0x8A Л=0x8B М=0x8C Н=0x8D О=0x8E П=0x8F Р=0x90 С=0x91 Т=0x92 У=0x93 Ф=0x94
 * Х=0x95 Ц=0x96 Ч=0x97 Ш=0x98 Щ=0x99 Ъ=0x9A Ы=0x9B Ь=0x9C Э=0x9D Ю=0x9E Я=0x9F Ё=0xF0 */
static const purrgo_kbd_key_t s_ru_row0[] = { DIGITS_ROW };
static const purrgo_kbd_key_t s_ru_row1[] = {   /* Й Ц У К Е Н Г Ш Щ З Х Ъ */
    L(0x89), L(0x96), L(0x93), L(0x8A), L(0x85), L(0x8D),
    L(0x83), L(0x98), L(0x99), L(0x87), L(0x95), L(0x9A)
};
static const purrgo_kbd_key_t s_ru_row2[] = {   /* Ф Ы В А П Р О Л Д Ж Э */
    L(0x94), L(0x9B), L(0x82), L(0x80), L(0x8F), L(0x90),
    L(0x8E), L(0x8B), L(0x84), L(0x86), L(0x9D)
};
static const purrgo_kbd_key_t s_ru_row3[] = {   /* Shift Я Ч С М И Т Ь Б Ю Ё Backspace */
    SHIFT(1), L(0x9F), L(0x97), L(0x91), L(0x8C), L(0x88), L(0x92),
    L(0x9C), L(0x81), L(0x9E), L(0xF0), BKSP(1)
};

static const purrgo_kbd_key_t s_sym_row0[] = { DIGITS_ROW };
static const purrgo_kbd_key_t s_sym_row1[] = {
    C('-'), C('.'), C(','), C('\''), C('"'), C('('), C(')'), C('/'), C(':'), C(';')
};
static const purrgo_kbd_key_t s_sym_row2[] = {
    C('!'), C('?'), C('@'), C('#'), C('&'), C('+'), C('='), C('*'), C('%'), C('_')
};
static const purrgo_kbd_key_t s_sym_row3[] = {
    C('['), C(']'), C('{'), C('}'), C('<'), C('>'), C('\\'), C('|'), C('~'), BKSP(1)
};

/* 10 столбцов (EN, SYM) и 12 столбцов (RU): 2 + 2 + пробел + 2. */
static const purrgo_kbd_key_t s_bottom10[] = { BOTTOM_ROW(4) };
static const purrgo_kbd_key_t s_bottom12[] = { BOTTOM_ROW(6) };

#undef L
#undef C
#undef SHIFT
#undef BKSP
#undef DIGITS_ROW
#undef BOTTOM_ROW

typedef struct {
    const purrgo_kbd_key_t *keys;
    uint8_t count;
} kbd_row_t;

#define ROW(a) { (a), (uint8_t)(sizeof(a) / sizeof((a)[0])) }

static const kbd_row_t s_layouts[3][PURRGO_KBD_ROWS] = {
    [PURRGO_KBD_LAYOUT_EN] = {
        ROW(s_en_row0), ROW(s_en_row1), ROW(s_en_row2), ROW(s_en_row3), ROW(s_bottom10)
    },
    [PURRGO_KBD_LAYOUT_RU] = {
        ROW(s_ru_row0), ROW(s_ru_row1), ROW(s_ru_row2), ROW(s_ru_row3), ROW(s_bottom12)
    },
    [PURRGO_KBD_LAYOUT_SYM] = {
        ROW(s_sym_row0), ROW(s_sym_row1), ROW(s_sym_row2), ROW(s_sym_row3), ROW(s_bottom10)
    }
};

static bool layout_valid(purrgo_kbd_layout_t layout)
{
    return (unsigned)layout <= (unsigned)PURRGO_KBD_LAYOUT_SYM;
}

int purrgo_kbd_row_len(purrgo_kbd_layout_t layout, int row)
{
    if (!layout_valid(layout) || row < 0 || row >= PURRGO_KBD_ROWS) {
        return 0;
    }
    return s_layouts[layout][row].count;
}

const purrgo_kbd_key_t *purrgo_kbd_key(purrgo_kbd_layout_t layout, int row, int col)
{
    if (col < 0 || col >= purrgo_kbd_row_len(layout, row)) {
        return NULL;
    }
    return &s_layouts[layout][row].keys[col];
}

int purrgo_kbd_cols(purrgo_kbd_layout_t layout)
{
    return (layout == PURRGO_KBD_LAYOUT_RU) ? 12 : 10;
}

static int cell_w(purrgo_kbd_layout_t layout)
{
    return PURRGO_HW_DISPLAY_WIDTH_PX / purrgo_kbd_cols(layout);
}

/* Сетка по центру экрана; линия справа от последней ячейки тоже внутри экрана. */
static int grid_x0(purrgo_kbd_layout_t layout)
{
    return (PURRGO_HW_DISPLAY_WIDTH_PX - cell_w(layout) * purrgo_kbd_cols(layout) - 1) / 2;
}

int purrgo_kbd_key_x(purrgo_kbd_layout_t layout, int row, int col)
{
    if (purrgo_kbd_key(layout, row, col) == NULL) {
        return 0;
    }

    int units = 0;

    for (int i = 0; i < col; i++) {
        units += s_layouts[layout][row].keys[i].width;
    }
    return grid_x0(layout) + units * cell_w(layout);
}

int purrgo_kbd_key_w(purrgo_kbd_layout_t layout, int row, int col)
{
    const purrgo_kbd_key_t *k = purrgo_kbd_key(layout, row, col);

    return (k == NULL) ? 0 : k->width * cell_w(layout);
}

static int key_center_x(purrgo_kbd_layout_t layout, int row, int col)
{
    const purrgo_kbd_key_t *k = purrgo_kbd_key(layout, row, col);

    return (k == NULL) ? 0 : purrgo_kbd_key_x(layout, row, col) + purrgo_kbd_key_w(layout, row, col) / 2;
}

/* Клавиша строки, центр которой ближе всего к x. */
static int nearest_col(purrgo_kbd_layout_t layout, int row, int x)
{
    const int n = purrgo_kbd_row_len(layout, row);
    int best = 0;
    int best_d = 0x7FFF;

    for (int i = 0; i < n; i++) {
        int d = key_center_x(layout, row, i) - x;

        if (d < 0) {
            d = -d;
        }
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

static bool is_lower_ready(const purrgo_keyboard_t *kb)
{
    return kb->shift == PURRGO_KBD_SHIFT_OFF;
}

uint8_t purrgo_kbd_key_char(const purrgo_keyboard_t *kb, const purrgo_kbd_key_t *key)
{
    if (kb == NULL || key == NULL) {
        return 0;
    }

    switch (key->kind) {
    case PURRGO_KBD_KEY_CHAR:
    case PURRGO_KBD_KEY_SPACE:
        return key->code;

    case PURRGO_KBD_KEY_LETTER:
        if (!is_lower_ready(kb)) {
            return key->code;
        }
        if (key->code >= 'A' && key->code <= 'Z') {
            return (uint8_t)(key->code + 0x20u);        /* a..z */
        }
        if (key->code >= 0x80u && key->code <= 0x9Fu) {
            return (uint8_t)(key->code + 0x20u);        /* а..я = 0xA0..0xBF */
        }
        if (key->code == 0xF0u) {
            return 0xF1u;                                /* ё */
        }
        return key->code;

    default:
        return 0;
    }
}

static void set_cursor(purrgo_keyboard_t *kb, int row, int col)
{
    kb->row = (uint8_t)row;
    kb->col = (uint8_t)col;
    kb->desired_x = (int16_t)key_center_x((purrgo_kbd_layout_t)kb->layout, row, col);
}

/* После смены раскладки: остаться в той же строке на клавише, ближайшей по x. */
static void relocate_cursor(purrgo_keyboard_t *kb)
{
    const purrgo_kbd_layout_t layout = (purrgo_kbd_layout_t)kb->layout;

    kb->col = (uint8_t)nearest_col(layout, kb->row, kb->desired_x);
}

void purrgo_kbd_open(purrgo_keyboard_t *kb, const char *title,
                     const char *initial, purrgo_kbd_layout_t layout)
{
    if (kb == NULL) {
        return;
    }
    memset(kb, 0, sizeof(*kb));

    if (title != NULL) {
        strncpy(kb->title, title, PURRGO_KBD_TITLE_MAX);
        kb->title[PURRGO_KBD_TITLE_MAX] = '\0';
    }
    if (initial != NULL) {
        strncpy(kb->text, initial, PURRGO_KBD_TEXT_MAX);
        kb->text[PURRGO_KBD_TEXT_MAX] = '\0';
        kb->len = (uint8_t)strlen(kb->text);
    }

    if (layout != PURRGO_KBD_LAYOUT_RU) {
        layout = PURRGO_KBD_LAYOUT_EN;
    }
    kb->layout = (uint8_t)layout;
    kb->alpha = (uint8_t)layout;
    kb->shift = PURRGO_KBD_SHIFT_OFF;
    kb->result = PURRGO_KBD_RESULT_EDITING;
    set_cursor(kb, 1, 0);
}

purrgo_kbd_action_t purrgo_kbd_action_from_button(purrgo_btn_t button)
{
    switch (button) {
    case PURRGO_BTN_KEY1_SHORT: return PURRGO_KBD_ACT_LEFT;
    case PURRGO_BTN_KEY2_SHORT: return PURRGO_KBD_ACT_RIGHT;
    case PURRGO_BTN_KEY3_SHORT: return PURRGO_KBD_ACT_PRESS;
    case PURRGO_BTN_KEY4_SHORT: return PURRGO_KBD_ACT_DOWN;
    case PURRGO_BTN_KEY1_LONG:  return PURRGO_KBD_ACT_BACKSPACE;
    case PURRGO_BTN_KEY2_LONG:  return PURRGO_KBD_ACT_UP;
    case PURRGO_BTN_KEY3_LONG:  return PURRGO_KBD_ACT_ACCEPT;
    case PURRGO_BTN_KEY4_LONG:  return PURRGO_KBD_ACT_CANCEL;
    default:                    return PURRGO_KBD_ACT_NONE;
    }
}

static bool type_char(purrgo_keyboard_t *kb, uint8_t ch, bool is_letter)
{
    if (ch == 0 || kb->len >= PURRGO_KBD_TEXT_MAX) {
        return false;
    }
    kb->text[kb->len++] = (char)ch;
    kb->text[kb->len] = '\0';

    if (is_letter && kb->shift == PURRGO_KBD_SHIFT_ONCE) {
        kb->shift = PURRGO_KBD_SHIFT_OFF;
    }
    return true;
}

static bool backspace(purrgo_keyboard_t *kb)
{
    if (kb->len == 0) {
        return false;
    }
    kb->text[--kb->len] = '\0';
    return true;
}

static bool press_key(purrgo_keyboard_t *kb)
{
    const purrgo_kbd_layout_t layout = (purrgo_kbd_layout_t)kb->layout;
    const purrgo_kbd_key_t *key = purrgo_kbd_key(layout, kb->row, kb->col);

    if (key == NULL) {
        return false;
    }

    switch (key->kind) {
    case PURRGO_KBD_KEY_LETTER:
        return type_char(kb, purrgo_kbd_key_char(kb, key), true);

    case PURRGO_KBD_KEY_CHAR:
    case PURRGO_KBD_KEY_SPACE:
        return type_char(kb, purrgo_kbd_key_char(kb, key), false);

    case PURRGO_KBD_KEY_SHIFT:
        kb->shift = (uint8_t)((kb->shift + 1u) % 3u);   /* OFF -> ONCE -> LOCK -> OFF */
        return true;

    case PURRGO_KBD_KEY_BACKSPACE:
        return backspace(kb);

    case PURRGO_KBD_KEY_LANG:
        kb->alpha = (kb->alpha == PURRGO_KBD_LAYOUT_EN) ? PURRGO_KBD_LAYOUT_RU
                                                         : PURRGO_KBD_LAYOUT_EN;
        kb->layout = kb->alpha;
        relocate_cursor(kb);
        return true;

    case PURRGO_KBD_KEY_SYM:
        kb->layout = (kb->layout == PURRGO_KBD_LAYOUT_SYM) ? kb->alpha
                                                            : PURRGO_KBD_LAYOUT_SYM;
        relocate_cursor(kb);
        return true;

    case PURRGO_KBD_KEY_OK:
        kb->result = PURRGO_KBD_RESULT_ACCEPTED;
        return true;

    default:
        return false;
    }
}

static void move_horizontal(purrgo_keyboard_t *kb, int step)
{
    const purrgo_kbd_layout_t layout = (purrgo_kbd_layout_t)kb->layout;
    const int n = purrgo_kbd_row_len(layout, kb->row);
    int col = ((int)kb->col + step + n) % n;

    set_cursor(kb, kb->row, col);
}

static void move_vertical(purrgo_keyboard_t *kb, int step)
{
    const purrgo_kbd_layout_t layout = (purrgo_kbd_layout_t)kb->layout;
    const int row = ((int)kb->row + step + PURRGO_KBD_ROWS) % PURRGO_KBD_ROWS;
    const int col = nearest_col(layout, row, kb->desired_x);

    /* desired_x не меняется: цепочка вверх/вниз возвращается в исходный столбец. */
    kb->row = (uint8_t)row;
    kb->col = (uint8_t)col;
}

bool purrgo_kbd_handle_action(purrgo_keyboard_t *kb, purrgo_kbd_action_t action)
{
    if (kb == NULL || kb->result != PURRGO_KBD_RESULT_EDITING) {
        return false;
    }

    switch (action) {
    case PURRGO_KBD_ACT_LEFT:
        move_horizontal(kb, -1);
        return true;
    case PURRGO_KBD_ACT_RIGHT:
        move_horizontal(kb, 1);
        return true;
    case PURRGO_KBD_ACT_UP:
        move_vertical(kb, -1);
        return true;
    case PURRGO_KBD_ACT_DOWN:
        move_vertical(kb, 1);
        return true;
    case PURRGO_KBD_ACT_PRESS:
        return press_key(kb);
    case PURRGO_KBD_ACT_BACKSPACE:
        return backspace(kb);
    case PURRGO_KBD_ACT_ACCEPT:
        kb->result = PURRGO_KBD_RESULT_ACCEPTED;
        return true;
    case PURRGO_KBD_ACT_CANCEL:
        kb->result = PURRGO_KBD_RESULT_CANCELLED;
        return true;
    default:
        return false;
    }
}

bool purrgo_kbd_handle_button(purrgo_keyboard_t *kb, purrgo_btn_t button)
{
    return purrgo_kbd_handle_action(kb, purrgo_kbd_action_from_button(button));
}

const char *purrgo_kbd_text(const purrgo_keyboard_t *kb)
{
    return (kb == NULL) ? "" : kb->text;
}

size_t purrgo_kbd_len(const purrgo_keyboard_t *kb)
{
    return (kb == NULL) ? 0u : kb->len;
}

purrgo_kbd_result_t purrgo_kbd_result(const purrgo_keyboard_t *kb)
{
    return (kb == NULL) ? PURRGO_KBD_RESULT_CANCELLED : (purrgo_kbd_result_t)kb->result;
}

void purrgo_kbd_clear(purrgo_keyboard_t *kb)
{
    if (kb == NULL) {
        return;
    }
    kb->text[0] = '\0';
    kb->len = 0;
}
