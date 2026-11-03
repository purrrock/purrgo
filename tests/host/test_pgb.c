/*
 * Host-тесты читателя электронных книг PGB (src/core/pgb.c).
 *
 * Эталонный файл tests/host/data/sample.pgb собран независимым компилятором
 * tools/book-compiler (CRC из zlib), см. tests/host/data/make_sample_pgb.py:
 * 150 строк «Строка NNN[ x...]», 5 страниц (4 x 32 строки + 22), название
 * «Тест PGB», автор «Автор».
 *
 * Проверяется: CRC (вектор zlib и обнуление полей CRC), разбор заголовка
 * (каждое правило отклонения), открытие и геометрия, размер файла, чтение
 * всех страниц и их содержимого, повреждения (каждый байт области
 * заголовок + метаданные + индекс), подмена индекса при пересчитанных CRC,
 * полная проверка файла, ошибки ФС и утечки дескрипторов.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "purrgo/pgb.h"
#include "fake_fs.h"
#include "test_common.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "tests/host/data"
#endif

#define BOOK "0:/PURRGO/BOOKS/SAMPLE.PGB"
#define MAX_FILE 8192u

static uint8_t g_sample[MAX_FILE];
static uint32_t g_sample_size;

/* Служебные смещения эталонного файла (заголовок 64 байта, название 8, автор 5, 5 записей индекса). */
#define OFF_TITLE_LEN   44u
#define OFF_INDEX       77u
#define OFF_DATA        117u

static int load_sample(void)
{
    FILE *f = fopen(TEST_DATA_DIR "/sample.pgb", "rb");
    if (!f) { return 0; }
    g_sample_size = (uint32_t)fread(g_sample, 1, sizeof(g_sample), f);
    fclose(f);
    return g_sample_size > 0u && g_sample_size < sizeof(g_sample);
}

static void put(const uint8_t *data, uint32_t size)
{
    fake_fs_reset();
    fake_fs_put_bin(BOOK, data, size);
}

static void put_sample(void) { put(g_sample, g_sample_size); }

static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | (rd16(p + 2) << 16); }

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* Пересчёт обоих CRC после намеренной порчи (проверка логики индекса, а не CRC). */
static void reseal(uint8_t *data, uint32_t size)
{
    pgb_crc_t meta;
    pgb_crc_init(&meta);
    pgb_crc_feed(&meta, data, OFF_DATA);
    wr32(data + 36, pgb_crc_value(&meta));
    pgb_crc_t all;
    pgb_crc_init(&all);
    pgb_crc_feed(&all, data, size);
    wr32(data + 32, pgb_crc_value(&all));
}

/* Ожидаемая строка номер n (1..150) в PurrGO-256: «Строка NNN» + [' ' + (n % 10) x]. */
static size_t expected_line(unsigned n, uint8_t *out)
{
    static const uint8_t word[] = { 0x91, 0xB2, 0xB0, 0xAE, 0xAA, 0xA0, ' ' }; /* «Строка » */
    size_t len = 0;
    memcpy(out, word, sizeof(word));
    len = sizeof(word);
    out[len++] = (uint8_t)('0' + (n / 100u) % 10u);
    out[len++] = (uint8_t)('0' + (n / 10u) % 10u);
    out[len++] = (uint8_t)('0' + n % 10u);
    if (n % 10u) {
        out[len++] = ' ';
        for (unsigned i = 0; i < n % 10u; i++) { out[len++] = 'x'; }
    }
    return len;
}

static pgb_status_t open_sample(pgb_book_t *b)
{
    return pgb_open(BOOK, PGB_SCREEN_COLS, PGB_SCREEN_ROWS, b);
}

static void test_crc(void)
{
    CHECK(pgb_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926u);
    CHECK(pgb_crc32((const uint8_t *)"", 0) == 0u);

    /* Потоковый CRC не зависит от порции и обнуляет байты 32..39. */
    uint8_t a[100], b[100];
    for (unsigned i = 0; i < sizeof(a); i++) { a[i] = (uint8_t)(i * 7u + 1u); }
    memcpy(b, a, sizeof(a));
    memset(b + 32, 0xEE, 8);
    pgb_crc_t c1, c2;
    pgb_crc_init(&c1);
    pgb_crc_feed(&c1, a, sizeof(a));
    pgb_crc_init(&c2);
    pgb_crc_feed(&c2, b, 10);
    pgb_crc_feed(&c2, b + 10, 31);
    pgb_crc_feed(&c2, b + 41, 59);
    CHECK(pgb_crc_value(&c1) == pgb_crc_value(&c2));
    uint8_t z[100];
    memcpy(z, a, sizeof(a));
    memset(z + 32, 0, 8);
    CHECK(pgb_crc_value(&c1) == pgb_crc32(z, sizeof(z)));
}

static void test_header_decode(void)
{
    pgb_header_t h;
    CHECK(pgb_header_decode(g_sample, g_sample_size, &h) == PGB_OK);
    CHECK(h.page_count == 5u && h.cols == 29u && h.rows == 32u);
    CHECK(h.title_length == 8u && h.author_length == 5u);
    CHECK(h.meta_offset == 64u && h.index_offset == OFF_INDEX && h.data_offset == OFF_DATA);
    CHECK(h.file_size == g_sample_size);

    uint8_t m[PGB_HEADER_SIZE];
#define MUTATE(expected, stmt) do { memcpy(m, g_sample, sizeof(m)); stmt; \
        CHECK(pgb_header_decode(m, sizeof(m), &h) == (expected)); } while (0)

    CHECK(pgb_header_decode(g_sample, 63, &h) == PGB_ERR_HEADER);
    CHECK(pgb_header_decode(NULL, 64, &h) == PGB_ERR_HEADER);
    MUTATE(PGB_ERR_MAGIC,       m[0] = 'X');
    MUTATE(PGB_ERR_VERSION,     m[4] = 2);
    MUTATE(PGB_ERR_VERSION,     m[6] = 1);
    MUTATE(PGB_ERR_HEADER,      m[8] = 65);
    MUTATE(PGB_ERR_UNSUPPORTED, m[10] = 1);
    MUTATE(PGB_ERR_UNSUPPORTED, m[42] = 1);          /* compression */
    MUTATE(PGB_ERR_UNSUPPORTED, m[43] = 1);          /* reserved0 */
    MUTATE(PGB_ERR_UNSUPPORTED, m[63] = 1);          /* reserved1 */
    MUTATE(PGB_ERR_HEADER,      wr32(m + 12, 0));    /* page_count */
    MUTATE(PGB_ERR_HEADER,      m[40] = 0);          /* cols */
    MUTATE(PGB_ERR_HEADER,      m[41] = 0);          /* rows */
    MUTATE(PGB_ERR_HEADER,      m[44] = 201; m[45] = 0);   /* title_length > 200 */
    MUTATE(PGB_ERR_HEADER,      m[46] = 101; m[47] = 0);   /* author_length > 100 */
    MUTATE(PGB_ERR_HEADER,      wr32(m + 16, 65));   /* meta_offset */
    MUTATE(PGB_ERR_HEADER,      wr32(m + 20, OFF_INDEX + 1u));
    MUTATE(PGB_ERR_HEADER,      wr32(m + 24, OFF_DATA - 1u));
    MUTATE(PGB_ERR_HEADER,      wr32(m + 28, OFF_DATA + 4u));   /* меньше страниц по 1 байту */
    MUTATE(PGB_ERR_HEADER,      wr32(m + 12, 0x20000000u));     /* 8 * page_count переполняет uint32 */
}

static void test_open_and_info(void)
{
    pgb_book_t b;
    put_sample();
    CHECK(open_sample(&b) == PGB_OK);
    CHECK(b.header.page_count == 5u);
    CHECK(strcmp(b.title, "\x92\xA5\xB1\xB2 PGB") == 0);           /* «Тест PGB» */
    CHECK(strcmp(b.author, "\x80\xA2\xB2\xAE\xB0") == 0);          /* «Автор» */
    pgb_close(&b);
    CHECK(b.file == NULL && fake_fs_open_handles() == 0);

    pgb_info_t info;
    CHECK(pgb_read_info(BOOK, &info) == PGB_OK);
    CHECK(strcmp(info.title, "\x92\xA5\xB1\xB2 PGB") == 0);
    CHECK(strcmp(info.author, "\x80\xA2\xB2\xAE\xB0") == 0);
    CHECK(info.page_count == 5u && info.cols == 29u && info.rows == 32u);
    CHECK(info.file_crc32 == rd32(g_sample + 32));
    CHECK(fake_fs_open_handles() == 0);

    /* список книг читает один сектор, а не файл целиком */
    fake_fs_reset_read_stats();
    CHECK(pgb_read_info(BOOK, &info) == PGB_OK);
    CHECK(fake_fs_max_read_end() <= PGB_INFO_READ_SIZE);

    /* геометрия экрана */
    CHECK(pgb_open(BOOK, 29, 31, &b) == PGB_ERR_GEOMETRY && b.file == NULL);
    CHECK(pgb_open(BOOK, 30, 32, &b) == PGB_ERR_GEOMETRY);
    CHECK(fake_fs_open_handles() == 0);

    /* нет файла / нет аргументов */
    CHECK(pgb_open("0:/nope.pgb", 29, 32, &b) == PGB_ERR_IO);
    CHECK(pgb_read_info("0:/nope.pgb", &info) == PGB_ERR_IO);
    CHECK(pgb_open(NULL, 29, 32, &b) == PGB_ERR_IO);
}

static void test_read_all_pages(void)
{
    pgb_book_t b;
    put_sample();
    CHECK(open_sample(&b) == PGB_OK);

    uint8_t page[PGB_SCREEN_MAX_PAGE_BYTES];
    unsigned line_no = 1;
    int content_ok = 1;
    static const unsigned expected_lines[5] = { 32, 32, 32, 32, 22 };
    for (uint32_t p = 0; p < 5u; p++) {
        size_t len = 0;
        CHECK(pgb_page_read(&b, p, page, sizeof(page), &len) == PGB_OK);
        unsigned lines_here = 0;
        size_t pos = 0;
        while (pos <= len) {
            size_t end = pos;
            while (end < len && page[end] != '\n') { end++; }
            uint8_t want[40];
            size_t want_len = expected_line(line_no, want);
            if (end - pos != want_len || memcmp(page + pos, want, want_len) != 0) { content_ok = 0; }
            line_no++; lines_here++;
            pos = end + 1u;
        }
        CHECK(lines_here == expected_lines[p]);
    }
    CHECK(content_ok);
    CHECK(line_no == 151u);

    /* буфер: ровно по размеру страницы подходит, на байт меньше - нет */
    size_t len = 0;
    CHECK(pgb_page_read(&b, 4, page, sizeof(page), &len) == PGB_OK);
    size_t last_len = len;
    CHECK(pgb_page_read(&b, 4, page, last_len, &len) == PGB_OK && len == last_len);
    CHECK(pgb_page_read(&b, 4, page, last_len - 1u, &len) == PGB_ERR_BUFFER && len == 0u);

    /* номер вне книги, неверные аргументы */
    CHECK(pgb_page_read(&b, 5, page, sizeof(page), &len) == PGB_ERR_PAGE_NUMBER);
    CHECK(pgb_page_read(&b, 0xFFFFFFFFu, page, sizeof(page), &len) == PGB_ERR_PAGE_NUMBER);
    CHECK(pgb_page_read(&b, 0, NULL, sizeof(page), &len) == PGB_ERR_IO);
    pgb_close(&b);
    CHECK(pgb_page_read(&b, 0, page, sizeof(page), &len) == PGB_ERR_IO);   /* закрытая книга */
    CHECK(fake_fs_open_handles() == 0);
}

static void test_file_size(void)
{
    pgb_book_t b;
    uint8_t big[MAX_FILE];

    put(g_sample, g_sample_size - 1u);
    CHECK(open_sample(&b) == PGB_ERR_SIZE);

    memcpy(big, g_sample, g_sample_size);
    big[g_sample_size] = 0;
    put(big, g_sample_size + 1u);
    CHECK(open_sample(&b) == PGB_ERR_SIZE);

    put(g_sample, 30u);
    CHECK(open_sample(&b) == PGB_ERR_IO);
    put(g_sample, 0u);
    CHECK(open_sample(&b) == PGB_ERR_IO);
    CHECK(fake_fs_open_handles() == 0);
}

static void test_meta_corruption(void)
{
    /*
     * Любой испорченный байт заголовка, метаданных или индекса не должен пройти
     * pgb_open. Исключение - сам file_crc32 (байты 32..35): при расчёте meta_crc32
     * он обнуляется, его порча видна только при полной проверке файла.
     */
    uint8_t data[MAX_FILE];
    int all_rejected = 1;
    for (uint32_t i = 0; i < OFF_DATA; i++) {
        if (i >= 32u && i < 36u) { continue; }
        for (int bit = 0; bit < 8; bit++) {
            memcpy(data, g_sample, g_sample_size);
            data[i] ^= (uint8_t)(1u << bit);
            put(data, g_sample_size);
            pgb_book_t b;
            if (open_sample(&b) == PGB_OK) { all_rejected = 0; pgb_close(&b); }
        }
    }
    CHECK(all_rejected);
    CHECK(fake_fs_open_handles() == 0);

    /* порча полей CRC даёт именно CRC-ошибку */
    memcpy(data, g_sample, g_sample_size);
    data[36] ^= 1;
    put(data, g_sample_size);
    pgb_book_t b;
    CHECK(open_sample(&b) == PGB_ERR_META_CRC);

    /* file_crc32 испорчен: книга открывается, полная проверка находит несоответствие */
    memcpy(data, g_sample, g_sample_size);
    data[32] ^= 1;
    put(data, g_sample_size);
    CHECK(open_sample(&b) == PGB_OK);
    CHECK(pgb_verify_file(&b, NULL, NULL) == PGB_ERR_FILE_CRC);
    pgb_close(&b);
    memcpy(data, g_sample, g_sample_size);
    data[OFF_INDEX + 8] ^= 1;      /* запись индекса страницы 1 */
    put(data, g_sample_size);
    CHECK(open_sample(&b) == PGB_ERR_META_CRC);
}

static void test_page_data_corruption(void)
{
    uint8_t data[MAX_FILE];
    uint8_t page[PGB_SCREEN_MAX_PAGE_BYTES];
    size_t len;
    pgb_book_t b;

    /* управляющий код в тексте страницы 2: open проходит, страница отклоняется */
    memcpy(data, g_sample, g_sample_size);
    uint32_t page2 = rd32(data + OFF_INDEX + 16);
    data[page2 + 3u] = 0x01;
    put(data, g_sample_size);
    CHECK(open_sample(&b) == PGB_OK);
    CHECK(pgb_page_read(&b, 2, page, sizeof(page), &len) == PGB_ERR_PAGE_DATA);
    CHECK(pgb_page_read(&b, 1, page, sizeof(page), &len) == PGB_OK);
    CHECK(pgb_verify_file(&b, NULL, NULL) == PGB_ERR_FILE_CRC);
    pgb_close(&b);

    /* допустимый символ вместо другого: страницу читаем, но полная проверка ловит */
    memcpy(data, g_sample, g_sample_size);
    data[page2 + 3u] ^= 0x01;
    put(data, g_sample_size);
    CHECK(open_sample(&b) == PGB_OK);
    CHECK(pgb_page_read(&b, 2, page, sizeof(page), &len) == PGB_OK);
    CHECK(pgb_verify_file(&b, NULL, NULL) == PGB_ERR_FILE_CRC);
    pgb_close(&b);

    /* страница, оканчивающаяся на LF, и строка длиннее 29 байт (при пересчитанных CRC) */
    memcpy(data, g_sample, g_sample_size);
    data[g_sample_size - 1u] = '\n';
    reseal(data, g_sample_size);
    put(data, g_sample_size);
    CHECK(open_sample(&b) == PGB_OK);
    CHECK(pgb_page_read(&b, 4, page, sizeof(page), &len) == PGB_ERR_PAGE_DATA);
    CHECK(pgb_verify_file(&b, NULL, NULL) == PGB_OK);   /* CRC согласован, ошибка лишь в тексте */
    pgb_close(&b);

    memcpy(data, g_sample, g_sample_size);
    page2 = rd32(data + OFF_INDEX + 16);
    for (uint32_t i = 0; i < 30u; i++) { data[page2 + i] = 'A'; }   /* 30 символов без LF */
    reseal(data, g_sample_size);
    put(data, g_sample_size);
    CHECK(open_sample(&b) == PGB_OK);
    CHECK(pgb_page_read(&b, 2, page, sizeof(page), &len) == PGB_ERR_PAGE_DATA);
    pgb_close(&b);
}

static void test_tampered_index(void)
{
    uint8_t data[MAX_FILE];
    uint8_t page[PGB_SCREEN_MAX_PAGE_BYTES];
    size_t len;
    pgb_book_t b;

    /* Индекс подменён, CRC пересчитаны: открытие проходит, но стыковка страниц нарушена. */
    memcpy(data, g_sample, g_sample_size);
    wr32(data + OFF_INDEX + 8, rd32(data + OFF_INDEX + 8) + 1u); /* offset[1] + 1 */
    reseal(data, g_sample_size);
    put(data, g_sample_size);
    CHECK(open_sample(&b) == PGB_OK);
    CHECK(pgb_page_read(&b, 0, page, sizeof(page), &len) == PGB_ERR_INDEX);   /* конец 0 != начало 1 */
    CHECK(pgb_page_read(&b, 1, page, sizeof(page), &len) == PGB_ERR_INDEX);   /* конец 1 != начало 2 */
    CHECK(pgb_page_read(&b, 2, page, sizeof(page), &len) == PGB_OK);
    pgb_close(&b);

    /* первая страница не с data_offset */
    memcpy(data, g_sample, g_sample_size);
    wr32(data + OFF_INDEX, OFF_DATA + 1u);
    reseal(data, g_sample_size);
    put(data, g_sample_size);
    CHECK(open_sample(&b) == PGB_OK);
    CHECK(pgb_page_read(&b, 0, page, sizeof(page), &len) == PGB_ERR_INDEX);
    pgb_close(&b);

    /* нулевой размер, stored != raw, размер больше допустимого */
    for (int variant = 0; variant < 3; variant++) {
        memcpy(data, g_sample, g_sample_size);
        uint8_t *entry = data + OFF_INDEX + 8u * 3u;
        if (variant == 0) { entry[4] = 0; entry[5] = 0; entry[6] = 0; entry[7] = 0; }
        if (variant == 1) { entry[6] ^= 1; }
        if (variant == 2) { entry[4] = 0xC0; entry[5] = 0x03; entry[6] = 0xC0; entry[7] = 0x03; }   /* 960 */
        reseal(data, g_sample_size);
        put(data, g_sample_size);
        CHECK(open_sample(&b) == PGB_OK);
        CHECK(pgb_page_read(&b, 3, page, sizeof(page), &len) == PGB_ERR_INDEX);
        pgb_close(&b);
    }

    /* последняя страница выходит за file_size */
    memcpy(data, g_sample, g_sample_size);
    data[OFF_INDEX + 8u * 4u + 4u] += 1;
    data[OFF_INDEX + 8u * 4u + 6u] += 1;
    reseal(data, g_sample_size);
    put(data, g_sample_size);
    CHECK(open_sample(&b) == PGB_OK);
    CHECK(pgb_page_read(&b, 4, page, sizeof(page), &len) == PGB_ERR_INDEX);
    pgb_close(&b);
    CHECK(fake_fs_open_handles() == 0);
}

static int g_progress_calls;
static uint32_t g_progress_last;
static bool progress_ok(uint32_t done, uint32_t total, void *user)
{
    (void)user;
    g_progress_calls++;
    g_progress_last = done;
    return done <= total;
}
static bool progress_cancel(uint32_t done, uint32_t total, void *user)
{
    (void)done; (void)total; (void)user;
    return false;
}

static void test_verify_file(void)
{
    pgb_book_t b;
    put_sample();
    CHECK(open_sample(&b) == PGB_OK);
    CHECK(pgb_verify_file(&b, NULL, NULL) == PGB_OK);
    g_progress_calls = 0; g_progress_last = 0;
    CHECK(pgb_verify_file(&b, progress_ok, NULL) == PGB_OK);
    CHECK(g_progress_calls == (int)((g_sample_size + 511u) / 512u) && g_progress_last == g_sample_size);
    CHECK(pgb_verify_file(&b, progress_cancel, NULL) == PGB_ERR_CANCELLED);
    pgb_close(&b);
    CHECK(pgb_verify_file(&b, NULL, NULL) == PGB_ERR_IO);
}

static void test_status_text(void)
{
    int ok = 1;
    for (int s = PGB_OK; s <= PGB_ERR_CANCELLED; s++) {
        const char *t = pgb_status_text((pgb_status_t)s);
        if (!t || !t[0]) { ok = 0; }
    }
    CHECK(ok);
    CHECK(pgb_status_text((pgb_status_t)99) != NULL);
}

int main(void)
{
    if (!load_sample()) {
        printf("не найден %s/sample.pgb (python3 tests/host/data/make_sample_pgb.py)\n", TEST_DATA_DIR);
        return 1;
    }
    test_crc();
    test_header_decode();
    test_open_and_info();
    test_read_all_pages();
    test_file_size();
    test_meta_corruption();
    test_page_data_corruption();
    test_tampered_index();
    test_verify_file();
    test_status_text();
    return test_finish("test_pgb");
}
