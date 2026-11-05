/*
 * Host-тесты режима чтения электронных книг (src/core/book_reader.c, ui_book.c,
 * интеграция в app_fsm.c).
 *
 * Проверяется:
 *   - вход выключает GNSS и запись трека, выход включает GNSS;
 *   - книга целиком в RAM: после открытия перелистывание не читает SD и файл
 *     закрыт; книга больше буфера: окно данных подгружается, любая страница
 *     читается верно вперёд и назад;
 *   - кнопки: страница, +-10 страниц, переход по процентам, список, выход;
 *   - позиция чтения: на каждом перелистывании меняется только RAM, на SD запись
 *     по тайм-ауту бездействия (в том числе при переполнении таймера) и при
 *     выходе; восстановление после перезагрузки; другой file_crc32 - начало;
 *   - список книг: фильтр *.PGB, сортировка, поврежденные файлы, пустой каталог;
 *   - ошибки открытия показываются на экране, выход возможен;
 *   - экран: режим дисплея 1 bit, геометрия текста, нет подсказок кнопок,
 *     перелистывание - частичное обновление без полного;
 *   - FSM: пункт BOOKS, отсутствие перерисовки по времени, выход в CONFIG.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "purrgo/app_fsm.h"
#include "purrgo/app_ui.h"
#include "purrgo/book_reader.h"
#include "purrgo/config.h"
#include "purrgo/menu_controller.h"
#include "purrgo/display_hal.h"
#include "purrgo/font5x7.h"
#include "purrgo/pc_display_stats.h"
#include "purrgo/pgb.h"
#include "purrgo/track_logger.h"
#include "ui_book.h"
#include "purrgo/ui/ui_dirty.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "tests/host/data"
#endif

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

#define BOOKS_DIR "books"
#define BOOK_A BOOKS_DIR "/ALPHA.PGB"

/* ---- Экран ------------------------------------------------------------------ */

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

/* ---- Генератор книги PGB 1.1 без сжатия ------------------------------------- */

#define ENTRY 12u   /* запись индекса PGB 1.1: offset, stored, raw, width, height */

static void wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t *p, uint32_t v) { wr16(p, v & 0xFFFFu); wr16(p + 2, v >> 16); }

/* Текст страницы i: от 3 до 8 строк разной длины, не длиннее 29 символов. */
static size_t make_page(uint32_t i, uint8_t *out)
{
    size_t n = 0;
    const uint32_t lines = 3u + (i % 6u);
    for (uint32_t k = 0; k < lines; k++) {
        char line[64];
        if (k == 0) {
            snprintf(line, sizeof line, "PAGE %u", (unsigned)i);
        } else {
            snprintf(line, sizeof line, "L%u of page %u %.*s", (unsigned)k, (unsigned)i,
                     (int)(3u + (k * 5u + i) % 12u), "xxxxxxxxxxxxxxxxxxxx");
        }
        line[29] = '\0';
        if (k != 0) { out[n++] = '\n'; }
        memcpy(out + n, line, strlen(line));
        n += strlen(line);
    }
    return n;
}

static uint8_t g_book[256 * 1024];

/* tweak != 0 меняет текст последней страницы (другой file_crc32 при тех же размерах). */
static uint32_t build_book(uint32_t pages, const char *title, const char *author, int tweak)
{
    const uint32_t tl = (uint32_t)strlen(title), al = (uint32_t)strlen(author);
    const uint32_t index_off = 64u + tl + al;
    const uint32_t data_off = index_off + ENTRY * pages;
    uint32_t pos = data_off;

    memset(g_book, 0, 64);
    memcpy(g_book + 64, title, tl);
    memcpy(g_book + 64 + tl, author, al);
    for (uint32_t i = 0; i < pages; i++) {
        uint8_t txt[PGB_SCREEN_MAX_PAGE_BYTES];
        size_t len = make_page(i, txt);
        if (tweak && i == pages - 1u) { txt[0] = 'Q'; }
        memcpy(g_book + pos, txt, len);
        wr32(g_book + index_off + ENTRY * i, pos);
        wr16(g_book + index_off + ENTRY * i + 4, (uint32_t)len);
        wr16(g_book + index_off + ENTRY * i + 6, (uint32_t)len);
        wr16(g_book + index_off + ENTRY * i + 8, 0);    /* текстовая страница: width = height = 0 */
        wr16(g_book + index_off + ENTRY * i + 10, 0);
        pos += (uint32_t)len;
    }
    memcpy(g_book, "PGB1", 4);
    wr16(g_book + 4, 1); wr16(g_book + 6, 1); wr16(g_book + 8, 64); wr16(g_book + 10, 0);
    wr32(g_book + 12, pages); wr32(g_book + 16, 64); wr32(g_book + 20, index_off);
    wr32(g_book + 24, data_off); wr32(g_book + 28, pos);
    g_book[40] = 29; g_book[41] = 24; g_book[42] = 0;
    wr16(g_book + 44, tl); wr16(g_book + 46, al);
    wr16(g_book + 48, 176); wr16(g_book + 50, 256);   /* окно картинок */

    pgb_crc_t c;
    pgb_crc_init(&c); pgb_crc_feed(&c, g_book, data_off);
    wr32(g_book + 36, pgb_crc_value(&c));
    pgb_crc_init(&c); pgb_crc_feed(&c, g_book, pos);
    wr32(g_book + 32, pgb_crc_value(&c));
    return pos;
}

static void put_book(const char *path, uint32_t pages, const char *title, const char *author, int tweak)
{
    const uint32_t size = build_book(pages, title, author, tweak);
    fake_fs_put_bin(path, g_book, size);
}

/* ---- Стенд -------------------------------------------------------------------- */

static void reset_all(void)
{
    fake_fs_reset();
    fake_power_reset();
    purrgo_logger_reset();
    purrgo_book_reader_reset();
    purrgo_book_reader_set_ram(NULL, 0);
    purrgo_config_init();
    g_now = 100000;
}

static purrgo_state_t press(purrgo_state_t st, purrgo_btn_t b, bool *redraw)
{
    purrgo_state_t next = st;
    bool r = purrgo_book_reader_handle_button(st, b, &next);
    if (redraw) { *redraw = r; }
    return next;
}

static bool page_is(uint32_t i)
{
    purrgo_book_view_t v;
    uint8_t exp[PGB_SCREEN_MAX_PAGE_BYTES];
    purrgo_book_reader_get_view(&v);
    size_t n = make_page(i, exp);
    return v.open && v.page == i && v.text != NULL && v.text_len == n && memcmp(v.text, exp, n) == 0;
}

static bool file_has(const char *name, const char *needle)
{
    uint32_t size = 0;
    const uint8_t *d = fake_fs_data(name, &size);
    if (d == NULL) { return false; }
    char *copy = malloc(size + 1u);
    memcpy(copy, d, size);
    copy[size] = '\0';
    bool r = strstr(copy, needle) != NULL;
    free(copy);
    return r;
}

/* ---- Тесты ---------------------------------------------------------------------- */

static void test_enter_whole_book(void)
{
    reset_all();
    put_book(BOOK_A, 40, "Alpha", "Author A", 0);
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");
    app_config.book_crc32 = 0;   /* позиции нет: начало */

    purrgo_state_t st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_READ);
    CHECK(purrgo_book_reader_is_active());
    CHECK(fake_power_gnss_off_calls() == 1);      /* GNSS выключен */
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);
    CHECK(page_is(0));
    CHECK(fake_fs_open_handles() == 0);           /* книга целиком в RAM: файл закрыт */

    /* Перелистывание не обращается к SD. */
    fake_fs_reset_read_stats();
    int opens = fake_fs_open_calls();
    bool redraw = false;
    st = press(st, PURRGO_BTN_KEY2_SHORT, &redraw);
    CHECK(redraw && page_is(1));
    for (int i = 0; i < 12; i++) { st = press(st, PURRGO_BTN_KEY2_SHORT, NULL); }
    CHECK(page_is(13));
    st = press(st, PURRGO_BTN_KEY2_LONG, &redraw);
    CHECK(redraw && page_is(23));
    st = press(st, PURRGO_BTN_KEY1_LONG, NULL);
    CHECK(page_is(13));
    st = press(st, PURRGO_BTN_KEY1_SHORT, NULL);
    CHECK(page_is(12));
    CHECK(fake_fs_read_calls() == 0);
    CHECK(fake_fs_open_calls() == opens);
    CHECK(st == APP_STATE_BOOK_READ);

    /* Края: на первой и последней странице кнопка ничего не меняет и не перерисовывает. */
    st = press(st, PURRGO_BTN_KEY1_LONG, NULL);
    st = press(st, PURRGO_BTN_KEY1_LONG, NULL);
    CHECK(page_is(0));
    st = press(st, PURRGO_BTN_KEY1_SHORT, &redraw);
    CHECK(!redraw && page_is(0));
    for (int i = 0; i < 6; i++) { st = press(st, PURRGO_BTN_KEY2_LONG, NULL); }
    CHECK(page_is(39));
    st = press(st, PURRGO_BTN_KEY2_SHORT, &redraw);
    CHECK(!redraw && page_is(39));
    st = press(st, PURRGO_BTN_KEY2_LONG, &redraw);
    CHECK(!redraw && page_is(39));

    /* Выход: меню CONFIG, GNSS включён. */
    st = press(st, PURRGO_BTN_KEY4_SHORT, &redraw);
    CHECK(redraw && st == APP_STATE_MENU_MAIN);
    CHECK(!purrgo_book_reader_is_active());
    CHECK(fake_power_gnss_on_calls() == 1);
    CHECK(fake_power_gnss_on_seq() > fake_power_gnss_off_seq());
    CHECK(fake_fs_open_handles() == 0);
}

static void test_windowed_book(void)
{
    reset_all();
    /* 300 страниц около 70 КБ при буфере 12 КБ: окно данных около 9 КБ (~40 страниц). */
    static uint8_t small[12 * 1024];
    purrgo_book_reader_set_ram(small, sizeof small);
    uint32_t size = build_book(300, "Big", "Author", 0);
    CHECK(size > 3u * sizeof small);
    fake_fs_put_bin(BOOK_A, g_book, size);
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");

    purrgo_state_t st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_READ);
    CHECK(page_is(0));
    CHECK(fake_fs_open_handles() == 1);           /* окно не покрывает книгу: файл остаётся открытым */

    /* Все страницы подряд вперёд: каждая верна, чтений SD заметно меньше, чем страниц. */
    fake_fs_reset_read_stats();
    for (uint32_t i = 1; i < 300; i++) {
        st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);
        CHECK(page_is(i));
    }
    uint32_t reads_forward = fake_fs_read_calls();
    CHECK(reads_forward > 0 && reads_forward < 300 / 4);

    /* Назад до начала. */
    for (uint32_t i = 299; i > 0; i--) {
        st = press(st, PURRGO_BTN_KEY1_SHORT, NULL);
        CHECK(page_is(i - 1u));
    }

    /* Далёкий переход по процентам: 50 % -> страница 150. */
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    for (int i = 0; i < 5; i++) { st = press(st, PURRGO_BTN_KEY2_LONG, NULL); }
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    CHECK(page_is(150));
    for (int i = 0; i < 20; i++) { st = press(st, PURRGO_BTN_KEY2_LONG, NULL); }
    CHECK(page_is(299));

    st = press(st, PURRGO_BTN_KEY4_SHORT, NULL);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(fake_fs_open_handles() == 0);

    /* Буфер, в который не помещается индекс (больше половины буфера): ошибка, а не порча памяти. */
    static uint8_t tiny[4000];
    purrgo_book_reader_set_ram(tiny, sizeof tiny);
    fake_fs_put_bin(BOOK_A, g_book, size);   /* 300 страниц: индекс 2400 байт */
    st = purrgo_book_reader_enter();
    purrgo_book_view_t v;
    CHECK(st == APP_STATE_BOOK_LIST);        /* последняя книга не открылась - список */
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    purrgo_book_reader_get_view(&v);
    CHECK(st == APP_STATE_BOOK_READ && !v.open && v.error == PGB_ERR_BUFFER);
    CHECK(fake_fs_open_handles() == 0);
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);

    /* Индекс помещается, а страница с запасом окна - нет: тоже ошибка. */
    static uint8_t narrow[180];
    purrgo_book_reader_set_ram(narrow, sizeof narrow);
    size = build_book(6, "T", "A", 0);        /* 6 страниц по 100-200 байт, индекс 48 байт */
    fake_fs_put_bin(BOOK_A, g_book, size);
    st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_LIST);
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    purrgo_book_reader_get_view(&v);
    CHECK(!v.open && v.error == PGB_ERR_BUFFER);
    CHECK(fake_fs_open_handles() == 0);
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);
    purrgo_book_reader_set_ram(NULL, 0);
}

static void test_position_save_policy(void)
{
    reset_all();
    put_book(BOOK_A, 60, "Alpha", "Author A", 0);
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");
    app_config.book_crc32 = 0;
    purrgo_config_save();                     /* стартовый PURRGO.CFG на SD */

    purrgo_state_t st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_READ);
    const int sync0 = fake_fs_sync_calls();

    /* Много перелистываний: SD не пишется, позиция живёт в RAM. */
    for (int i = 0; i < 25; i++) {
        g_now += 1000;
        st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);
        purrgo_book_reader_tick(g_now);
    }
    CHECK(app_config.book_page == 25);
    CHECK(purrgo_book_reader_position_dirty());
    CHECK(fake_fs_sync_calls() == sync0);

    /* Тайм-аут считается от последнего перелистывания. */
    g_now += PURRGO_BOOK_SAVE_TIMEOUT_MS - 1u;
    purrgo_book_reader_tick(g_now);
    CHECK(fake_fs_sync_calls() == sync0);
    CHECK(purrgo_book_reader_position_dirty());
    g_now += 1u;
    purrgo_book_reader_tick(g_now);
    CHECK(fake_fs_sync_calls() == sync0 + 1);
    CHECK(!purrgo_book_reader_position_dirty());
    CHECK(file_has("PURRGO.CFG", "BOOK_FILE=ALPHA.PGB"));
    CHECK(file_has("PURRGO.CFG", "BOOK_PAGE=25"));

    /* Без изменений позиция повторно не пишется. */
    g_now += 5u * PURRGO_BOOK_SAVE_TIMEOUT_MS;
    purrgo_book_reader_tick(g_now);
    CHECK(fake_fs_sync_calls() == sync0 + 1);

    /* Выход сохраняет сразу, не дожидаясь тайм-аута. */
    st = press(st, PURRGO_BTN_KEY2_LONG, NULL);
    CHECK(purrgo_book_reader_position_dirty());
    st = press(st, PURRGO_BTN_KEY4_SHORT, NULL);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(!purrgo_book_reader_position_dirty());
    CHECK(file_has("PURRGO.CFG", "BOOK_PAGE=35"));

    /* Сбой записи (SD не отвечает): позиция остаётся несохранённой, повтор через тайм-аут. */
    g_fault.fail_replace = 1;
    st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_READ && page_is(35));
    st = press(st, PURRGO_BTN_KEY1_SHORT, NULL);
    g_now += PURRGO_BOOK_SAVE_TIMEOUT_MS;
    purrgo_book_reader_tick(g_now);
    CHECK(purrgo_book_reader_position_dirty());
    g_fault.fail_replace = 0;
    g_now += PURRGO_BOOK_SAVE_TIMEOUT_MS - 1u;
    purrgo_book_reader_tick(g_now);
    CHECK(purrgo_book_reader_position_dirty());          /* от неудачной попытки не прошло тайм-аута */
    g_now += 1u;
    purrgo_book_reader_tick(g_now);
    CHECK(!purrgo_book_reader_position_dirty());
    CHECK(file_has("PURRGO.CFG", "BOOK_PAGE=34"));
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);

    /* Переполнение 32-битного таймера не ломает тайм-аут. */
    reset_all();
    put_book(BOOK_A, 20, "Alpha", "A", 0);
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");
    purrgo_config_save();
    g_now = 0xFFFFFFF0u;
    st = purrgo_book_reader_enter();
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);
    CHECK(purrgo_book_reader_position_dirty());
    g_now += PURRGO_BOOK_SAVE_TIMEOUT_MS;               /* теперь меньше 0xFFFFFFF0 */
    CHECK(g_now < 0xFFFFFFF0u);
    purrgo_book_reader_tick(g_now);
    CHECK(!purrgo_book_reader_position_dirty());
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);
}

static void test_resume_after_reboot(void)
{
    reset_all();
    put_book(BOOK_A, 60, "Alpha", "Author A", 0);
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");
    purrgo_config_save();

    purrgo_state_t st = purrgo_book_reader_enter();
    for (int i = 0; i < 17; i++) { st = press(st, PURRGO_BTN_KEY2_SHORT, NULL); }
    st = press(st, PURRGO_BTN_KEY4_SHORT, NULL);

    /* "Перезагрузка": конфиг с SD, состояние читалки обнулено. */
    memset(&app_config, 0, sizeof app_config);
    purrgo_book_reader_reset();
    CHECK(purrgo_config_load());
    CHECK(strcmp(app_config.book_file, "ALPHA.PGB") == 0);
    CHECK(app_config.book_page == 17);
    CHECK(app_config.book_crc32 != 0);
    st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_READ);
    CHECK(page_is(17));
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);

    /* Книгу перекомпилировали (другой file_crc32): сохранённая страница недействительна. */
    put_book(BOOK_A, 60, "Alpha", "Author A", 1);
    st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_READ);
    CHECK(page_is(0));
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);
    CHECK(app_config.book_page == 0);

    /* Страница за концом книги (книга стала короче при том же CRC невозможно, но конфиг могли править). */
    app_config.book_page = 9999;
    st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_READ);
    CHECK(page_is(0));
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);

    /* Файла последней книги нет: список. */
    fake_fs_reset();
    put_book(BOOKS_DIR "/OTHER.PGB", 5, "Other", "O", 0);
    st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_LIST);
    CHECK(purrgo_book_list_count() == 1);
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);

    /* Имя из конфига с путём не принимается. */
    fake_fs_reset();
    fake_fs_put("PURRGO.CFG", "BOOK_FILE=../x.PGB\nBOOK_CRC=zz\nBOOK_PAGE=-4\n");
    CHECK(purrgo_config_load());
    CHECK(app_config.book_file[0] == '\0');
    CHECK(app_config.book_crc32 == 0);
    CHECK(app_config.book_page == 0);
    fake_fs_put("PURRGO.CFG", "BOOK_FILE=A B.PGB\nBOOK_CRC=DEADBEEF\nBOOK_PAGE=7\n");
    CHECK(purrgo_config_load());
    CHECK(strcmp(app_config.book_file, "A B.PGB") == 0);
    CHECK(app_config.book_crc32 == 0xDEADBEEFu);
    CHECK(app_config.book_page == 7);
}

static void test_list(void)
{
    reset_all();
    put_book(BOOKS_DIR "/zeta.pgb", 3, "Zeta", "Zed", 0);
    put_book(BOOKS_DIR "/ALPHA.PGB", 4, "Alpha", "Author A", 0);
    put_book(BOOKS_DIR "/Mid.Pgb", 5, "Middle", "Em", 0);
    put_book(BOOKS_DIR "/._ALPHA.PGB", 4, "Fork", "x", 0);        /* ресурсный файл macOS */
    fake_fs_put(BOOKS_DIR "/notes.txt", "hello");
    fake_fs_put(BOOKS_DIR "/BROKEN.PGB", "this is not a book");
    put_book(BOOKS_DIR "/NOTITLE.PGB", 2, "", "", 0);

    purrgo_state_t st = purrgo_book_reader_enter();             /* книги в конфиге нет */
    CHECK(st == APP_STATE_BOOK_LIST);
    CHECK(fake_power_gnss_off_calls() == 1);
    CHECK(purrgo_book_list_count() == 5);                       /* 4 книги и одна повреждённая */

    /* Порядок по названию: пустое название заменено именем файла. */
    const char *want[] = { "ALPHA.PGB", "BROKEN.PGB", "Mid.Pgb", "NOTITLE.PGB", "zeta.pgb" };
    for (int i = 0; i < 5; i++) {
        const purrgo_book_item_t *it = purrgo_book_list_get(i);
        CHECK(it != NULL && strcmp(it->file, want[i]) == 0);
    }
    CHECK(purrgo_book_list_get(5) == NULL && purrgo_book_list_get(-1) == NULL);
    CHECK(purrgo_book_list_get(0)->ok && strcmp(purrgo_book_list_get(0)->title, "Alpha") == 0);
    CHECK(strcmp(purrgo_book_list_get(0)->author, "Author A") == 0 && purrgo_book_list_get(0)->page_count == 4);
    CHECK(!purrgo_book_list_get(1)->ok);
    CHECK(strcmp(purrgo_book_list_get(3)->title, "NOTITLE") == 0 && purrgo_book_list_get(3)->ok);
    CHECK(fake_fs_open_handles() == 0 && fake_fs_open_dirs() == 0);

    /* Курсор: границы без переноса, длинное нажатие - на 8 строк. */
    bool redraw;
    CHECK(purrgo_book_list_cursor() == 0);
    st = press(st, PURRGO_BTN_KEY1_SHORT, &redraw);
    CHECK(!redraw && purrgo_book_list_cursor() == 0);
    st = press(st, PURRGO_BTN_KEY2_SHORT, &redraw);
    CHECK(redraw && purrgo_book_list_cursor() == 1);
    st = press(st, PURRGO_BTN_KEY2_LONG, NULL);
    CHECK(purrgo_book_list_cursor() == 4);
    st = press(st, PURRGO_BTN_KEY2_SHORT, &redraw);
    CHECK(!redraw && purrgo_book_list_cursor() == 4);
    st = press(st, PURRGO_BTN_KEY1_LONG, NULL);
    CHECK(purrgo_book_list_cursor() == 0);

    /* Открыть Alpha: начало книги, позиция запомнена в app_config (в RAM). */
    st = press(st, PURRGO_BTN_KEY3_SHORT, &redraw);
    CHECK(redraw && st == APP_STATE_BOOK_READ && page_is(0));
    CHECK(strcmp(app_config.book_file, "ALPHA.PGB") == 0);
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);

    /* K3 длинное - обратно в список, курсор на последней открытой. */
    st = press(st, PURRGO_BTN_KEY3_LONG, &redraw);
    CHECK(redraw && st == APP_STATE_BOOK_LIST);
    CHECK(purrgo_book_list_cursor() == 0 && purrgo_book_list_is_last(0) && !purrgo_book_list_is_last(2));

    /* Открыть повреждённую: экран ошибки; список и выход работают. */
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    CHECK(st == APP_STATE_BOOK_READ);
    purrgo_book_view_t v;
    purrgo_book_reader_get_view(&v);
    CHECK(!v.open && v.error != PGB_OK && v.text == NULL);
    st = press(st, PURRGO_BTN_KEY2_SHORT, &redraw);
    CHECK(!redraw && st == APP_STATE_BOOK_READ);
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    CHECK(st == APP_STATE_BOOK_LIST);

    /* Из списка выход в CONFIG: GNSS включён. */
    st = press(st, PURRGO_BTN_KEY4_LONG, NULL);
    CHECK(st == APP_STATE_MENU_MAIN && fake_power_gnss_on_calls() == 1);

    /* Пустой каталог. */
    reset_all();
    st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_LIST && purrgo_book_list_count() == 0);
    st = press(st, PURRGO_BTN_KEY3_SHORT, &redraw);
    CHECK(!redraw && st == APP_STATE_BOOK_LIST);
    st = press(st, PURRGO_BTN_KEY2_SHORT, &redraw);
    CHECK(!redraw);
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);

    /* Лишние книги сверх лимита не ломают список. */
    reset_all();
    for (int i = 0; i < PURRGO_BOOK_LIST_MAX + 5; i++) {
        char name[64];
        snprintf(name, sizeof name, BOOKS_DIR "/B%02d.PGB", i);
        if (i >= 30) { break; }   /* в fake_fs только 32 файла */
        put_book(name, 2, "T", "A", 0);
    }
    st = purrgo_book_reader_enter();
    CHECK(purrgo_book_list_count() == 30);
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);
}

static void test_jump_mode(void)
{
    reset_all();
    put_book(BOOK_A, 101, "Alpha", "A", 0);
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");

    purrgo_state_t st = purrgo_book_reader_enter();
    bool redraw;
    purrgo_book_view_t v;

    /* 101 страница: проценты совпадают с индексом страницы. */
    st = press(st, PURRGO_BTN_KEY3_SHORT, &redraw);
    purrgo_book_reader_get_view(&v);
    CHECK(redraw && v.jump && v.jump_percent == 0 && v.jump_page == 0);
    st = press(st, PURRGO_BTN_KEY2_LONG, NULL);
    st = press(st, PURRGO_BTN_KEY2_LONG, NULL);
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);
    purrgo_book_reader_get_view(&v);
    CHECK(v.jump_percent == 21 && v.jump_page == 21 && v.page == 0);
    st = press(st, PURRGO_BTN_KEY1_SHORT, NULL);
    purrgo_book_reader_get_view(&v);
    CHECK(v.jump_percent == 20);

    /* Отмена: страница не меняется. */
    st = press(st, PURRGO_BTN_KEY4_SHORT, &redraw);
    purrgo_book_reader_get_view(&v);
    CHECK(redraw && st == APP_STATE_BOOK_READ && !v.jump && v.page == 0);

    /* Подтверждение. */
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    for (int i = 0; i < 4; i++) { st = press(st, PURRGO_BTN_KEY2_LONG, NULL); }
    st = press(st, PURRGO_BTN_KEY3_SHORT, &redraw);
    purrgo_book_reader_get_view(&v);
    CHECK(redraw && !v.jump && v.page == 40 && page_is(40) && v.percent == 40);

    /* Границы процентов 0..100. */
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    for (int i = 0; i < 12; i++) { st = press(st, PURRGO_BTN_KEY2_LONG, NULL); }
    st = press(st, PURRGO_BTN_KEY2_SHORT, &redraw);
    CHECK(!redraw);
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    CHECK(page_is(100));
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    for (int i = 0; i < 12; i++) { st = press(st, PURRGO_BTN_KEY1_LONG, NULL); }
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    CHECK(page_is(0));

    /* Выход из режима перехода не зависает: K4 отмена, K4 выход. */
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    st = press(st, PURRGO_BTN_KEY4_SHORT, NULL);
    CHECK(st == APP_STATE_BOOK_READ);
    st = press(st, PURRGO_BTN_KEY4_SHORT, NULL);
    CHECK(st == APP_STATE_MENU_MAIN);

    /* Книга из одной страницы: переход по процентам бессмыслен и не включается. */
    reset_all();
    put_book(BOOK_A, 1, "One", "A", 0);
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");
    st = purrgo_book_reader_enter();
    st = press(st, PURRGO_BTN_KEY3_SHORT, &redraw);
    purrgo_book_reader_get_view(&v);
    CHECK(!redraw && !v.jump && v.page_count == 1);
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);
}

static void test_lz4_fixtures_and_errors(void)
{
    reset_all();
    static uint8_t fix[8192];
    const char *files[] = { "sample.pgb", "sample_py.pgb", "sample_raw.pgb" };
    for (int f = 0; f < 3; f++) {
        char path[256];
        snprintf(path, sizeof path, TEST_DATA_DIR "/%s", files[f]);
        FILE *fp = fopen(path, "rb");
        CHECK(fp != NULL);
        if (fp == NULL) { continue; }
        size_t n = fread(fix, 1, sizeof fix, fp);
        fclose(fp);
        fake_fs_reset();
        fake_fs_put_bin(BOOKS_DIR "/SAMPLE.PGB", fix, (uint32_t)n);
        purrgo_book_reader_reset();
        snprintf(app_config.book_file, sizeof app_config.book_file, "SAMPLE.PGB");
        app_config.book_crc32 = 0;
        purrgo_state_t st = purrgo_book_reader_enter();
        CHECK(st == APP_STATE_BOOK_READ);
        purrgo_book_view_t v;
        purrgo_book_reader_get_view(&v);
        CHECK(v.open && v.page_count == 7);
        for (uint32_t p = 0; p < 7; p++) {
            purrgo_book_reader_get_view(&v);
            CHECK(v.page == p && v.text != NULL && v.text_len > 0 && v.page_error == PGB_OK);
            st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);
        }
        (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);
    }

    /* Повреждённый индекс: открытие отклоняется по meta_crc32, выход без зависаний. */
    reset_all();
    uint32_t size = build_book(10, "Alpha", "A", 0);
    g_book[70 + (int)ENTRY * 3] ^= 0x40;   /* байт в индексе (CRC не пересчитан) */
    fake_fs_put_bin(BOOK_A, g_book, size);
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");
    purrgo_state_t st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_LIST);                 /* последняя книга не открылась */
    st = press(st, PURRGO_BTN_KEY3_SHORT, NULL);
    purrgo_book_view_t v;
    purrgo_book_reader_get_view(&v);
    CHECK(st == APP_STATE_BOOK_READ && !v.open && v.error == PGB_ERR_META_CRC);
    CHECK(fake_fs_open_handles() == 0);
    st = press(st, PURRGO_BTN_KEY4_SHORT, NULL);
    CHECK(st == APP_STATE_MENU_MAIN);

    /* Повреждена одна страница (индекс в порядке): остальные страницы читаются. */
    reset_all();
    size = build_book(10, "Alpha", "A", 0);
    {
        uint32_t index_off = 64u + 5u + 1u;
        uint32_t off = (uint32_t)g_book[index_off + ENTRY * 4u] | ((uint32_t)g_book[index_off + ENTRY * 4u + 1u] << 8);
        g_book[off] = 0x01;   /* управляющий код в тексте страницы 4 */
        pgb_crc_t c;
        pgb_crc_init(&c); pgb_crc_feed(&c, g_book, size);
        wr32(g_book + 32, pgb_crc_value(&c));   /* meta_crc не затронут: данные страниц после индекса */
    }
    fake_fs_put_bin(BOOK_A, g_book, size);
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");
    st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_READ && page_is(0));
    for (int i = 0; i < 4; i++) { st = press(st, PURRGO_BTN_KEY2_SHORT, NULL); }
    purrgo_book_reader_get_view(&v);
    CHECK(v.page == 4 && v.text == NULL && v.page_error == PGB_ERR_PAGE_DATA);
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);
    CHECK(page_is(5));
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);
}

/* ---- Экран и FSM ------------------------------------------------------------------ */

static void render(gfx_context_t *g)
{
    purrgo_app_ui_render(g, purrgo_app_get_gnss_solution(), NULL);
}

static void run_ticks(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 100) { g_now += 100; purrgo_app_tick(g_now); }
}

static bool glyph_at(int cx, int cy, unsigned char ch)
{
    /* Символ ch в ячейке (cx, cy): x = 1 + cx * 6, y = PGB_TEXT_TOP_PX + cy * PGB_LINE_PITCH_PX (шаг строк 10 px). */
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            const int want = ((font5x7[ch][col] >> row) & 1) ? BLACK : WHITE;
            if (px(1 + cx * 6 + col, (int)PGB_TEXT_TOP_PX + cy * (int)PGB_LINE_PITCH_PX + row) != want) { return false; }
        }
    }
    return true;
}

static void test_fsm_and_screen(void)
{
    reset_all();
    gfx_context_t g;
    memset(g_fb, 0, sizeof g_fb);
    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));

    put_book(BOOKS_DIR "/ALPHA.PGB", 50, "Alpha", "Author A", 0);
    put_book(BOOKS_DIR "/BETA.PGB", 9, "Beta", "Author B", 0);

    memset(&app_config, 0, sizeof app_config);
    purrgo_app_init();                 /* конфиг: умолчания, SD пуста */
    purrgo_app_ui_reset();
    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));
    run_ticks(1000);
    CHECK(purrgo_app_get_state() == APP_STATE_MAP);

    /* Карта -> Trip -> CONFIG; первый пункт - BOOKS. */
    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);
    CHECK(purrgo_app_get_state() == APP_STATE_MENU_MAIN);
    CHECK(menu_app_get_main_cursor() == 0);
    render(&g);

    /* BOOKS: книги в конфиге нет - список; GNSS выключен. */
    purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);
    CHECK(purrgo_app_get_state() == APP_STATE_BOOK_LIST);
    CHECK(fake_power_gnss_off_calls() == 1);
    CHECK(!purrgo_app_get_gnss_solution()->valid);
    CHECK(purrgo_ui_dirty_test(UI_DIRTY_SCREEN));
    pc_display_stats_reset();
    render(&g);
    CHECK(pc_display_stats_get().mode == DISPLAY_MODE_MONO);
    CHECK(pc_display_stats_get().fulls == 1);                  /* смена экрана - полное обновление */
    CHECK(px(0, H - 1) == WHITE || px(0, H - 1) == BLACK);      /* подсказки кнопок на месте */

    /* Выбрать Beta (вторая) и открыть. */
    purrgo_app_handle_button(PURRGO_BTN_KEY2_SHORT);
    render(&g);
    purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);
    CHECK(purrgo_app_get_state() == APP_STATE_BOOK_READ);
    pc_display_stats_reset();
    render(&g);
    CHECK(pc_display_stats_get().mode == DISPLAY_MODE_MONO);
    CHECK(pc_display_stats_get().fulls == 1);

    /* Страница 0 книги Beta: статус инверсный, текст "PAGE 0" с y = PGB_TEXT_TOP_PX, x = 1. */
    CHECK(px(0, 0) == BLACK && px(175, 7) == BLACK);            /* строка статуса залита */
    CHECK(glyph_at(0, 0, 'P') && glyph_at(1, 0, 'A') && glyph_at(5, 0, '0'));
    CHECK(px(100, 9) == WHITE);                                 /* дальше строки фон белый */
    {
        /* Последняя строка экрана - текст книги, а не подсказки кнопок: на странице 0 всего
         * 3 + 0 % 6 = 3 строки, ниже пусто, но область не залита подсказкой. */
        int dark = 0;
        for (int x = 0; x < W; x++) { dark += (px(x, H - 1) == BLACK); }
        CHECK(dark == 0);
    }

    /* Перелистывание: частичное обновление всего экрана, без полного. */
    pc_display_stats_reset();
    purrgo_app_handle_button(PURRGO_BTN_KEY2_SHORT);
    CHECK(purrgo_app_get_state() == APP_STATE_BOOK_READ);
    render(&g);
    pc_display_stats_t ds = pc_display_stats_get();
    CHECK(ds.fulls == 0 && ds.partials == 1);
    CHECK(glyph_at(5, 0, '1'));

    /* Время идёт, фикса нет и не будет: перерисовок нет, SD не пишется (позиция ждёт тайм-аут). */
    const int sync0 = fake_fs_sync_calls();
    for (int i = 0; i < 100; i++) {
        run_ticks(1000);
        CHECK(!purrgo_app_ui_needs_render(g_now));
    }
    CHECK(fake_fs_sync_calls() == sync0 + 1);                  /* ровно одна запись по тайм-ауту */
    CHECK(file_has("PURRGO.CFG", "BOOK_FILE=BETA.PGB"));
    CHECK(file_has("PURRGO.CFG", "BOOK_PAGE=1"));
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);

    /* Кнопка без действия на краю не вызывает перерисовки. */
    purrgo_app_handle_button(PURRGO_BTN_KEY1_SHORT);            /* страница 0 */
    render(&g);
    purrgo_app_handle_button(PURRGO_BTN_KEY1_SHORT);            /* уже первая */
    CHECK(!purrgo_app_ui_needs_render(g_now));

    /* Переход по процентам рисует подсказку поверх нижней строки. */
    purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);
    CHECK(purrgo_app_ui_needs_render(g_now));
    render(&g);
    {
        int dark = 0;
        for (int x = 0; x < W; x++) { dark += (px(x, H - 1) == BLACK); }
        CHECK(dark > W / 2);
    }
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);            /* отмена */
    render(&g);

    /* Выход: CONFIG, GNSS включён, позиция записана, курсор меню на BOOKS. */
    purrgo_app_handle_button(PURRGO_BTN_KEY2_SHORT);
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);
    CHECK(purrgo_app_get_state() == APP_STATE_MENU_MAIN);
    CHECK(fake_power_gnss_on_calls() == 1);
    CHECK(menu_app_get_main_cursor() == 0);
    CHECK(file_has("PURRGO.CFG", "BOOK_PAGE=1"));
    pc_display_stats_reset();
    render(&g);
    CHECK(pc_display_stats_get().fulls == 1);

    /* Повторный вход в BOOKS возвращает на последнюю книгу и страницу. */
    purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);
    CHECK(purrgo_app_get_state() == APP_STATE_BOOK_READ);
    CHECK(page_is(1));
    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);
    CHECK(purrgo_app_get_state() == APP_STATE_MENU_MAIN);

    /* Назад в навигатор: MENU, карта. */
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);
    CHECK(purrgo_app_get_state() == APP_STATE_MAP);
}

/* Строка статуса (инверсная): текст str с ячейки cx, белые символы на чёрном. */
static bool status_text_at(int cx, const char *str)
{
    for (int i = 0; str[i] != '\0'; i++) {
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 7; row++) {
                const int want = ((font5x7[(unsigned char)str[i]][col] >> row) & 1) ? WHITE : BLACK;
                if (px(1 + (cx + i) * 6 + col, row) != want) { return false; }
            }
        }
    }
    return true;
}

/*
 * Строка статуса: страница/всего слева, заряд батареи справа (без процента
 * прочитанного); заряд опрашивается раз в минуту и меняет только строку статуса;
 * если на экране ничего не изменилось, дисплей не обновляется.
 */
static void test_status_battery_and_idle_refresh(void)
{
    reset_all();
    gfx_context_t g;
    memset(g_fb, 0, sizeof g_fb);
    put_book(BOOKS_DIR "/ALPHA.PGB", 50, "Alpha", "Author A", 0);

    memset(&app_config, 0, sizeof app_config);
    purrgo_app_init();
    purrgo_app_ui_reset();
    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));
    app_config.power_source = PURRGO_POWER_SRC_ZINC_CARBON;   /* 3 x 1200 мВ = 25 % */
    fake_power_set_battery_pin_mv(1800);                      /* делитель 1:1 */
    run_ticks(1000);

    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);
    render(&g);
    purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);           /* BOOKS: список */
    render(&g);
    purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);           /* открыть книгу */
    CHECK(purrgo_app_get_state() == APP_STATE_BOOK_READ);
    render(&g);

    /* "1/50" слева, "25%" у правого края (процента прочитанного нет). */
    CHECK(status_text_at(0, "1/50"));
    CHECK(status_text_at(26, "25%"));

    /* Батарея заряжена, но замер раз в минуту: до минуты ничего не меняется и перерисовки нет. */
    fake_power_set_battery_pin_mv(2400);                       /* 100 % */
    run_ticks(50000);
    CHECK(!purrgo_app_ui_needs_render(g_now));

    /* Минута прошла: заряд изменился - обновляется только строка статуса. */
    run_ticks(20000);
    CHECK(purrgo_app_ui_needs_render(g_now));
    pc_display_stats_reset();
    render(&g);
    {
        pc_display_stats_t ds = pc_display_stats_get();
        CHECK(ds.fulls == 0 && ds.partials == 1);
        CHECK(ds.last_y == 0 && ds.last_h == 8 && ds.last_w == W);
    }
    CHECK(status_text_at(25, "100%"));
    CHECK(glyph_at(0, 0, 'P'));                                /* текст страницы цел */

    /* Значение не изменилось: новых перерисовок нет, сколько бы времени ни прошло. */
    for (int i = 0; i < 5; i++) {
        run_ticks(61000);
        CHECK(!purrgo_app_ui_needs_render(g_now));
    }

    /* Принудительный проход при неизменном экране: ни рисования, ни обновления дисплея. */
    pc_display_stats_reset();
    purrgo_ui_dirty_mark(UI_DIRTY_SCREEN);
    render(&g);
    {
        pc_display_stats_t ds = pc_display_stats_get();
        CHECK(ds.fulls == 0 && ds.partials == 0);
    }

    /* Перелистывание: обновляется весь экран. */
    purrgo_app_handle_button(PURRGO_BTN_KEY2_SHORT);
    pc_display_stats_reset();
    render(&g);
    {
        pc_display_stats_t ds = pc_display_stats_get();
        CHECK(ds.fulls == 0 && ds.partials == 1);
        CHECK(ds.last_y == 0 && ds.last_h == H);
    }

    /* Переход по процентам: подсказка внизу - весь экран, смена процента - только строка статуса. */
    purrgo_app_handle_button(PURRGO_BTN_KEY3_SHORT);
    pc_display_stats_reset();
    render(&g);
    CHECK(pc_display_stats_get().last_h == H);
    purrgo_app_handle_button(PURRGO_BTN_KEY2_SHORT);
    pc_display_stats_reset();
    render(&g);
    {
        pc_display_stats_t ds = pc_display_stats_get();
        CHECK(ds.partials == 1 && ds.last_y == 0 && ds.last_h == 8);
    }
}

/* ---- Страницы-картинки ------------------------------------------------------------ */

/* Страница i: i % 3 == 0 - текст, 1 - картинка 176x256, 2 - картинка 50x40 (ширина не кратна 8). */
static bool img_black(unsigned page, unsigned x, unsigned y) { return ((x * 3u + y * 5u + page) % 7u) < 3u; }

static void img_dims(uint32_t i, unsigned *w, unsigned *h)
{
    *w = (i % 3u == 1u) ? 176u : (i % 3u == 2u) ? 50u : 0u;
    *h = (i % 3u == 1u) ? 256u : (i % 3u == 2u) ? 40u : 0u;
}

/* bad_pad != 0 ставит ненулевой бит выравнивания в первой картинке 50x40. */
static uint32_t build_image_book(uint32_t pages, int bad_pad)
{
    const char *title = "Pics", *author = "P";
    const uint32_t tl = (uint32_t)strlen(title), al = (uint32_t)strlen(author);
    const uint32_t index_off = 64u + tl + al;
    const uint32_t data_off = index_off + ENTRY * pages;
    uint32_t pos = data_off;
    int pad_done = 0;

    memset(g_book, 0, 64);
    memcpy(g_book + 64, title, tl);
    memcpy(g_book + 64 + tl, author, al);
    for (uint32_t i = 0; i < pages; i++) {
        unsigned w, h;
        img_dims(i, &w, &h);
        size_t len;
        if (w == 0u) {
            len = make_page(i, g_book + pos);
        } else {
            const unsigned rb = PGB_IMG_ROW_BYTES(w);
            len = (size_t)rb * h;
            memset(g_book + pos, 0, len);
            for (unsigned y = 0; y < h; y++) {
                for (unsigned x = 0; x < w; x++) {
                    if (img_black(i, x, y)) { g_book[pos + y * rb + x / 8u] |= (uint8_t)(0x80u >> (x % 8u)); }
                }
            }
            if (bad_pad && w == 50u && !pad_done) { g_book[pos + rb - 1u] |= 0x01u; pad_done = 1; }
        }
        wr32(g_book + index_off + ENTRY * i, pos);
        wr16(g_book + index_off + ENTRY * i + 4, (uint32_t)len);
        wr16(g_book + index_off + ENTRY * i + 6, (uint32_t)len);
        wr16(g_book + index_off + ENTRY * i + 8, w);
        wr16(g_book + index_off + ENTRY * i + 10, h);
        pos += (uint32_t)len;
    }
    memcpy(g_book, "PGB1", 4);
    wr16(g_book + 4, 1); wr16(g_book + 6, 1); wr16(g_book + 8, 64); wr16(g_book + 10, 0);
    wr32(g_book + 12, pages); wr32(g_book + 16, 64); wr32(g_book + 20, index_off);
    wr32(g_book + 24, data_off); wr32(g_book + 28, pos);
    g_book[40] = 29; g_book[41] = 24; g_book[42] = 0;
    wr16(g_book + 44, tl); wr16(g_book + 46, al);
    wr16(g_book + 48, 176); wr16(g_book + 50, 256);

    pgb_crc_t c;
    pgb_crc_init(&c); pgb_crc_feed(&c, g_book, data_off);
    wr32(g_book + 36, pgb_crc_value(&c));
    pgb_crc_init(&c); pgb_crc_feed(&c, g_book, pos);
    wr32(g_book + 32, pgb_crc_value(&c));
    return pos;
}

/* Что показывает читалка на странице i: текст make_page() или картинка с нужными пикселями. */
static bool shown_page_is(uint32_t i)
{
    purrgo_book_view_t v;
    unsigned w, h;
    purrgo_book_reader_get_view(&v);
    img_dims(i, &w, &h);
    if (!v.open || v.page != i || v.page_error != PGB_OK) { return false; }
    if (w == 0u) { return page_is(i) && v.image == NULL; }
    if (v.text != NULL || v.image == NULL || v.image_w != w || v.image_h != h) { return false; }
    for (unsigned y = 0; y < h; y++) {
        for (unsigned x = 0; x < w; x++) {
            const int bit = (v.image[y * PGB_IMG_ROW_BYTES(w) + x / 8u] >> (7u - x % 8u)) & 1;
            if (bit != (int)img_black(i, x, y)) { return false; }
        }
    }
    return true;
}

static void test_image_pages(void)
{
    gfx_context_t g;
    purrgo_state_t st;
    bool redraw = false;

    reset_all();
    fake_fs_put_bin(BOOK_A, g_book, build_image_book(5, 0));
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");
    app_config.book_crc32 = 0;
    st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_READ && shown_page_is(0));
    CHECK(fake_fs_open_handles() == 0);

    memset(g_fb, 0, sizeof g_fb);
    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));
    display_set_mode(DISPLAY_MODE_MONO);

    /* Страница 1: картинка 176x256 на весь экран под строкой статуса. */
    st = press(st, PURRGO_BTN_KEY2_SHORT, &redraw);
    CHECK(redraw && shown_page_is(1));
    ui_book_read_reset();
    pc_display_stats_reset();
    ui_render_book_read(&g);
    display_flush();
    CHECK(pc_display_stats_get().partials + pc_display_stats_get().fulls >= 1 && pc_display_stats_get().last_h == H);
    CHECK(px(0, 0) == BLACK && px(175, 7) == BLACK);               /* строка статуса */
    {
        int bad = 0;
        for (int y = 0; y < 256; y++) {
            for (int x = 0; x < 176; x++) {
                if (px(x, 8 + y) != (img_black(1, (unsigned)x, (unsigned)y) ? BLACK : WHITE)) { bad++; }
            }
        }
        CHECK(bad == 0);
    }

    /* Страница 2: картинка 50x40 по центру окна 176x256, вокруг белый фон. */
    st = press(st, PURRGO_BTN_KEY2_SHORT, &redraw);
    CHECK(redraw && shown_page_is(2));
    ui_render_book_read(&g);
    {
        const int x0 = (176 - 50) / 2, y0 = 8 + (256 - 40) / 2;
        int bad = 0, outside_dark = 0;
        for (int y = 8; y < H; y++) {
            for (int x = 0; x < W; x++) {
                const bool inside = x >= x0 && x < x0 + 50 && y >= y0 && y < y0 + 40;
                const int want = inside ? (img_black(2, (unsigned)(x - x0), (unsigned)(y - y0)) ? BLACK : WHITE) : WHITE;
                if (px(x, y) != want) { bad++; }
                if (!inside && px(x, y) == BLACK) { outside_dark++; }
            }
        }
        CHECK(bad == 0 && outside_dark == 0);
    }

    /* Следующий кадр без изменений ничего не рисует; переход по процентам рисует подсказку поверх картинки. */
    display_flush();    /* обновление предыдущего кадра */
    pc_display_stats_reset();
    ui_render_book_read(&g);
    display_flush();
    CHECK(pc_display_stats_get().partials == 0 && pc_display_stats_get().fulls == 0);
    st = press(st, PURRGO_BTN_KEY3_SHORT, &redraw);
    ui_render_book_read(&g);
    {
        int dark = 0;
        for (int x = 0; x < W; x++) { dark += (px(x, H - 1) == BLACK); }
        CHECK(dark > W / 2);
    }
    st = press(st, PURRGO_BTN_KEY4_SHORT, NULL);                    /* отмена */

    /* Страница 3 - снова текст: картинка не остаётся на экране. */
    st = press(st, PURRGO_BTN_KEY2_SHORT, &redraw);
    CHECK(redraw && shown_page_is(3));
    ui_render_book_read(&g);
    {
        int dark_low = 0;
        for (int y = 200; y < H; y++) {
            for (int x = 0; x < W; x++) { dark_low += (px(x, y) == BLACK); }
        }
        CHECK(dark_low == 0);
    }
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);
    CHECK(shown_page_is(4));
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);

    /* Повреждённая картинка (ненулевой бит выравнивания): страница с ошибкой, соседние читаются. */
    reset_all();
    fake_fs_put_bin(BOOK_A, g_book, build_image_book(5, 1));
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");
    st = purrgo_book_reader_enter();
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);
    CHECK(shown_page_is(1));
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);
    {
        purrgo_book_view_t v;
        purrgo_book_reader_get_view(&v);
        CHECK(v.page == 2 && v.text == NULL && v.image == NULL && v.page_error == PGB_ERR_PAGE_DATA);
    }
    memset(g_fb, 0, sizeof g_fb);
    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));
    ui_book_read_reset();
    ui_render_book_read(&g);                                        /* сообщение об ошибке, без падения */
    st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);
    CHECK(shown_page_is(3));
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);

    /* Книга больше RAM-буфера: окно данных с картинками подгружается, страницы верны вперёд и назад. */
    reset_all();
    static uint8_t small_ram[14000];
    purrgo_book_reader_set_ram(small_ram, sizeof small_ram);
    {
        const uint32_t size = build_image_book(10, 0);
        CHECK(size > sizeof small_ram);
        fake_fs_put_bin(BOOK_A, g_book, size);
    }
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");
    st = purrgo_book_reader_enter();
    CHECK(st == APP_STATE_BOOK_READ && shown_page_is(0));
    CHECK(fake_fs_open_handles() == 1);                             /* окно меньше книги: файл открыт */
    int ok = 1;
    for (uint32_t i = 1; i < 10; i++) {
        st = press(st, PURRGO_BTN_KEY2_SHORT, NULL);
        if (!shown_page_is(i)) { ok = 0; }
    }
    for (uint32_t i = 9; i-- > 0;) {
        st = press(st, PURRGO_BTN_KEY1_SHORT, NULL);
        if (!shown_page_is(i)) { ok = 0; }
    }
    CHECK(ok);
    (void)press(st, PURRGO_BTN_KEY4_SHORT, NULL);
    purrgo_book_reader_set_ram(NULL, 0);
}

/* Шаг строк текста 10 px: строки на PGB_TEXT_TOP_PX + n * 10, между ними 2 px пустых, нижний край свободен. */
static void test_line_pitch(void)
{
    gfx_context_t g;
    reset_all();
    put_book(BOOK_A, 3, "Alpha", "A", 0);        /* страница 0: 3 строки */
    snprintf(app_config.book_file, sizeof app_config.book_file, "ALPHA.PGB");
    (void)purrgo_book_reader_enter();
    memset(g_fb, 0, sizeof g_fb);
    CHECK(gfx_init(&g, W, H, g_fb, cb_draw, cb_read));
    ui_book_read_reset();
    ui_render_book_read(&g);

    CHECK(PGB_SCREEN_ROWS == 24u && PGB_LINE_PITCH_PX == 10u);
    CHECK(PGB_TEXT_TOP_PX - PGB_STATUS_H_PX == (unsigned)H - PGB_TEXT_TOP_PX - (23u * PGB_LINE_PITCH_PX + 8u));   /* поля сверху и снизу равны */
    CHECK(PGB_TEXT_TOP_PX + 23u * PGB_LINE_PITCH_PX + 8u <= (unsigned)H);   /* 24-я строка помещается */
    CHECK(glyph_at(0, 0, 'P') && glyph_at(1, 0, 'A') && glyph_at(2, 0, 'G') && glyph_at(3, 0, 'E'));
    CHECK(glyph_at(0, 1, 'L') && glyph_at(1, 1, '1'));              /* вторая строка - на 10 px ниже первой */
    CHECK(glyph_at(0, 2, 'L') && glyph_at(1, 2, '2'));
    for (int cy = 0; cy < 2; cy++) {                                /* промежуток 2 px под строкой (кроме подчёркиваний) пуст */
        for (int gap = 8; gap < 10; gap++) {
            int dark = 0;
            for (int x = 0; x < W; x++) { dark += (px(x, (int)PGB_TEXT_TOP_PX + cy * 10 + gap) == BLACK); }
            CHECK(dark == 0);
        }
    }
    (void)press(APP_STATE_BOOK_READ, PURRGO_BTN_KEY4_SHORT, NULL);
}

int main(void)
{
    test_enter_whole_book();
    test_windowed_book();
    test_position_save_policy();
    test_resume_after_reboot();
    test_list();
    test_jump_mode();
    test_lz4_fixtures_and_errors();
    test_fsm_and_screen();
    test_image_pages();
    test_line_pitch();
    test_status_battery_and_idle_refresh();
    return test_finish("test_book_reader");
}
