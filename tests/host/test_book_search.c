/*
 * Host-тесты поиска по книге (src/core/book_search.c, поиск в book_reader.c,
 * ввод запроса на экранной клавиатуре, экран ui_book.c):
 *   - сопоставление: регистр, Е/Ё, фраза через строки и отступы, перенос "-", граница
 *     страницы, границы поиска, устойчивость к произвольным данным (ASan);
 *   - читалка: вперёд/назад по совпадениям с переходом через конец книги, поиск с текущей
 *     страницы, несколько совпадений на странице, картинки пропускаются, не найдено,
 *     отмена ввода, пустой запрос, раскладка клавиатуры по тексту страницы;
 *   - поиск порциями (прогресс, остановка), книга больше RAM-буфера (окно с SD вперёд и назад);
 *   - экран: клавиатура, строка статуса, инверсная подсветка найденного.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "purrgo/app_fsm.h"
#include "purrgo/app_ui.h"
#include "purrgo/book_reader.h"
#include "purrgo/book_search.h"
#include "purrgo/config.h"
#include "purrgo/display_hal.h"
#include "purrgo/font5x7.h"
#include "purrgo/keyboard.h"
#include "purrgo/pc_display_stats.h"
#include "purrgo/pgb.h"
#include "purrgo/ui/ui_dirty.h"
#include "purrgo/pgo_text.h"
#include "purrgo/track_logger.h"
#include "ui_book.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

/* ---- Сопоставление текста ---------------------------------------------------- */

static bool find1(const char *text, const char *query, size_t *s, size_t *e)
{
    uint8_t pat[PURRGO_BOOK_SEARCH_MAX + 1];
    size_t m = purrgo_book_search_prepare(query, pat);
    return purrgo_book_search_find((const uint8_t *)text, strlen(text), 0, strlen(text), pat, m, s, e);
}

static void test_fold_prepare(void)
{
    CHECK(purrgo_book_search_fold('A') == 'a' && purrgo_book_search_fold('z') == 'z');
    CHECK(purrgo_book_search_fold('5') == '5' && purrgo_book_search_fold('-') == '-');
    CHECK(purrgo_book_search_fold(0x80) == 0xA0 && purrgo_book_search_fold(0x9F) == 0xBF);
    CHECK(purrgo_book_search_fold(0xA0) == 0xA0);
    CHECK(purrgo_book_search_fold(0xF0) == 0xA5 && purrgo_book_search_fold(0xF1) == 0xA5);
    CHECK(purrgo_book_search_fold(0xA5) == 0xA5);                       /* е */
    CHECK(purrgo_book_search_fold(0xC0) == 0xD0 && purrgo_book_search_fold(0xCF) == 0xDF);
    CHECK(purrgo_book_search_fold(0xD0) == 0xD0 && purrgo_book_search_fold(0xE0) == 0xE0);

    uint8_t o[PURRGO_BOOK_SEARCH_MAX + 1];
    CHECK(purrgo_book_search_prepare("  Hello   World  ", o) == 11 && strcmp((char *)o, "hello world") == 0);
    CHECK(purrgo_book_search_prepare("   ", o) == 0 && o[0] == 0);
    CHECK(purrgo_book_search_prepare("", o) == 0);
    CHECK(purrgo_book_search_prepare(NULL, o) == 0);
    CHECK(purrgo_book_search_prepare("a\tb", o) == 3 && o[1] == ' ');

    char longq[100];
    memset(longq, 'x', sizeof longq - 1); longq[sizeof longq - 1] = 0;
    CHECK(purrgo_book_search_prepare(longq, o) == PURRGO_BOOK_SEARCH_MAX && o[PURRGO_BOOK_SEARCH_MAX] == 0);
}

static void test_matching(void)
{
    size_t s, e;

    CHECK(find1("The Quick Fox", "quick", &s, &e) && s == 4 && e == 9);
    CHECK(find1("The Quick Fox", "FOX", &s, &e) && s == 10 && e == 13);
    CHECK(!find1("The Quick Fox", "dog", &s, &e));
    CHECK(!find1("abc", "abcd", &s, &e));                /* запрос длиннее текста */
    CHECK(!find1("abc", "   ", &s, &e));                 /* пустой запрос */

    /* Е = Ё в обе стороны: "ёжик" (F1 A6 A8 AA) и "ЕЖИК". */
    CHECK(find1("\xF1\xA6\xA8\xAA", "\x85\xA6\xA8\xAA", &s, &e) && s == 0 && e == 4);
    CHECK(find1("\x85\xA6\xA8\xAA", "\xF1\xA6", &s, &e) && s == 0 && e == 2);
    CHECK(find1("\x80\x81\x82", "\xA0\xA1\xA2", &s, &e));  /* АБВ ~ абв */

    /* Фраза через конец строки и отступ следующей. */
    CHECK(find1("he said that the\nold man was right", "the old man", &s, &e) && s == 13 && e == 24);
    CHECK(find1("word\n  Next", "word next", &s, &e) && s == 0 && e == 11);
    CHECK(find1("a  b", "a b", &s, &e));                  /* серия пробелов в тексте */
    CHECK(!find1("ab", "a b", &s, &e));                   /* пробела в тексте нет */
    CHECK(!find1("a-b", "a b", &s, &e));

    /* Перенос слова: дефис и LF. */
    CHECK(find1("an ex-\nample here", "example", &s, &e) && s == 3 && e == 12);
    CHECK(find1("an ex-\nample here", "ex-ample", &s, &e) && s == 3 && e == 12);
    CHECK(find1("an ex-\nample here", "ex-", &s, &e) && s == 3 && e == 6);
    CHECK(find1("ko-\ne-\nchto", "ko-e-chto", &s, &e));
    CHECK(!find1("-\nx", "x", &s, &e) || s == 2);          /* начало не на переносе */
    CHECK(find1("a-b", "a-b", &s, &e) && e == 3);
    CHECK(!find1("a-b", "ab", &s, &e));                    /* дефис не в конце строки - не перенос */

    /* Границы начала: lo/hi. */
    const uint8_t *t = (const uint8_t *)"fox fox fox";
    uint8_t pat[PURRGO_BOOK_SEARCH_MAX + 1];
    size_t m = purrgo_book_search_prepare("fox", pat);
    CHECK(purrgo_book_search_find(t, 11, 1, 11, pat, m, &s, &e) && s == 4);
    CHECK(purrgo_book_search_find(t, 11, 5, 11, pat, m, &s, &e) && s == 8);
    CHECK(!purrgo_book_search_find(t, 11, 9, 11, pat, m, &s, &e));
    CHECK(purrgo_book_search_find(t, 11, 0, 1, pat, m, &s, &e) && s == 0);
    CHECK(purrgo_book_search_find_last(t, 11, 0, 11, pat, m, &s, &e) && s == 8 && e == 11);
    CHECK(purrgo_book_search_find_last(t, 11, 0, 8, pat, m, &s, &e) && s == 4);
    CHECK(purrgo_book_search_find_last(t, 11, 0, 4, pat, m, &s, &e) && s == 0);
    CHECK(!purrgo_book_search_find_last(t, 11, 1, 4, pat, m, &s, &e));
    /* Начало в [lo,hi), но совпадение выходит за hi - допустимо; за text_len - нет. */
    CHECK(purrgo_book_search_find(t, 11, 8, 9, pat, m, &s, &e) && s == 8 && e == 11);
    CHECK(!purrgo_book_search_find(t, 10, 8, 10, pat, m, &s, &e));
    /* Пересекающиеся совпадения. */
    m = purrgo_book_search_prepare("aa", pat);
    CHECK(purrgo_book_search_find((const uint8_t *)"aaa", 3, 1, 3, pat, m, &s, &e) && s == 1);

    /* NULL и неверные аргументы. */
    CHECK(!purrgo_book_search_find(NULL, 0, 0, 0, pat, m, &s, &e));
    CHECK(!purrgo_book_search_find(t, 11, 0, 11, NULL, 0, &s, &e));
    CHECK(!purrgo_book_search_find(t, 11, 0, 11, pat, 0, &s, &e));
    CHECK(purrgo_book_search_find(t, 11, 0, 11, pat, m, NULL, NULL) || 1);
    const uint8_t sp[] = " a";
    CHECK(!purrgo_book_search_find(t, 11, 0, 11, sp, 2, &s, &e));   /* запрос не начинается с пробела */
}

static void test_fuzz(void)
{
    srand(12345);
    for (int it = 0; it < 20000; it++) {
        uint8_t text[96], pat[PURRGO_BOOK_SEARCH_MAX + 1];
        size_t n = (size_t)(rand() % 96);
        const char alpha[] = "ab -\n\xA0\xF1\x85";
        for (size_t i = 0; i < n; i++) { text[i] = (uint8_t)alpha[rand() % 8]; }
        size_t m = (size_t)(rand() % 12);
        for (size_t i = 0; i < m; i++) { pat[i] = (uint8_t)alpha[rand() % 8]; }
        pat[m] = 0;
        size_t lo = (size_t)(rand() % 100), hi = (size_t)(rand() % 120), s, e;
        bool a = purrgo_book_search_find(text, n, lo, hi, pat, m, &s, &e);
        if (a) { CHECK(s >= lo && s < hi && e > s && e <= n); }
        bool b = purrgo_book_search_find_last(text, n, lo, hi, pat, m, &s, &e);
        if (b) { CHECK(s >= lo && s < hi && e > s && e <= n); }
        CHECK(a == b);                                      /* есть совпадение вперёд - есть и назад */
    }
}

/* ---- Книги --------------------------------------------------------------------- */

#define ENTRY 12u
#define MAXP 600
static const char *g_text[MAXP];      /* NULL - заполнитель */
static uint8_t g_img[MAXP];
static uint8_t g_book[512 * 1024];

static void wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t *p, uint32_t v) { wr16(p, v & 0xFFFFu); wr16(p + 2, v >> 16); }

static size_t page_text(uint32_t i, uint8_t *out)
{
    char buf[256];
    if (g_text[i] != NULL) {
        snprintf(buf, sizeof buf, "%s", g_text[i]);
    } else {
        snprintf(buf, sizeof buf, "Page %u first line\nFiller text number %u\nLast row %u", i, i, i);
    }
    memcpy(out, buf, strlen(buf));
    return strlen(buf);
}

static void clear_book(void)
{
    memset(g_text, 0, sizeof g_text);
    memset(g_img, 0, sizeof g_img);
}

static uint32_t build_book(uint32_t pages)
{
    const char *title = "Search", *author = "Test";
    const uint32_t tl = (uint32_t)strlen(title), al = (uint32_t)strlen(author);
    const uint32_t index_off = 64u + tl + al, data_off = index_off + ENTRY * pages;
    uint32_t pos = data_off;

    memset(g_book, 0, 64);
    memcpy(g_book + 64, title, tl);
    memcpy(g_book + 64 + tl, author, al);
    for (uint32_t i = 0; i < pages; i++) {
        size_t len;
        uint32_t w = 0, h = 0;
        if (g_img[i]) {
            w = 16; h = 4; len = 8;
            for (size_t k = 0; k < len; k++) { g_book[pos + k] = (uint8_t)(0xA5u ^ k); }
        } else {
            len = page_text(i, g_book + pos);
        }
        wr32(g_book + index_off + ENTRY * i, pos);
        wr16(g_book + index_off + ENTRY * i + 4, (uint32_t)len);
        wr16(g_book + index_off + ENTRY * i + 6, (uint32_t)len);
        wr16(g_book + index_off + ENTRY * i + 8, w);
        wr16(g_book + index_off + ENTRY * i + 10, h);
        pos += (uint32_t)len;
    }
    memcpy(g_book, "PGB1", 4);
    wr16(g_book + 4, 1); wr16(g_book + 6, 1); wr16(g_book + 8, 64);
    wr32(g_book + 12, pages); wr32(g_book + 16, 64); wr32(g_book + 20, index_off);
    wr32(g_book + 24, data_off); wr32(g_book + 28, pos);
    g_book[40] = 29; g_book[41] = 25;
    wr16(g_book + 44, tl); wr16(g_book + 46, al);
    wr16(g_book + 48, 176); wr16(g_book + 50, 256);
    pgb_crc_t c;
    pgb_crc_init(&c); pgb_crc_feed(&c, g_book, data_off);
    wr32(g_book + 36, pgb_crc_value(&c));
    pgb_crc_init(&c); pgb_crc_feed(&c, g_book, pos);
    wr32(g_book + 32, pgb_crc_value(&c));
    return pos;
}

static void reset_all(void)
{
    fake_fs_reset();
    fake_power_reset();
    purrgo_logger_reset();
    purrgo_book_reader_reset();
    purrgo_book_reader_set_ram(NULL, 0);
    purrgo_config_init();
    clear_book();
    g_now = 100000;
}

static purrgo_state_t open_book(uint32_t pages)
{
    fake_fs_put_bin("books/ALPHA.PGB", g_book, build_book(pages));
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");
    app_config.book_crc32 = 0;
    purrgo_state_t st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_READ);
    return st;
}

static purrgo_state_t press(purrgo_state_t st, purrgo_btn_t b, bool *redraw)
{
    purrgo_state_t next = st;
    bool r = purrgo_book_reader_handle_button(st, b, &next);
    if (redraw) { *redraw = r; }
    return next;
}

/* ---- Ввод на клавиатуре кнопками -------------------------------------------------- */

static bool find_key(const purrgo_keyboard_t *kb, purrgo_kbd_layout_t lay, uint8_t ch, int *r, int *c)
{
    for (int i = 0; i < PURRGO_KBD_ROWS; i++) {
        for (int j = 0; j < purrgo_kbd_row_len(lay, i); j++) {
            const purrgo_kbd_key_t *k = purrgo_kbd_key(lay, i, j);
            if (k->kind != PURRGO_KBD_KEY_LETTER && k->kind != PURRGO_KBD_KEY_CHAR &&
                !(k->kind == PURRGO_KBD_KEY_SPACE && ch == ' ')) { continue; }
            if (purrgo_kbd_key_char(kb, k) == ch) { *r = i; *c = j; return true; }
        }
    }
    return false;
}

static void kbd_click(purrgo_state_t *st, int r, int c)
{
    const purrgo_keyboard_t *kb = purrgo_book_reader_get_keyboard();
    for (int g = 0; kb->row != r && g < 12; g++) { *st = press(*st, PURRGO_BTN_KEY4_SHORT, NULL); }
    for (int g = 0; kb->col != c && g < 24; g++) { *st = press(*st, PURRGO_BTN_KEY2_SHORT, NULL); }
    *st = press(*st, PURRGO_BTN_KEY3_SHORT, NULL);
}

static bool type_char(purrgo_state_t *st, uint8_t ch)
{
    const purrgo_keyboard_t *kb = purrgo_book_reader_get_keyboard();
    int r, c;
    if (find_key(kb, (purrgo_kbd_layout_t)kb->layout, ch, &r, &c)) { kbd_click(st, r, c); return true; }
    /* Знак - в SYM: войти, нажать, вернуться. */
    for (int i = 0; i < PURRGO_KBD_ROWS; i++) {
        for (int j = 0; j < purrgo_kbd_row_len((purrgo_kbd_layout_t)kb->layout, i); j++) {
            if (purrgo_kbd_key((purrgo_kbd_layout_t)kb->layout, i, j)->kind == PURRGO_KBD_KEY_SYM) {
                kbd_click(st, i, j);
                goto in_sym;
            }
        }
    }
    return false;
in_sym:;
    bool ok = find_key(kb, PURRGO_KBD_LAYOUT_SYM, ch, &r, &c);
    if (ok) { kbd_click(st, r, c); }
    for (int i = 0; i < PURRGO_KBD_ROWS; i++) {
        for (int j = 0; j < purrgo_kbd_row_len(PURRGO_KBD_LAYOUT_SYM, i); j++) {
            if (purrgo_kbd_key(PURRGO_KBD_LAYOUT_SYM, i, j)->kind == PURRGO_KBD_KEY_SYM) { kbd_click(st, i, j); return ok; }
        }
    }
    return ok;
}

static void type_str(purrgo_state_t *st, const char *pgo)
{
    for (; *pgo; pgo++) { CHECK(type_char(st, (uint8_t)*pgo)); }
}

static purrgo_book_view_t view(void)
{
    purrgo_book_view_t v;
    purrgo_book_reader_get_view(&v);
    return v;
}

/* Открыть клавиатуру, набрать запрос (PurrGO-256) и принять. */
static purrgo_state_t search(purrgo_state_t st, const char *pgo)
{
    /* Вне поиска клавиатуру открывает K4 длинное, в режиме поиска - K3. */
    st = press(st, (view().find == PURRGO_BOOK_FIND_OFF) ? PURRGO_BTN_KEY4_LONG : PURRGO_BTN_KEY3_SHORT, NULL);
    CHECK(st == APP_STATE_BOOK_SEARCH_INPUT);
    while (purrgo_kbd_len(purrgo_book_reader_get_keyboard()) > 0) { st = press(st, PURRGO_BTN_KEY1_LONG, NULL); }
    type_str(&st, pgo);
    return press(st, PURRGO_BTN_KEY3_LONG, NULL);
}

static purrgo_state_t search_ru(purrgo_state_t st, const char *utf8)
{
    char b[64];
    purrgo_utf8_to_pgo(utf8, b, sizeof b);
    return search(st, b);
}

/* Подсвечен ровно текст `what` (PurrGO-256) на странице. */
static bool highlighted(const char *what)
{
    purrgo_book_view_t v = view();
    return v.find == PURRGO_BOOK_FIND_FOUND && v.text != NULL && v.hl_len == strlen(what) &&
           v.hl_start + v.hl_len <= v.text_len && memcmp(v.text + v.hl_start, what, v.hl_len) == 0;
}

/* ---- Тесты читалки ------------------------------------------------------------------ */

static void test_forward_backward(void)
{
    reset_all();
    g_text[7] = "the quick brown fox\njumps";
    g_text[20] = "a Fox is here";
    purrgo_state_t st = open_book(40);

    st = search(st, "fox");
    CHECK(st == APP_STATE_BOOK_READ);
    CHECK(view().page == 7 && view().find == PURRGO_BOOK_FIND_FOUND);
    CHECK(highlighted("fox"));
    CHECK(app_config.book_page == 7);

    bool redraw = false;
    st = press(st, PURRGO_BTN_KEY2_SHORT, &redraw);          /* следующее */
    CHECK(redraw && view().page == 20 && highlighted("Fox"));
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);             /* через конец книги */
    CHECK(view().page == 7 && highlighted("fox"));
    st = press(st, PURRGO_BTN_KEY1_SHORT, NULL);             /* предыдущее: через начало */
    CHECK(view().page == 20 && highlighted("Fox"));
    st = press(st, PURRGO_BTN_KEY1_SHORT, NULL);
    CHECK(view().page == 7);
    st = press(st, PURRGO_BTN_KEY2_LONG, NULL);              /* длинное - то же, что короткое */
    CHECK(view().page == 20);

    /* K4: закончить поиск, остаёмся на странице; повторное K4 - выход в меню. */
    st = press(st, PURRGO_BTN_KEY4_SHORT, &redraw);
    CHECK(redraw && st == APP_STATE_BOOK_READ && view().find == PURRGO_BOOK_FIND_OFF && view().hl_len == 0);
    CHECK(view().page == 20);
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);             /* снова обычные страницы */
    CHECK(view().page == 21);
    st = press(st, PURRGO_BTN_KEY4_SHORT, NULL);
    CHECK(st == APP_STATE_MENU_MAIN);
}

static void test_current_page_and_same_page(void)
{
    reset_all();
    g_text[3] = "fox here";
    g_text[20] = "fox and\nanother fox";
    g_text[30] = "fox";
    purrgo_state_t st = open_book(40);
    for (int i = 0; i < 20; i++) { st = press(st, PURRGO_BTN_KEY2_SHORT, NULL); }
    CHECK(view().page == 20);

    st = search(st, "fox");                                  /* с начала текущей страницы */
    CHECK(view().page == 20 && view().hl_start == 0 && highlighted("fox"));
    uint32_t first = view().hl_start;
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);             /* второе совпадение той же страницы */
    CHECK(view().page == 20 && view().hl_start > first && highlighted("fox"));
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);
    CHECK(view().page == 30);
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);             /* вокруг книги: страница 3 */
    CHECK(view().page == 3);
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);
    CHECK(view().page == 20 && view().hl_start == 0);
    st = press(st, PURRGO_BTN_KEY1_SHORT, NULL);             /* назад: страница 3 */
    CHECK(view().page == 3);
    st = press(st, PURRGO_BTN_KEY1_SHORT, NULL);             /* назад через начало: 30 */
    CHECK(view().page == 30);
    st = press(st, PURRGO_BTN_KEY1_SHORT, NULL);             /* назад: второе совпадение на 20 */
    CHECK(view().page == 20 && view().hl_start > 0);
    st = press(st, PURRGO_BTN_KEY1_SHORT, NULL);
    CHECK(view().page == 20 && view().hl_start == 0);
    (void)st;
}

static void test_phrases(void)
{
    reset_all();
    g_text[2] = "He said that the\nold man was right";
    g_text[3] = "...and the quick";
    g_text[4] = "brown dog";
    g_text[5] = "an ex-\nample of it";
    g_text[6] = "THE END";
    g_text[7] = "word\n  Next";
    g_img[8] = 1;
    g_text[9] = "image page skipped fox";
    purrgo_state_t st = open_book(12);

    st = search(st, "the old man");
    CHECK(view().page == 2 && highlighted("the\nold man"));
    st = search(st, "quick brown");                          /* через границу страниц */
    CHECK(view().page == 3 && highlighted("quick"));
    st = search(st, "example");
    CHECK(view().page == 5 && highlighted("ex-\nample"));
    st = search(st, "the end");
    CHECK(view().page == 6 && highlighted("THE END"));
    st = search(st, "word next");
    CHECK(view().page == 7 && highlighted("word\n  Next"));
    /* Знаки через SYM: дефис. */
    st = search(st, "ex-ample");
    CHECK(view().page == 5 && highlighted("ex-\nample"));
    /* Пробел в начале и конце запроса не мешает. */
    st = search(st, " fox ");
    CHECK(view().page == 9 && highlighted("fox"));
    (void)st;
}

static void test_images_skipped_and_layout(void)
{
    reset_all();
    g_img[0] = 1;
    g_img[1] = 1;
    g_text[2] = "\x8F\x80\x8B\x8E\x8D";   /* "ПАЛОН" */
    purrgo_state_t st = open_book(4);
    CHECK(st == APP_STATE_BOOK_READ && view().image != NULL);
    st = press(st, PURRGO_BTN_KEY4_LONG, NULL);
    CHECK(st == APP_STATE_BOOK_SEARCH_INPUT);
    CHECK(purrgo_book_reader_get_keyboard()->layout == PURRGO_KBD_LAYOUT_RU);   /* нет латиницы - RU */
    st = press(st, PURRGO_BTN_KEY4_LONG, NULL);
    CHECK(st == APP_STATE_BOOK_READ);
    st = search_ru(st, "палон");
    CHECK(view().page == 2 && view().find == PURRGO_BOOK_FIND_FOUND);

    /* Ё/е: страница с "ёжик", запрос "ежик" и наоборот. */
    reset_all();
    g_text[1] = "\xF1\xA6\xA8\xAA \x80\x85\xA6";
    st = open_book(3);
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);               /* страница с кириллицей: клавиатура RU */
    st = search_ru(st, "ежик");
    CHECK(view().page == 1 && view().hl_start == 0 && view().hl_len == 4);
    st = search_ru(st, "ёж");
    CHECK(view().page == 1 && view().hl_len == 2);
    st = search_ru(st, "аеж");                                /* "АЕЖ" в конце */
    CHECK(view().page == 1 && view().hl_start == 5);

    /* Страница на латинице - клавиатура сразу EN. */
    reset_all();
    g_text[0] = "Plain english text";
    st = open_book(2);
    st = press(st, PURRGO_BTN_KEY4_LONG, NULL);
    CHECK(purrgo_book_reader_get_keyboard()->layout == PURRGO_KBD_LAYOUT_EN);
    (void)press(st, PURRGO_BTN_KEY4_LONG, NULL);
}

static void test_not_found_cancel_empty(void)
{
    reset_all();
    g_text[5] = "needle";
    purrgo_state_t st = open_book(10);
    bool redraw = true;

    st = search(st, "zebra");
    CHECK(st == APP_STATE_BOOK_READ && view().find == PURRGO_BOOK_FIND_NOT_FOUND && view().page == 0);
    CHECK(view().hl_len == 0);
    st = press(st, PURRGO_BTN_KEY1_SHORT, &redraw);
    CHECK(!redraw && view().page == 0);                       /* листать нельзя */
    st = press(st, PURRGO_BTN_KEY2_SHORT, &redraw);
    CHECK(!redraw && view().page == 0);

    /* K3: клавиатура с прежним запросом; правка и повторный поиск. */
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    CHECK(st == APP_STATE_BOOK_SEARCH_INPUT && strcmp(purrgo_kbd_text(purrgo_book_reader_get_keyboard()), "zebra") == 0);
    for (int i = 0; i < 5; i++) { st = press(st, PURRGO_BTN_KEY1_LONG, NULL); }   /* стереть */
    CHECK(purrgo_kbd_len(purrgo_book_reader_get_keyboard()) == 0);
    type_str(&st, "needle");
    st = press(st, PURRGO_BTN_KEY3_LONG, NULL);
    CHECK(view().find == PURRGO_BOOK_FIND_FOUND && view().page == 5);

    /* Отмена ввода: режим и совпадение сохраняются. */
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    CHECK(st == APP_STATE_BOOK_SEARCH_INPUT && strcmp(purrgo_kbd_text(purrgo_book_reader_get_keyboard()), "needle") == 0);
    st = press(st, PURRGO_BTN_KEY4_LONG, NULL);
    CHECK(st == APP_STATE_BOOK_READ && view().find == PURRGO_BOOK_FIND_FOUND && highlighted("needle"));

    /* Пустой запрос из режима чтения: ничего не происходит. */
    st = press(st, PURRGO_BTN_KEY4_SHORT, NULL);              /* закончить поиск */
    st = press(st, PURRGO_BTN_KEY4_LONG, NULL);
    for (int i = 0; i < 8; i++) { st = press(st, PURRGO_BTN_KEY1_LONG, NULL); }
    st = press(st, PURRGO_BTN_KEY3_LONG, NULL);
    CHECK(st == APP_STATE_BOOK_READ && view().find == PURRGO_BOOK_FIND_OFF && view().page == 5);

    /* K4 в NOT FOUND закрывает сообщение. */
    st = search(st, "zzz");
    CHECK(view().find == PURRGO_BOOK_FIND_NOT_FOUND);
    st = press(st, PURRGO_BTN_KEY4_SHORT, NULL);
    CHECK(view().find == PURRGO_BOOK_FIND_OFF && st == APP_STATE_BOOK_READ);

    /* K3 длинное в режиме поиска - список; повторное открытие без поиска. */
    st = search(st, "needle");
    st = press(st, PURRGO_BTN_KEY3_LONG, NULL);
    CHECK(st == APP_STATE_BOOK_LIST);
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    CHECK(st == APP_STATE_BOOK_READ && view().find == PURRGO_BOOK_FIND_OFF && view().hl_len == 0);
}

static void test_progress_and_stop(void)
{
    reset_all();
    g_text[480] = "far away zebra";
    purrgo_state_t st = open_book(500);
    bool redraw = false;

    st = search(st, "zebra");
    CHECK(view().find == PURRGO_BOOK_FIND_RUNNING && view().page == 0);
    CHECK(view().find_percent > 10 && view().find_percent < 25);   /* 96 из 501 страниц */

    /* Пока идёт, листать нельзя, K4 останавливает. */
    st = press(st, PURRGO_BTN_KEY2_SHORT, &redraw);
    CHECK(!redraw && view().page == 0 && view().find == PURRGO_BOOK_FIND_RUNNING);

    /* Прогресс перерисовывается не чаще раза в секунду. */
    int redraws = 0;
    for (int i = 0; i < 4; i++) {
        g_now += 10;
        if (purrgo_book_reader_tick(g_now)) { redraws++; }
        if (view().find != PURRGO_BOOK_FIND_RUNNING) { break; }
    }
    CHECK(redraws == 0 || view().find != PURRGO_BOOK_FIND_RUNNING);
    g_now += 2000;
    {
        /* к этому моменту страницы просмотрены; результат - после завершения */
        int guard = 0;
        bool done_redraw = false;
        while (view().find == PURRGO_BOOK_FIND_RUNNING && guard++ < 100) {
            g_now += 1500;
            if (purrgo_book_reader_tick(g_now)) { done_redraw = true; }
        }
        CHECK(done_redraw);
    }
    CHECK(view().find == PURRGO_BOOK_FIND_FOUND && view().page == 480 && highlighted("zebra"));

    /* Остановка: новый поиск, K4 до завершения. */
    st = press(st, PURRGO_BTN_KEY4_SHORT, NULL);
    for (int i = 0; i < 5; i++) { st = press(st, PURRGO_BTN_KEY1_SHORT, NULL); }
    CHECK(view().page == 475);
    st = search(st, "zebra");
    CHECK(view().find == PURRGO_BOOK_FIND_FOUND && view().page == 480);   /* близко: за один шаг */
    st = press(st, PURRGO_BTN_KEY4_SHORT, NULL);
    for (int i = 0; i < 200; i++) { st = press(st, PURRGO_BTN_KEY1_SHORT, NULL); }
    st = press(st, PURRGO_BTN_KEY1_LONG, NULL);
    CHECK(view().page < 300);
    st = search(st, "zebra");
    CHECK(view().find == PURRGO_BOOK_FIND_RUNNING);
    uint32_t page = view().page;
    st = press(st, PURRGO_BTN_KEY4_SHORT, &redraw);
    CHECK(redraw && st == APP_STATE_BOOK_READ && view().find == PURRGO_BOOK_FIND_OFF && view().page == page);
    CHECK(!purrgo_book_reader_tick(g_now + 5000));            /* поиск не продолжается */

    /* Нет совпадений во всей книге: NOT FOUND после последней порции, tick сообщает о перерисовке. */
    st = search(st, "qqq");
    int guard = 0;
    bool nf_redraw = (view().find == PURRGO_BOOK_FIND_NOT_FOUND);
    while (view().find == PURRGO_BOOK_FIND_RUNNING && guard++ < 100) {
        g_now += 10;
        if (purrgo_book_reader_tick(g_now)) { nf_redraw = true; }
    }
    CHECK(nf_redraw && view().find == PURRGO_BOOK_FIND_NOT_FOUND);
}

static void test_windowed(void)
{
    reset_all();
    static uint8_t small[10 * 1024];
    purrgo_book_reader_set_ram(small, sizeof small);
    g_text[250] = "deep zebra here";
    g_text[40] = "early ZEBRA";
    for (int i = 0; i < 400; i++) { if (!g_text[i]) { /* заполнитель подлиннее обычного */ } }
    uint32_t size = build_book(400);
    CHECK(size > 2u * sizeof small);
    fake_fs_put_bin("books/ALPHA.PGB", g_book, size);
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");
    purrgo_state_t st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_READ && fake_fs_open_handles() == 1);

    st = search(st, "zebra");
    int guard = 0;
    while (view().find == PURRGO_BOOK_FIND_RUNNING && guard++ < 200) { g_now += 10; purrgo_book_reader_tick(g_now); }
    CHECK(view().page == 40 && highlighted("ZEBRA"));
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);
    guard = 0;
    while (view().find == PURRGO_BOOK_FIND_RUNNING && guard++ < 200) { g_now += 10; purrgo_book_reader_tick(g_now); }
    CHECK(view().page == 250 && highlighted("zebra"));

    /* Назад по окнам: 250 -> 40 (209 страниц) за небольшое число чтений SD. */
    fake_fs_reset_read_stats();
    st = press(st, PURRGO_BTN_KEY1_SHORT, NULL);
    guard = 0;
    while (view().find == PURRGO_BOOK_FIND_RUNNING && guard++ < 200) { g_now += 10; purrgo_book_reader_tick(g_now); }
    CHECK(view().page == 40 && highlighted("ZEBRA"));
    CHECK(fake_fs_read_calls() > 0 && fake_fs_read_calls() < 40);

    /* После поиска обычное перелистывание вперёд и назад работает. */
    st = press(st, PURRGO_BTN_KEY4_SHORT, NULL);
    for (int i = 0; i < 30; i++) { st = press(st, PURRGO_BTN_KEY2_SHORT, NULL); }
    CHECK(view().page == 70 && view().text != NULL && view().text_len > 0);
    for (int i = 0; i < 60; i++) { st = press(st, PURRGO_BTN_KEY1_SHORT, NULL); }
    CHECK(view().page == 10 && view().text != NULL);
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);
    CHECK(fake_fs_open_handles() == 0);
    purrgo_book_reader_set_ram(NULL, 0);
}

/* ---- Экран --------------------------------------------------------------------------- */

#define W 176
#define H 264
static uint8_t g_fb[W * H];
static void cb_draw(void *fb, int16_t x, int16_t y, gfx_color_t c)
{
    (void)fb;
    if (x >= 0 && x < W && y >= 0 && y < H) { g_fb[y * W + x] = (uint8_t)(c + 1); }
}
static gfx_color_t cb_read(void *fb, int16_t x, int16_t y)
{
    (void)fb;
    return (x >= 0 && x < W && y >= 0 && y < H) ? (gfx_color_t)(g_fb[y * W + x] - 1) : 0;
}
static int px(int x, int y) { return g_fb[y * W + x] - 1; }

static void render(gfx_context_t *g)
{
    purrgo_app_ui_render(g, purrgo_app_get_gnss_solution(), NULL);
}

static bool status_is(int cx, const char *str)
{
    for (int i = 0; str[i]; i++) {
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 7; row++) {
                const int want = ((font5x7[(unsigned char)str[i]][col] >> row) & 1) ? WHITE : BLACK;
                if (px(1 + (cx + i) * 6 + col, row) != want) { return false; }
            }
        }
    }
    return true;
}

/* Символ в ячейке (cx, cy) страницы: инвертированный (белый на чёрном) или обычный. */
static bool cell_is(int cx, int cy, unsigned char ch, bool inverted)
{
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            const bool on = (font5x7[ch][col] >> row) & 1;
            const int want = (on != inverted) ? BLACK : WHITE;
            if (px(1 + cx * 6 + col, (int)PGB_TEXT_TOP_PX + cy * (int)PGB_LINE_PITCH_PX + row) != want) { return false; }
        }
    }
    return true;
}

static void test_screen(void)
{
    reset_all();
    gfx_context_t g;
    memset(g_fb, 0, sizeof g_fb);
    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));
    g_text[6] = "a quick brown fox\nsecond fox";
    fake_fs_put_bin("books/ALPHA.PGB", g_book, build_book(10));
    memset(&app_config, 0, sizeof app_config);
    purrgo_app_init();
    purrgo_app_ui_reset();
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");
    for (int i = 0; i < 20; i++) { g_now += 100; purrgo_app_tick(g_now); }

    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);          /* карта -> trip */
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);         /* CONFIG */
    purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);         /* BOOKS: последняя книга */
    CHECK(purrgo_app_get_state() == APP_STATE_BOOK_READ);
    render(&g);
    for (int i = 0; i < 6; i++) { purrgo_app_handle_button(PURRGO_BTN_KEY2_SHORT); }
    render(&g);

    /* Клавиатура: экран ввода, заголовок SEARCH, без общих подсказок кнопок. */
    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);
    CHECK(purrgo_app_get_state() == APP_STATE_BOOK_SEARCH_INPUT);
    CHECK(purrgo_ui_dirty_test(UI_DIRTY_SCREEN));
    pc_display_stats_reset();
    render(&g);
    CHECK(pc_display_stats_get().mode == DISPLAY_MODE_MONO && pc_display_stats_get().fulls == 1);
    {
        int black = 0;
        for (int y = 130; y < 240; y++) { for (int x = 0; x < W; x++) { black += px(x, y) == BLACK; } }
        CHECK(black > 200);                                    /* сетка клавиш нарисована */
    }
    /* Набор "fox" - тот же путь, что на устройстве: кнопки через FSM. */
    {
        purrgo_state_t st = APP_STATE_BOOK_SEARCH_INPUT;
        const purrgo_keyboard_t *kb = purrgo_book_reader_get_keyboard();
        for (const char *c = "fox"; *c; c++) {
            int r, k;
            CHECK(find_key(kb, (purrgo_kbd_layout_t)kb->layout, (uint8_t)*c, &r, &k));
            for (int i = 0; kb->row != r && i < 12; i++) { purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT); }
            for (int i = 0; kb->col != k && i < 24; i++) { purrgo_app_handle_button(PURRGO_BTN_KEY2_SHORT); }
            purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);
            render(&g);
        }
        (void)st;
    }
    CHECK(strcmp(purrgo_kbd_text(purrgo_book_reader_get_keyboard()), "fox") == 0);
    purrgo_app_handle_button(PURRGO_BTN_KEY3_LONG);
    CHECK(purrgo_app_get_state() == APP_STATE_BOOK_READ);
    pc_display_stats_reset();
    render(&g);
    CHECK(pc_display_stats_get().fulls == 1);                  /* смена экрана */
    CHECK(view().find == PURRGO_BOOK_FIND_FOUND && view().page == 6);

    /* Строка статуса и подсветка: "fox" - ячейки 14..16 первой строки, обычный текст не тронут. */
    CHECK(status_is(0, "FIND 7/10"));
    CHECK(status_is(18, "3:NEW 4:END"));
    CHECK(cell_is(0, 0, 'a', false) && cell_is(2, 0, 'q', false));
    CHECK(cell_is(14, 0, 'f', true) && cell_is(15, 0, 'o', true) && cell_is(16, 0, 'x', true));
    CHECK(cell_is(17, 0, 'x', true) == false);                  /* ячейка за совпадением не подсвечена */
    CHECK(px(1 + 14 * 6, (int)PGB_TEXT_TOP_PX - 1) == BLACK);    /* 1 px над подсвеченной ячейкой */
    CHECK(px(1 + 2 * 6, (int)PGB_TEXT_TOP_PX - 1) == WHITE);

    /* Следующее совпадение - вторая строка той же страницы: подсветка переезжает. */
    purrgo_app_handle_button(PURRGO_BTN_KEY2_SHORT);
    render(&g);
    CHECK(view().page == 6 && cell_is(7, 1, 'f', true) && cell_is(14, 0, 'f', false));
    CHECK(status_is(0, "FIND 7/10"));

    /* Нет в книге. */
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);
    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);
    {
        const purrgo_keyboard_t *kb = purrgo_book_reader_get_keyboard();
        for (int i = 0; kb->len > 0 && i < 8; i++) { purrgo_app_handle_button(PURRGO_BTN_KEY1_LONG); }
        int r, k;
        CHECK(find_key(kb, (purrgo_kbd_layout_t)kb->layout, 'z', &r, &k));
        for (int i = 0; kb->row != r && i < 12; i++) { purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT); }
        for (int i = 0; kb->col != k && i < 24; i++) { purrgo_app_handle_button(PURRGO_BTN_KEY2_SHORT); }
        purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);
    }
    purrgo_app_handle_button(PURRGO_BTN_KEY3_LONG);
    render(&g);
    CHECK(view().find == PURRGO_BOOK_FIND_NOT_FOUND);
    CHECK(status_is(0, "NOT FOUND  3:EDIT 4:OK"));
    CHECK(cell_is(14, 0, 'f', false));                          /* подсветки нет */

    /* Выход из режима: GNSS включается, состояние читалки не залипает. */
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);
    CHECK(purrgo_app_get_state() == APP_STATE_MENU_MAIN && !purrgo_book_reader_is_active());
}

int main(void)
{
    test_fold_prepare();
    test_matching();
    test_fuzz();
    test_forward_backward();
    test_current_page_and_same_page();
    test_phrases();
    test_images_skipped_and_layout();
    test_not_found_cancel_empty();
    test_progress_and_stop();
    test_windowed();
    test_screen();
    return test_finish("test_book_search");
}
