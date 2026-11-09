#ifndef PURRGO_KEYBOARD_H
#define PURRGO_KEYBOARD_H

/*
 * Экранная клавиатура En/Ru (логика, без отрисовки; отрисовка - ui_keyboard.h).
 *
 * Назначение: ввод строки для поиска объектов на карте и текста в книгах.
 * Устройство имеет четыре кнопки, поэтому курсор клавиатуры перемещается по
 * сетке клавиш, а нажатие выбранной клавиши - отдельное действие.
 *
 * Текст хранится в кодировке PurrGO-256 (docs/PurrGO_font_encoding.md), то есть
 * в той же, в которой записаны названия объектов в картах и текст книг PGB:
 * результат можно сразу сравнивать с ними, перекодировка не нужна. Латиница -
 * ASCII, кириллица - 0x80..0xBF, Ё/ё - 0xF0/0xF1. Регистр сохраняется как
 * набран; регистронезависимое сравнение - дело вызывающего.
 *
 * Раскладки:
 *   EN  - цифры, QWERTY;
 *   RU  - цифры, ЙЦУКЕН (с Ё);
 *   SYM - цифры и знаки препинания (общая для обоих языков).
 * Shift: выключен -> один раз (сбрасывается после буквы) -> постоянный (Caps).
 *
 * Кнопки (purrgo_kbd_action_from_button):
 *   SHORT: K1 влево, K2 вправо, K3 нажать клавишу, K4 вниз.
 *   LONG:  K1 стереть символ, K2 вверх, K3 готово (ввод принят), K4 отмена.
 * Курсор по строкам и столбцам замыкается (за последней клавишей - первая).
 *
 * Модуль не зависит от платформы и не выделяет память.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "purrgo/app_fsm.h"

/* Максимальная длина вводимой строки в байтах PurrGO-256 (без завершающего нуля). */
#define PURRGO_KBD_TEXT_MAX 48

/* Заголовок над полем ввода (например "SEARCH"), без завершающего нуля. */
#define PURRGO_KBD_TITLE_MAX 24

typedef enum {
    PURRGO_KBD_LAYOUT_EN = 0,
    PURRGO_KBD_LAYOUT_RU = 1,
    PURRGO_KBD_LAYOUT_SYM = 2
} purrgo_kbd_layout_t;

typedef enum {
    PURRGO_KBD_SHIFT_OFF = 0,
    PURRGO_KBD_SHIFT_ONCE = 1,   /* следующая буква заглавная */
    PURRGO_KBD_SHIFT_LOCK = 2    /* Caps Lock */
} purrgo_kbd_shift_t;

typedef enum {
    PURRGO_KBD_RESULT_EDITING = 0,   /* ввод продолжается */
    PURRGO_KBD_RESULT_ACCEPTED,      /* ввод принят: текст в purrgo_kbd_text() */
    PURRGO_KBD_RESULT_CANCELLED      /* ввод отменён */
} purrgo_kbd_result_t;

typedef enum {
    PURRGO_KBD_ACT_NONE = 0,
    PURRGO_KBD_ACT_LEFT,
    PURRGO_KBD_ACT_RIGHT,
    PURRGO_KBD_ACT_UP,
    PURRGO_KBD_ACT_DOWN,
    PURRGO_KBD_ACT_PRESS,       /* нажать клавишу под курсором */
    PURRGO_KBD_ACT_BACKSPACE,   /* стереть последний символ (без перехода к клавише) */
    PURRGO_KBD_ACT_ACCEPT,      /* принять ввод (клавиша OK) */
    PURRGO_KBD_ACT_CANCEL       /* отменить ввод */
} purrgo_kbd_action_t;

typedef enum {
    PURRGO_KBD_KEY_LETTER = 0,  /* буква: регистр зависит от Shift */
    PURRGO_KBD_KEY_CHAR,        /* цифра или знак: не зависит от Shift */
    PURRGO_KBD_KEY_SHIFT,
    PURRGO_KBD_KEY_BACKSPACE,
    PURRGO_KBD_KEY_LANG,        /* переключение EN <-> RU */
    PURRGO_KBD_KEY_SYM,         /* вход в SYM и возврат в буквенную раскладку */
    PURRGO_KBD_KEY_SPACE,
    PURRGO_KBD_KEY_OK
} purrgo_kbd_key_kind_t;

/* Описание клавиши в таблице раскладки. */
typedef struct {
    uint8_t kind;   /* purrgo_kbd_key_kind_t */
    uint8_t code;   /* LETTER: заглавная буква PurrGO-256; CHAR: символ */
    uint8_t width;  /* ширина в ячейках сетки (1 - обычная клавиша) */
} purrgo_kbd_key_t;

/*
 * Геометрия сетки. Клавиши лежат в регулярной сетке: строки не сдвинуты друг
 * относительно друга, ячейки одной ширины, соседние клавиши делят общую линию.
 * Число столбцов зависит от раскладки (RU - 12, EN и SYM - 10), ширина ячейки
 * подбирается под экран. Высота ячейки одинакова во всех раскладках.
 */
#define PURRGO_KBD_ROWS      5
#define PURRGO_KBD_ROW_PITCH 18
#define PURRGO_KBD_MAX_COLS  12

typedef struct {
    char    text[PURRGO_KBD_TEXT_MAX + 1];   /* PurrGO-256, завершён нулём */
    uint8_t len;
    char    title[PURRGO_KBD_TITLE_MAX + 1];

    uint8_t layout;      /* purrgo_kbd_layout_t */
    uint8_t alpha;       /* последняя буквенная раскладка (EN или RU): для возврата из SYM */
    uint8_t shift;       /* purrgo_kbd_shift_t */
    uint8_t row;         /* клавиша под курсором */
    uint8_t col;
    int16_t desired_x;   /* центр клавиши, к которому тянется курсор при движении вверх/вниз */
    uint8_t result;      /* purrgo_kbd_result_t */
} purrgo_keyboard_t;

/*
 * Открыть клавиатуру. title - заголовок над полем ввода (ASCII или
 * PurrGO-256), может быть NULL. initial - начальный текст в PurrGO-256
 * (NULL - пусто), обрезается до PURRGO_KBD_TEXT_MAX. layout - EN или RU
 * (SYM заменяется на EN). Курсор встаёт на первую клавишу первой буквенной
 * строки (Q или Й), Shift выключен.
 */
void purrgo_kbd_open(purrgo_keyboard_t *kb, const char *title,
                     const char *initial, purrgo_kbd_layout_t layout);

/* Преобразование кнопки устройства в действие клавиатуры (NONE для комбинаций). */
purrgo_kbd_action_t purrgo_kbd_action_from_button(purrgo_btn_t button);

/*
 * Выполнить действие. Возвращает true, если что-то изменилось (курсор, текст,
 * раскладка, Shift или результат) и экран нужно перерисовать.
 * После ACCEPTED/CANCELLED действия игнорируются.
 */
bool purrgo_kbd_handle_action(purrgo_keyboard_t *kb, purrgo_kbd_action_t action);

/* То же для кнопки устройства. */
bool purrgo_kbd_handle_button(purrgo_keyboard_t *kb, purrgo_btn_t button);

/* Введённый текст (PurrGO-256, завершён нулём) и его длина. */
const char *purrgo_kbd_text(const purrgo_keyboard_t *kb);
size_t purrgo_kbd_len(const purrgo_keyboard_t *kb);

purrgo_kbd_result_t purrgo_kbd_result(const purrgo_keyboard_t *kb);

/* Очистить текст (раскладка и курсор не меняются). */
void purrgo_kbd_clear(purrgo_keyboard_t *kb);

/* --- Описание раскладок для отрисовки и тестов ---------------------------- */

/* Число клавиш в строке раскладки (0 для строки вне 0..PURRGO_KBD_ROWS-1). */
int purrgo_kbd_row_len(purrgo_kbd_layout_t layout, int row);

/* Клавиша раскладки или NULL, если row/col вне таблицы. */
const purrgo_kbd_key_t *purrgo_kbd_key(purrgo_kbd_layout_t layout, int row, int col);

/* Число столбцов (ячеек) сетки раскладки. */
int purrgo_kbd_cols(purrgo_kbd_layout_t layout);

/* Координата x левого края клавиши (px). Клавиши разных строк, занимающие одни ячейки, выровнены. */
int purrgo_kbd_key_x(purrgo_kbd_layout_t layout, int row, int col);

/* Ширина клавиши (px) от её левой линии до левой линии следующей клавиши. */
int purrgo_kbd_key_w(purrgo_kbd_layout_t layout, int row, int col);

/*
 * Символ PurrGO-256, который введёт клавиша при текущем Shift (для LETTER -
 * с учётом регистра, для CHAR - сам символ, для SPACE - ' '); 0 для служебных
 * клавиш (Shift, Backspace, Lang, Sym, OK).
 */
uint8_t purrgo_kbd_key_char(const purrgo_keyboard_t *kb, const purrgo_kbd_key_t *key);

#endif /* PURRGO_KEYBOARD_H */
