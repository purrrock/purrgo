/*
 * Host-тесты экранной клавиатуры En/Ru (src/core/keyboard.c, ui/ui_keyboard.c):
 *   - раскладки: коды букв, геометрия, отсутствие перекрытий и выхода за экран;
 *   - ввод: регистр, Shift (один раз / Caps), цифры, знаки, пробел, Backspace, лимит длины;
 *   - переключение EN/RU/SYM, перемещение курсора с замыканием и запоминанием столбца;
 *   - кнопки устройства, принятие и отмена ввода;
 *   - отрисовка и частичное обновление дисплея.
 */
#include <string.h>

#include "purrgo/display_hal.h"
#include "purrgo/gfx_renderer.h"
#include "purrgo/hardware_config.h"
#include "purrgo/keyboard.h"
#include "purrgo/pc_display_stats.h"
#include "ui_keyboard.h"
#include "test_common.h"

#define W PURRGO_HW_DISPLAY_WIDTH_PX
#define H PURRGO_HW_DISPLAY_HEIGHT_PX

static uint8_t g_fb[W * H];

static void cb_draw(void *fb, int16_t x, int16_t y, gfx_color_t c)
{
    ((uint8_t *)fb)[y * W + x] = c;
}

static gfx_color_t cb_read(void *fb, int16_t x, int16_t y)
{
    return ((uint8_t *)fb)[y * W + x];
}

static void act(purrgo_keyboard_t *kb, purrgo_kbd_action_t a)
{
    purrgo_kbd_handle_action(kb, a);
}

/* Переместить курсор на клавишу (row, col) кратчайшими шагами вправо/вниз; для тестов достаточно. */
static void goto_key(purrgo_keyboard_t *kb, int row, int col)
{
    int guard = 0;

    while (kb->row != row && guard++ < 16) act(kb, PURRGO_KBD_ACT_DOWN);
    /* Вертикальные шаги выбирают ближайшую клавишу; дальше выравниваем по столбцу. */
    guard = 0;
    while (kb->col != col && guard++ < 32) act(kb, PURRGO_KBD_ACT_RIGHT);
}

static void press_at(purrgo_keyboard_t *kb, int row, int col)
{
    goto_key(kb, row, col);
    act(kb, PURRGO_KBD_ACT_PRESS);
}

/* Нажать клавишу, на которой выведена буква с заданным кодом (в текущей раскладке и регистре). */
static int press_char(purrgo_keyboard_t *kb, uint8_t ch)
{
    const purrgo_kbd_layout_t lay = (purrgo_kbd_layout_t)kb->layout;

    for (int r = 0; r < PURRGO_KBD_ROWS; r++) {
        for (int c = 0; c < purrgo_kbd_row_len(lay, r); c++) {
            const purrgo_kbd_key_t *k = purrgo_kbd_key(lay, r, c);

            if (purrgo_kbd_key_char(kb, k) == ch && k->kind != PURRGO_KBD_KEY_SPACE) {
                press_at(kb, r, c);
                return 1;
            }
        }
    }
    return 0;
}

static int find_kind(purrgo_kbd_layout_t lay, purrgo_kbd_key_kind_t kind, int *row, int *col)
{
    for (int r = 0; r < PURRGO_KBD_ROWS; r++) {
        for (int c = 0; c < purrgo_kbd_row_len(lay, r); c++) {
            if (purrgo_kbd_key(lay, r, c)->kind == kind) {
                *row = r;
                *col = c;
                return 1;
            }
        }
    }
    return 0;
}

static void press_kind(purrgo_keyboard_t *kb, purrgo_kbd_key_kind_t kind)
{
    int r = 0, c = 0;

    CHECK(find_kind((purrgo_kbd_layout_t)kb->layout, kind, &r, &c));
    press_at(kb, r, c);
}

static void test_layout_tables(void)
{
    for (int lay = PURRGO_KBD_LAYOUT_EN; lay <= PURRGO_KBD_LAYOUT_SYM; lay++) {
        for (int r = 0; r < PURRGO_KBD_ROWS; r++) {
            const int n = purrgo_kbd_row_len((purrgo_kbd_layout_t)lay, r);

            CHECK(n > 0 && n <= PURRGO_KBD_MAX_COLS);
            int prev_end = -1;

            for (int c = 0; c < n; c++) {
                const purrgo_kbd_key_t *k = purrgo_kbd_key((purrgo_kbd_layout_t)lay, r, c);
                const int x = purrgo_kbd_key_x((purrgo_kbd_layout_t)lay, r, c);

                CHECK(k != NULL);
                CHECK(x >= 0 && x + k->width <= W);   /* в пределах экрана */
                CHECK(x >= prev_end);                 /* без перекрытий */
                prev_end = x + k->width;
            }
        }
    }
    CHECK(purrgo_kbd_key(PURRGO_KBD_LAYOUT_EN, 0, 99) == NULL);
    CHECK(purrgo_kbd_key(PURRGO_KBD_LAYOUT_EN, 9, 0) == NULL);
    CHECK(purrgo_kbd_row_len(PURRGO_KBD_LAYOUT_EN, -1) == 0);

    /* Все буквы алфавитов на месте и не повторяются. */
    for (int lay = PURRGO_KBD_LAYOUT_EN; lay <= PURRGO_KBD_LAYOUT_RU; lay++) {
        int seen[256] = {0};
        int letters = 0;

        for (int r = 0; r < PURRGO_KBD_ROWS; r++) {
            for (int c = 0; c < purrgo_kbd_row_len((purrgo_kbd_layout_t)lay, r); c++) {
                const purrgo_kbd_key_t *k = purrgo_kbd_key((purrgo_kbd_layout_t)lay, r, c);

                if (k->kind == PURRGO_KBD_KEY_LETTER) {
                    CHECK(!seen[k->code]);
                    seen[k->code] = 1;
                    letters++;
                }
            }
        }
        if (lay == PURRGO_KBD_LAYOUT_EN) {
            CHECK(letters == 26);
            for (int ch = 'A'; ch <= 'Z'; ch++) CHECK(seen[ch]);
        } else {
            CHECK(letters == 33);
            for (int ch = 0x80; ch <= 0x9F; ch++) CHECK(seen[ch]);
            CHECK(seen[0xF0]);
        }
    }
}

static void test_typing_en(void)
{
    purrgo_keyboard_t kb;

    purrgo_kbd_open(&kb, "SEARCH", NULL, PURRGO_KBD_LAYOUT_EN);
    CHECK(strcmp(kb.title, "SEARCH") == 0);
    CHECK(purrgo_kbd_len(&kb) == 0 && purrgo_kbd_text(&kb)[0] == '\0');
    CHECK(kb.shift == PURRGO_KBD_SHIFT_OFF);

    /* Без Shift - строчные. */
    CHECK(press_char(&kb, 'h'));
    CHECK(press_char(&kb, 'i'));
    CHECK(strcmp(purrgo_kbd_text(&kb), "hi") == 0);

    /* Shift один раз: следующая буква заглавная, потом снова строчные. */
    press_kind(&kb, PURRGO_KBD_KEY_SHIFT);
    CHECK(kb.shift == PURRGO_KBD_SHIFT_ONCE);
    CHECK(press_char(&kb, 'T'));
    CHECK(kb.shift == PURRGO_KBD_SHIFT_OFF);
    CHECK(press_char(&kb, 'o'));
    CHECK(strcmp(purrgo_kbd_text(&kb), "hiTo") == 0);

    /* Двойное нажатие Shift - Caps Lock, третье выключает. */
    press_kind(&kb, PURRGO_KBD_KEY_SHIFT);
    press_kind(&kb, PURRGO_KBD_KEY_SHIFT);
    CHECK(kb.shift == PURRGO_KBD_SHIFT_LOCK);
    CHECK(press_char(&kb, 'X'));
    CHECK(press_char(&kb, 'Y'));
    CHECK(kb.shift == PURRGO_KBD_SHIFT_LOCK);
    press_kind(&kb, PURRGO_KBD_KEY_SHIFT);
    CHECK(kb.shift == PURRGO_KBD_SHIFT_OFF);
    CHECK(strcmp(purrgo_kbd_text(&kb), "hiToXY") == 0);

    /* Цифры и пробел не зависят от Shift и не сбрасывают его. */
    press_kind(&kb, PURRGO_KBD_KEY_SHIFT);
    CHECK(press_char(&kb, '7'));
    CHECK(kb.shift == PURRGO_KBD_SHIFT_ONCE);
    press_kind(&kb, PURRGO_KBD_KEY_SPACE);
    CHECK(strcmp(purrgo_kbd_text(&kb), "hiToXY7 ") == 0);

    /* Backspace: клавишей и долгим K1; пустую строку не портит. */
    press_kind(&kb, PURRGO_KBD_KEY_BACKSPACE);
    CHECK(strcmp(purrgo_kbd_text(&kb), "hiToXY7") == 0);
    CHECK(purrgo_kbd_handle_button(&kb, PURRGO_BTN_KEY1_LONG));
    CHECK(strcmp(purrgo_kbd_text(&kb), "hiToXY") == 0);
    purrgo_kbd_clear(&kb);
    CHECK(purrgo_kbd_len(&kb) == 0);
    CHECK(!purrgo_kbd_handle_action(&kb, PURRGO_KBD_ACT_BACKSPACE));
}

static void test_typing_ru(void)
{
    purrgo_keyboard_t kb;

    purrgo_kbd_open(&kb, "", NULL, PURRGO_KBD_LAYOUT_RU);
    CHECK(kb.layout == PURRGO_KBD_LAYOUT_RU);

    /* "Минск": М=0x8C и, н, с, к строчные = A8 AD B1 AA. */
    press_kind(&kb, PURRGO_KBD_KEY_SHIFT);
    CHECK(press_char(&kb, 0x8C));
    CHECK(press_char(&kb, 0xA8));
    CHECK(press_char(&kb, 0xAD));
    CHECK(press_char(&kb, 0xB1));
    CHECK(press_char(&kb, 0xAA));
    CHECK(strcmp(purrgo_kbd_text(&kb), "\x8C\xA8\xAD\xB1\xAA") == 0);

    /* Ё/ё: заглавная F0, строчная F1. */
    CHECK(press_char(&kb, 0xF1));
    press_kind(&kb, PURRGO_KBD_KEY_SHIFT);
    CHECK(press_char(&kb, 0xF0));
    CHECK(strcmp(purrgo_kbd_text(&kb), "\x8C\xA8\xAD\xB1\xAA\xF1\xF0") == 0);

    /* Ъ/ъ и Я/я: крайние коды диапазона. */
    CHECK(press_char(&kb, 0xBA));
    press_kind(&kb, PURRGO_KBD_KEY_SHIFT);
    CHECK(press_char(&kb, 0x9F));
    CHECK(kb.text[kb.len - 1] == (char)0x9F && kb.text[kb.len - 2] == (char)0xBA);
}

static void test_layout_switching(void)
{
    purrgo_keyboard_t kb;

    purrgo_kbd_open(&kb, "", NULL, PURRGO_KBD_LAYOUT_EN);

    /* EN -> RU -> EN. */
    press_kind(&kb, PURRGO_KBD_KEY_LANG);
    CHECK(kb.layout == PURRGO_KBD_LAYOUT_RU && kb.alpha == PURRGO_KBD_LAYOUT_RU);
    press_kind(&kb, PURRGO_KBD_KEY_LANG);
    CHECK(kb.layout == PURRGO_KBD_LAYOUT_EN);

    /* SYM и возврат в ту же языковую раскладку. */
    press_kind(&kb, PURRGO_KBD_KEY_LANG);                       /* RU */
    press_kind(&kb, PURRGO_KBD_KEY_SYM);
    CHECK(kb.layout == PURRGO_KBD_LAYOUT_SYM && kb.alpha == PURRGO_KBD_LAYOUT_RU);
    CHECK(press_char(&kb, '-'));
    CHECK(press_char(&kb, '.'));
    CHECK(press_char(&kb, '('));
    CHECK(strcmp(purrgo_kbd_text(&kb), "-.(") == 0);
    press_kind(&kb, PURRGO_KBD_KEY_SYM);
    CHECK(kb.layout == PURRGO_KBD_LAYOUT_RU);

    /* LANG из SYM ведёт в другой язык. */
    press_kind(&kb, PURRGO_KBD_KEY_SYM);
    press_kind(&kb, PURRGO_KBD_KEY_LANG);
    CHECK(kb.layout == PURRGO_KBD_LAYOUT_EN);

    /* Курсор остаётся внутри таблицы после смены раскладки (в строках разной длины). */
    for (int i = 0; i < 6; i++) {
        act(&kb, PURRGO_KBD_ACT_DOWN);
        press_kind(&kb, PURRGO_KBD_KEY_LANG);
        CHECK(purrgo_kbd_key((purrgo_kbd_layout_t)kb.layout, kb.row, kb.col) != NULL);
    }

    /* open: SYM заменяется на EN. */
    purrgo_kbd_open(&kb, "", NULL, PURRGO_KBD_LAYOUT_SYM);
    CHECK(kb.layout == PURRGO_KBD_LAYOUT_EN);
}

static void test_cursor_movement(void)
{
    purrgo_keyboard_t kb;

    purrgo_kbd_open(&kb, "", NULL, PURRGO_KBD_LAYOUT_EN);
    CHECK(kb.row == 1 && kb.col == 0);

    /* Влево от первой клавиши - последняя клавиша строки (замыкание). */
    act(&kb, PURRGO_KBD_ACT_LEFT);
    CHECK(kb.col == purrgo_kbd_row_len(PURRGO_KBD_LAYOUT_EN, 1) - 1);
    act(&kb, PURRGO_KBD_ACT_RIGHT);
    CHECK(kb.col == 0);

    /* Вверх с первой строки сетки - в строку цифр, потом замыкание на нижнюю строку. */
    act(&kb, PURRGO_KBD_ACT_UP);
    CHECK(kb.row == 0);
    act(&kb, PURRGO_KBD_ACT_UP);
    CHECK(kb.row == PURRGO_KBD_ROWS - 1);
    act(&kb, PURRGO_KBD_ACT_DOWN);
    CHECK(kb.row == 0);

    /* Столбец запоминается: вниз-вверх возвращает на ту же клавишу. */
    purrgo_kbd_open(&kb, "", NULL, PURRGO_KBD_LAYOUT_EN);
    for (int i = 0; i < 7; i++) act(&kb, PURRGO_KBD_ACT_RIGHT);   /* U */
    const int col = kb.col;
    act(&kb, PURRGO_KBD_ACT_DOWN);
    act(&kb, PURRGO_KBD_ACT_DOWN);
    act(&kb, PURRGO_KBD_ACT_UP);
    act(&kb, PURRGO_KBD_ACT_UP);
    CHECK(kb.row == 1 && kb.col == col);

    /* Движение меняет состояние (нужна перерисовка), нажатие "в пустоту" - нет. */
    CHECK(purrgo_kbd_handle_action(&kb, PURRGO_KBD_ACT_LEFT));
    CHECK(!purrgo_kbd_handle_action(&kb, PURRGO_KBD_ACT_NONE));
}

static void test_buttons_and_result(void)
{
    purrgo_keyboard_t kb;

    CHECK(purrgo_kbd_action_from_button(PURRGO_BTN_KEY1_SHORT) == PURRGO_KBD_ACT_LEFT);
    CHECK(purrgo_kbd_action_from_button(PURRGO_BTN_KEY2_SHORT) == PURRGO_KBD_ACT_RIGHT);
    CHECK(purrgo_kbd_action_from_button(PURRGO_BTN_KEY3_SHORT) == PURRGO_KBD_ACT_PRESS);
    CHECK(purrgo_kbd_action_from_button(PURRGO_BTN_KEY4_SHORT) == PURRGO_KBD_ACT_DOWN);
    CHECK(purrgo_kbd_action_from_button(PURRGO_BTN_KEY1_LONG) == PURRGO_KBD_ACT_BACKSPACE);
    CHECK(purrgo_kbd_action_from_button(PURRGO_BTN_KEY2_LONG) == PURRGO_KBD_ACT_UP);
    CHECK(purrgo_kbd_action_from_button(PURRGO_BTN_KEY3_LONG) == PURRGO_KBD_ACT_ACCEPT);
    CHECK(purrgo_kbd_action_from_button(PURRGO_BTN_KEY4_LONG) == PURRGO_KBD_ACT_CANCEL);
    CHECK(purrgo_kbd_action_from_button(PURRGO_BTN_COMBO_K1_K4) == PURRGO_KBD_ACT_NONE);

    /* Ввод кнопками: K3 нажимает клавишу под курсором (Q), долгий K3 принимает. */
    purrgo_kbd_open(&kb, "", NULL, PURRGO_KBD_LAYOUT_EN);
    CHECK(purrgo_kbd_handle_button(&kb, PURRGO_BTN_KEY3_SHORT));
    CHECK(strcmp(purrgo_kbd_text(&kb), "q") == 0);
    CHECK(purrgo_kbd_result(&kb) == PURRGO_KBD_RESULT_EDITING);
    CHECK(purrgo_kbd_handle_button(&kb, PURRGO_BTN_KEY3_LONG));
    CHECK(purrgo_kbd_result(&kb) == PURRGO_KBD_RESULT_ACCEPTED);
    /* После завершения ввод игнорируется. */
    CHECK(!purrgo_kbd_handle_button(&kb, PURRGO_BTN_KEY3_SHORT));
    CHECK(strcmp(purrgo_kbd_text(&kb), "q") == 0);

    /* Клавиша OK на экране тоже принимает; долгий K4 отменяет. */
    purrgo_kbd_open(&kb, "", "abc", PURRGO_KBD_LAYOUT_EN);
    press_kind(&kb, PURRGO_KBD_KEY_OK);
    CHECK(purrgo_kbd_result(&kb) == PURRGO_KBD_RESULT_ACCEPTED);
    CHECK(strcmp(purrgo_kbd_text(&kb), "abc") == 0);

    purrgo_kbd_open(&kb, "", "abc", PURRGO_KBD_LAYOUT_EN);
    CHECK(purrgo_kbd_handle_button(&kb, PURRGO_BTN_KEY4_LONG));
    CHECK(purrgo_kbd_result(&kb) == PURRGO_KBD_RESULT_CANCELLED);

    /* NULL безопасен. */
    CHECK(!purrgo_kbd_handle_action(NULL, PURRGO_KBD_ACT_PRESS));
    CHECK(purrgo_kbd_len(NULL) == 0 && purrgo_kbd_text(NULL)[0] == '\0');
    purrgo_kbd_open(NULL, "", NULL, PURRGO_KBD_LAYOUT_EN);
    purrgo_kbd_clear(NULL);
}

static void test_text_limits(void)
{
    purrgo_keyboard_t kb;
    char big[PURRGO_KBD_TEXT_MAX + 20];
    char title[PURRGO_KBD_TITLE_MAX + 20];

    /* Начальный текст и заголовок обрезаются, строка остаётся завершённой. */
    memset(big, 'a', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    memset(title, 'T', sizeof title - 1);
    title[sizeof title - 1] = '\0';
    purrgo_kbd_open(&kb, title, big, PURRGO_KBD_LAYOUT_EN);
    CHECK(purrgo_kbd_len(&kb) == PURRGO_KBD_TEXT_MAX);
    CHECK(strlen(kb.text) == PURRGO_KBD_TEXT_MAX);
    CHECK(strlen(kb.title) == PURRGO_KBD_TITLE_MAX);

    /* Переполнение: символ не добавляется, изменений нет, Shift не сбрасывается. */
    press_kind(&kb, PURRGO_KBD_KEY_SHIFT);
    goto_key(&kb, 1, 0);
    CHECK(!purrgo_kbd_handle_action(&kb, PURRGO_KBD_ACT_PRESS));
    CHECK(purrgo_kbd_len(&kb) == PURRGO_KBD_TEXT_MAX);
    CHECK(kb.shift == PURRGO_KBD_SHIFT_ONCE);

    /* Стирание освобождает место. */
    act(&kb, PURRGO_KBD_ACT_BACKSPACE);
    CHECK(purrgo_kbd_handle_action(&kb, PURRGO_KBD_ACT_PRESS));
    CHECK(purrgo_kbd_len(&kb) == PURRGO_KBD_TEXT_MAX && kb.text[kb.len - 1] == 'Q');
}

static int fb_black_pixels(int x0, int y0, int w, int h)
{
    int n = 0;

    for (int y = y0; y < y0 + h; y++) {
        for (int x = x0; x < x0 + w; x++) {
            if (g_fb[y * W + x] == BLACK) n++;
        }
    }
    return n;
}

static void test_render_and_refresh(void)
{
    gfx_context_t g;
    purrgo_keyboard_t kb;
    pc_display_stats_t st;

    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));
    purrgo_kbd_open(&kb, "SEARCH", NULL, PURRGO_KBD_LAYOUT_EN);

    /* Первый кадр рисует всё и не регистрирует областей (обновление экрана целиком). */
    pc_display_stats_reset();
    ui_keyboard_reset();
    ui_render_keyboard(&g, &kb);
    display_flush();
    st = pc_display_stats_get();
    CHECK(st.partials == 0 && st.fulls == 0);

    /* Сетка и поле ввода нарисованы, свободная область между ними пуста. */
    CHECK(fb_black_pixels(0, UI_KEYBOARD_GRID_TOP_PX, W, PURRGO_KBD_ROWS * PURRGO_KBD_ROW_PITCH) > 500);
    CHECK(fb_black_pixels(0, UI_KEYBOARD_FREE_TOP_PX, W,
                          UI_KEYBOARD_FREE_BOTTOM_PX - UI_KEYBOARD_FREE_TOP_PX) == 0);
    /* Ничего не выходит за пределы кадра: нижняя полоса ниже подсказок пуста. */
    CHECK(fb_black_pixels(0, 262, W, H - 262) == 0);

    /* Движение курсора: обновляются только две клавиши. */
    act(&kb, PURRGO_KBD_ACT_RIGHT);
    ui_render_keyboard(&g, &kb);
    display_flush();
    st = pc_display_stats_get();
    CHECK(st.partials == 1 && st.has_region);
    CHECK(st.last_y >= UI_KEYBOARD_GRID_TOP_PX && st.last_h <= PURRGO_KBD_ROW_PITCH);
    CHECK(st.last_w <= 2 * PURRGO_KBD_KEY_W);

    /* Ввод символа: только поле ввода. */
    pc_display_stats_reset();
    act(&kb, PURRGO_KBD_ACT_PRESS);
    ui_render_keyboard(&g, &kb);
    display_flush();
    st = pc_display_stats_get();
    CHECK(st.partials == 1 && st.last_y == 14 && st.last_h == 18 && st.last_w == W);

    /* Смена Shift: строка состояния и сетка (область от верха до низа сетки). */
    pc_display_stats_reset();
    press_kind(&kb, PURRGO_KBD_KEY_SHIFT);
    ui_render_keyboard(&g, &kb);
    display_flush();
    st = pc_display_stats_get();
    CHECK(st.partials == 1 && st.last_y == 0);
    CHECK(st.last_y + st.last_h >= UI_KEYBOARD_GRID_TOP_PX + PURRGO_KBD_ROWS * PURRGO_KBD_ROW_PITCH);

    /* Без изменений - без обновления. */
    pc_display_stats_reset();
    ui_render_keyboard(&g, &kb);
    display_flush();
    st = pc_display_stats_get();
    CHECK(st.partials == 0 && st.fulls == 0);

    /* Длинный текст и все раскладки рисуются без выхода за кадр (ASan/UBSan в run.sh). */
    purrgo_kbd_open(&kb, "", "0123456789012345678901234567890123456789", PURRGO_KBD_LAYOUT_RU);
    ui_keyboard_reset();
    ui_render_keyboard(&g, &kb);
    press_kind(&kb, PURRGO_KBD_KEY_SYM);
    ui_render_keyboard(&g, &kb);
    ui_render_keyboard(NULL, &kb);
    ui_render_keyboard(&g, NULL);
}

int main(void)
{
    test_layout_tables();
    test_typing_en();
    test_typing_ru();
    test_layout_switching();
    test_cursor_movement();
    test_buttons_and_result();
    test_text_limits();
    test_render_and_refresh();
    return test_finish("test_keyboard");
}
