/*
 * Host-тесты читателя электронных книг PGB (src/core/pgb.c).
 *
 * Эталонные файлы tests/host/data/sample*.pgb собраны tools/book-compiler
 * (tests/host/data/make_sample_pgb.py), CRC считает zlib, а не проверяемый код:
 *   sample.pgb      - LZ4 Block, кодер пакета lz4 (HC);
 *   sample_py.pgb   - LZ4 Block, встроенный кодер конвертера;
 *   sample_raw.pgb  - без сжатия.
 * Во всех трёх одно и то же: 154 строки, 7 страниц, название «Тест PGB», автор
 * «Автор»; страница 6 (индекс 5) - случайный текст, в сжатых книгах она хранится
 * как есть.
 *
 * Четвёртая книга sample_img.pgb: страницы-картинки (176x256, 13x5, 100x60) и одна
 * текстовая страница между ними, LZ4.
 *
 * Проверяется: CRC (вектор zlib и обнуление полей CRC), декодер LZ4 (векторы,
 * все виды повреждений, мутации страниц без выхода за буферы), разбор заголовка,
 * открытие, геометрия, размер файла, содержимое всех страниц, повреждения
 * (каждый бит области заголовок + метаданные + индекс, каждый байт данных),
 * подмена индекса при пересчитанных CRC, полная проверка файла, ошибки ФС,
 * образ книги в памяти (pgb_page_read_mem, окно данных, meta_crc32 по образу).
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
#define PAGES 7u

/* Заголовок 64 байта, название 8, автор 5, затем индекс из PAGES записей. */
#define OFF_INDEX 77u
#define ENTRY     12u   /* запись индекса: offset u32, stored u16, raw u16, width u16, height u16 */
#define OFF_DATA  (OFF_INDEX + ENTRY * PAGES)

typedef struct {
    const char *file;
    int compressed;
    uint8_t data[MAX_FILE];
    uint32_t size;
} fixture_t;

static fixture_t g_fix[3] = {
    { "sample.pgb", 1, { 0 }, 0 },
    { "sample_py.pgb", 1, { 0 }, 0 },
    { "sample_raw.pgb", 0, { 0 }, 0 },
};
static fixture_t *g_fx;

#define DATA (g_fx->data)
#define SIZE (g_fx->size)

static int load_fixtures(void)
{
    for (size_t i = 0; i < sizeof(g_fix) / sizeof(g_fix[0]); i++) {
        char path[256];
        snprintf(path, sizeof(path), TEST_DATA_DIR "/%s", g_fix[i].file);
        FILE *f = fopen(path, "rb");
        if (!f) { printf("не найден %s (python3 tests/host/data/make_sample_pgb.py)\n", path); return 0; }
        g_fix[i].size = (uint32_t)fread(g_fix[i].data, 1, MAX_FILE, f);
        fclose(f);
        if (g_fix[i].size == 0u || g_fix[i].size >= MAX_FILE) { return 0; }
    }
    return 1;
}

static void put(const uint8_t *data, uint32_t size)
{
    fake_fs_reset();
    fake_fs_put_bin(BOOK, data, size);
}

static void put_fixture(void) { put(DATA, SIZE); }

static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | (rd16(p + 2) << 16); }

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* Запись индекса страницы n: смещение, размер в файле, размер текста. */
static uint32_t entry_offset(const uint8_t *d, uint32_t n) { return rd32(d + OFF_INDEX + ENTRY * n); }
static uint32_t entry_stored(const uint8_t *d, uint32_t n) { return rd16(d + OFF_INDEX + ENTRY * n + 4u); }
static uint32_t entry_raw(const uint8_t *d, uint32_t n)    { return rd16(d + OFF_INDEX + ENTRY * n + 6u); }

/* Пересчёт обоих CRC после намеренной порчи (проверка логики индекса, а не CRC). */
static void reseal_at(uint8_t *data, uint32_t size, uint32_t data_offset)
{
    pgb_crc_t meta;
    pgb_crc_init(&meta);
    pgb_crc_feed(&meta, data, data_offset);
    wr32(data + 36, pgb_crc_value(&meta));
    pgb_crc_t all;
    pgb_crc_init(&all);
    pgb_crc_feed(&all, data, size);
    wr32(data + 32, pgb_crc_value(&all));
}

static void reseal(uint8_t *data, uint32_t size) { reseal_at(data, size, OFF_DATA); }

/* ---- Ожидаемое содержимое (то же, что строит make_sample_pgb.py) ----------- */
typedef struct { unsigned n; uint32_t lcg; } line_gen_t;

static size_t next_line(line_gen_t *g, uint8_t *out)
{
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    static const uint8_t word1[] = { 0x91, 0xB2, 0xB0, 0xAE, 0xAA, 0xA0, ' ' };   /* «Строка » */
    static const uint8_t word2[] = { 0x95, 0xA2, 0xAE, 0xB1, 0xB2, ' ' };         /* «Хвост » */
    size_t len = 0;
    unsigned n = ++g->n;
    if (n <= 120u) {
        memcpy(out, word1, sizeof(word1));
        len = sizeof(word1);
        out[len++] = (uint8_t)('0' + (n / 100u) % 10u);
        out[len++] = (uint8_t)('0' + (n / 10u) % 10u);
        out[len++] = (uint8_t)('0' + n % 10u);
        if (n % 10u) {
            out[len++] = ' ';
            for (unsigned i = 0; i < n % 10u; i++) { out[len++] = 'x'; }
        }
    } else if (n <= 144u) {
        for (unsigned i = 0; i < 29u; i++) {
            g->lcg = g->lcg * 1664525u + 1013904223u;
            out[len++] = (uint8_t)alphabet[(g->lcg >> 16) % 62u];
        }
    } else {
        unsigned k = n - 144u;
        memcpy(out, word2, sizeof(word2));
        len = sizeof(word2);
        out[len++] = (uint8_t)('0' + (k / 10u) % 10u);
        out[len++] = (uint8_t)('0' + k % 10u);
    }
    return len;
}

static pgb_status_t open_fixture(pgb_book_t *b)
{
    return pgb_open(BOOK, PGB_SCREEN_COLS, PGB_SCREEN_ROWS, b);
}

/* ---- CRC -------------------------------------------------------------------- */
static void test_crc(void)
{
    CHECK(pgb_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926u);
    CHECK(pgb_crc32((const uint8_t *)"", 0) == 0u);

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

/* ---- Декодер LZ4 ------------------------------------------------------------ */
static int lz4_ok(const uint8_t *src, size_t src_len, const uint8_t *expect, size_t expect_len)
{
    uint8_t out[512];
    memset(out, 0xA5, sizeof(out));
    if (!pgb_lz4_decode(src, src_len, out, expect_len)) { return 0; }
    return memcmp(out, expect, expect_len) == 0 && out[expect_len] == 0xA5;
}

static int lz4_fails(const uint8_t *src, size_t src_len, size_t dst_len)
{
    uint8_t out[512];
    return !pgb_lz4_decode(src, src_len, out, dst_len);
}

static void test_lz4_vectors(void)
{
    /* только литералы */
    static const uint8_t hello[] = { 0x50, 'h', 'e', 'l', 'l', 'o' };
    CHECK(lz4_ok(hello, sizeof(hello), (const uint8_t *)"hello", 5));

    /* литерал 'a' и совпадение offset=1, длина 9 (перекрывающееся копирование) */
    static const uint8_t run[] = { 0x15, 'a', 0x01, 0x00, 0x00 };
    CHECK(lz4_ok(run, sizeof(run), (const uint8_t *)"aaaaaaaaaa", 10));

    /* "ab" и совпадение offset=2, длина 6 */
    static const uint8_t ab[] = { 0x22, 'a', 'b', 0x02, 0x00, 0x00 };
    CHECK(lz4_ok(ab, sizeof(ab), (const uint8_t *)"abababab", 8));

    /* длина литералов 300 = 15 + 255 + 30 */
    uint8_t lit[3 + 300];
    uint8_t lit_expect[300];
    lit[0] = 0xF0; lit[1] = 0xFF; lit[2] = 0x1E;
    for (unsigned i = 0; i < 300u; i++) { lit[3 + i] = (uint8_t)(i * 3u); lit_expect[i] = (uint8_t)(i * 3u); }
    CHECK(lz4_ok(lit, sizeof(lit), lit_expect, sizeof(lit_expect)));

    /* длина совпадения 300 = 4 + 15 + 255 + 26 */
    static const uint8_t longmatch[] = { 0x1F, 'a', 0x01, 0x00, 0xFF, 0x1A, 0x00 };
    uint8_t aexpect[301];
    memset(aexpect, 'a', sizeof(aexpect));
    CHECK(lz4_ok(longmatch, sizeof(longmatch), aexpect, 301));

    /* ---- повреждённые блоки ---- */
    CHECK(lz4_fails(hello, 0, 5));                    /* пустой вход */
    CHECK(lz4_fails(hello, 3, 5));                    /* литералы оборваны */
    CHECK(lz4_fails(hello, sizeof(hello), 4));        /* литералов больше буфера */
    CHECK(lz4_fails(hello, sizeof(hello), 6));        /* выход короче ожидаемого */
    static const uint8_t trailing[] = { 0x50, 'h', 'e', 'l', 'l', 'o', 0x00 };
    CHECK(lz4_fails(trailing, sizeof(trailing), 5));  /* лишний байт после блока */
    static const uint8_t off0[] = { 0x11, 'a', 0x00, 0x00, 0x00 };
    CHECK(lz4_fails(off0, sizeof(off0), 6));          /* смещение 0 */
    static const uint8_t offfar[] = { 0x11, 'a', 0x02, 0x00, 0x00 };
    CHECK(lz4_fails(offfar, sizeof(offfar), 6));      /* смещение больше выведенного */
    CHECK(lz4_fails(run, sizeof(run), 9));            /* совпадение выходит за буфер */
    CHECK(lz4_fails(run, sizeof(run), 11));           /* вывод короче буфера */
    static const uint8_t nofinal[] = { 0x15, 'a', 0x01, 0x00 };
    CHECK(lz4_fails(nofinal, sizeof(nofinal), 10));   /* после совпадения нет токена */
    static const uint8_t cutoff[] = { 0x11, 'a', 0x01 };
    CHECK(lz4_fails(cutoff, sizeof(cutoff), 6));      /* смещение оборвано */
    static const uint8_t cutext[] = { 0xF0, 0xFF };
    CHECK(lz4_fails(cutext, sizeof(cutext), 300));    /* расширение длины оборвано */
    static const uint8_t cutmatch[] = { 0x1F, 'a', 0x01, 0x00, 0xFF };
    CHECK(lz4_fails(cutmatch, sizeof(cutmatch), 300));
    uint8_t huge[2000];
    memset(huge, 0xFF, sizeof(huge));
    huge[0] = 0xF0;
    CHECK(lz4_fails(huge, sizeof(huge), 400));        /* бесконечные 0xFF: переполнений нет */
    huge[0] = 0x1F; huge[1] = 'a'; huge[2] = 1; huge[3] = 0;
    CHECK(lz4_fails(huge, sizeof(huge), 400));
    uint8_t dummy[8];
    CHECK(!pgb_lz4_decode(NULL, 1, dummy, 1));
    CHECK(!pgb_lz4_decode(hello, sizeof(hello), NULL, 5));
}

/* Мутации реальных сжатых страниц: любой исход допустим, кроме выхода за буферы (ASan). */
static void test_lz4_fuzz(void)
{
    static const uint8_t patterns[] = { 0x01, 0x80, 0xFF, 0x00, 0x0F, 0xF0 };
    int decoded_ok = 0;
    for (uint32_t p = 0; p < PAGES; p++) {
        uint32_t stored = entry_stored(DATA, p), raw = entry_raw(DATA, p);
        if (stored >= raw) { continue; }
        const uint8_t *src = DATA + entry_offset(DATA, p);
        uint8_t *in = malloc(stored);
        uint8_t *out = malloc(raw);
        for (uint32_t i = 0; i < stored; i++) {
            for (size_t k = 0; k < sizeof(patterns); k++) {
                memcpy(in, src, stored);
                in[i] = (k < 4u) ? (uint8_t)(in[i] ^ patterns[k]) : patterns[k];
                if (pgb_lz4_decode(in, stored, out, raw)) { decoded_ok++; }
            }
        }
        /* усечения и удлинения входа */
        for (uint32_t cut = 0; cut < stored; cut++) {
            CHECK(!pgb_lz4_decode(in, cut, out, raw) || cut == stored);
        }
        free(in);
        free(out);
    }
    CHECK(decoded_ok >= 0);   /* сам факт прохода под ASan/UBSan */
}

/* ---- Заголовок -------------------------------------------------------------- */
static void test_header_decode(void)
{
    pgb_header_t h;
    CHECK(pgb_header_decode(DATA, SIZE, &h) == PGB_OK);
    CHECK(h.page_count == PAGES && h.cols == 29u && h.rows == 24u);
    CHECK(h.img_w == 176u && h.img_h == 256u);
    CHECK(h.compression == (g_fx->compressed ? PGB_COMPRESSION_LZ4 : PGB_COMPRESSION_NONE));
    CHECK(h.title_length == 8u && h.author_length == 5u);
    CHECK(h.meta_offset == 64u && h.index_offset == OFF_INDEX && h.data_offset == OFF_DATA);
    CHECK(h.file_size == SIZE);

    uint8_t m[PGB_HEADER_SIZE];
#define MUTATE(expected, stmt) do { memcpy(m, DATA, sizeof(m)); stmt; \
        CHECK(pgb_header_decode(m, sizeof(m), &h) == (expected)); } while (0)

    CHECK(pgb_header_decode(DATA, 63, &h) == PGB_ERR_HEADER);
    CHECK(pgb_header_decode(NULL, 64, &h) == PGB_ERR_HEADER);
    MUTATE(PGB_ERR_MAGIC,       m[0] = 'X');
    MUTATE(PGB_ERR_VERSION,     m[4] = 2);
    MUTATE(PGB_ERR_VERSION,     m[6] = 0);           /* PGB 1.0 (старый формат) отклоняется */
    MUTATE(PGB_ERR_VERSION,     m[6] = 2);
    MUTATE(PGB_ERR_HEADER,      m[8] = 65);
    MUTATE(PGB_ERR_UNSUPPORTED, m[10] = 1);
    MUTATE(PGB_ERR_UNSUPPORTED, m[42] = 2);          /* неизвестное сжатие */
    MUTATE(PGB_ERR_UNSUPPORTED, m[42] = 255);
    MUTATE(PGB_ERR_UNSUPPORTED, m[43] = 1);          /* reserved0 */
    MUTATE(PGB_ERR_UNSUPPORTED, m[63] = 1);          /* reserved1 */
    MUTATE(PGB_ERR_UNSUPPORTED, m[52] = 1);
    MUTATE(PGB_ERR_HEADER,      m[48] = 0; m[49] = 0);                    /* img_w */
    MUTATE(PGB_ERR_HEADER,      m[50] = 0; m[51] = 0);                    /* img_h */
    MUTATE(PGB_ERR_HEADER,      m[48] = 0xFF; m[49] = 0xFF; m[50] = 0xFF; m[51] = 0xFF);   /* окно больше uint16 */
    MUTATE(PGB_OK,              m[48] = 100; m[49] = 0);                  /* другое окно - решает pgb_open */
    MUTATE(PGB_ERR_HEADER,      wr32(m + 12, 0));    /* page_count */
    MUTATE(PGB_ERR_HEADER,      m[40] = 0);          /* cols */
    MUTATE(PGB_ERR_HEADER,      m[41] = 0);          /* rows */
    MUTATE(PGB_ERR_HEADER,      m[44] = 201; m[45] = 0);
    MUTATE(PGB_ERR_HEADER,      m[46] = 101; m[47] = 0);
    MUTATE(PGB_ERR_HEADER,      wr32(m + 16, 65));
    MUTATE(PGB_ERR_HEADER,      wr32(m + 20, OFF_INDEX + 1u));
    MUTATE(PGB_ERR_HEADER,      wr32(m + 24, OFF_DATA - 1u));
    MUTATE(PGB_ERR_HEADER,      wr32(m + 28, OFF_DATA + 4u));
    MUTATE(PGB_ERR_HEADER,      wr32(m + 12, 0x20000000u));
    /* значения сжатия 0 и 1 допустимы при любой раскладке заголовка */
    MUTATE(PGB_OK,              m[42] = 0);
    MUTATE(PGB_OK,              m[42] = 1);
}

/* ---- Открытие и список ------------------------------------------------------ */
static void test_open_and_info(void)
{
    pgb_book_t b;
    put_fixture();
    CHECK(open_fixture(&b) == PGB_OK);
    CHECK(b.header.page_count == PAGES);
    CHECK(strcmp(b.title, "\x92\xA5\xB1\xB2 PGB") == 0);           /* «Тест PGB» */
    CHECK(strcmp(b.author, "\x80\xA2\xB2\xAE\xB0") == 0);          /* «Автор» */
    pgb_close(&b);
    CHECK(b.file == NULL && fake_fs_open_handles() == 0);

    pgb_info_t info;
    CHECK(pgb_read_info(BOOK, &info) == PGB_OK);
    CHECK(strcmp(info.title, "\x92\xA5\xB1\xB2 PGB") == 0);
    CHECK(strcmp(info.author, "\x80\xA2\xB2\xAE\xB0") == 0);
    CHECK(info.page_count == PAGES && info.cols == 29u && info.rows == 24u);
    CHECK(info.img_w == 176u && info.img_h == 256u);
    CHECK(info.compression == (g_fx->compressed ? PGB_COMPRESSION_LZ4 : PGB_COMPRESSION_NONE));
    CHECK(info.file_crc32 == rd32(DATA + 32));
    CHECK(fake_fs_open_handles() == 0);

    fake_fs_reset_read_stats();
    CHECK(pgb_read_info(BOOK, &info) == PGB_OK);
    CHECK(fake_fs_max_read_end() <= PGB_INFO_READ_SIZE);

    CHECK(pgb_open(BOOK, 29, 31, &b) == PGB_ERR_GEOMETRY && b.file == NULL);
    CHECK(pgb_open(BOOK, 30, 24, &b) == PGB_ERR_GEOMETRY);
    CHECK(fake_fs_open_handles() == 0);

    CHECK(pgb_open("0:/nope.pgb", 29, 24, &b) == PGB_ERR_IO);
    CHECK(pgb_read_info("0:/nope.pgb", &info) == PGB_ERR_IO);
    CHECK(pgb_open(NULL, 29, 24, &b) == PGB_ERR_IO);
}

/* ---- Чтение страниц --------------------------------------------------------- */
static void test_read_all_pages(void)
{
    pgb_book_t b;
    put_fixture();
    CHECK(open_fixture(&b) == PGB_OK);

    uint8_t page[PGB_SCREEN_MAX_PAGE_BYTES];
    line_gen_t gen = { 0, 12345u };
    int content_ok = 1;
    static const unsigned expected_lines[PAGES] = { 24, 24, 24, 24, 24, 24, 10 };
    for (uint32_t p = 0; p < PAGES; p++) {
        size_t len = 0;
        CHECK(pgb_page_read(&b, p, page, sizeof(page), &len) == PGB_OK);
        CHECK(len == entry_raw(DATA, p));
        unsigned lines_here = 0;
        size_t pos = 0;
        while (pos <= len) {
            size_t end = pos;
            while (end < len && page[end] != '\n') { end++; }
            uint8_t want[40];
            size_t want_len = next_line(&gen, want);
            if (end - pos != want_len || memcmp(page + pos, want, want_len) != 0) { content_ok = 0; }
            lines_here++;
            pos = end + 1u;
        }
        CHECK(lines_here == expected_lines[p]);
    }
    CHECK(content_ok);
    CHECK(gen.n == 154u);

    /* Какие страницы сжаты: случайная (индекс 5) хранится как есть, остальные сжаты. */
    unsigned packed = 0;
    for (uint32_t p = 0; p < PAGES; p++) {
        CHECK(entry_stored(DATA, p) <= entry_raw(DATA, p));
        if (entry_stored(DATA, p) < entry_raw(DATA, p)) { packed++; }
    }
    if (g_fx->compressed) {
        CHECK(packed >= 5u);
        CHECK(entry_stored(DATA, 5) == entry_raw(DATA, 5));
        CHECK(entry_stored(DATA, 0) < entry_raw(DATA, 0) / 2u);
    } else {
        CHECK(packed == 0u);
    }

    /* буфер: ровно по размеру страницы подходит, на байт меньше - нет */
    size_t len = 0;
    CHECK(pgb_page_read(&b, 6, page, sizeof(page), &len) == PGB_OK);
    size_t last_len = len;
    CHECK(pgb_page_read(&b, 6, page, last_len, &len) == PGB_OK && len == last_len);
    CHECK(pgb_page_read(&b, 6, page, last_len - 1u, &len) == PGB_ERR_BUFFER && len == 0u);
    CHECK(pgb_page_read(&b, 0, page, 100, &len) == PGB_ERR_BUFFER);

    CHECK(pgb_page_read(&b, PAGES, page, sizeof(page), &len) == PGB_ERR_PAGE_NUMBER);
    CHECK(pgb_page_read(&b, 0xFFFFFFFFu, page, sizeof(page), &len) == PGB_ERR_PAGE_NUMBER);
    CHECK(pgb_page_read(&b, 0, NULL, sizeof(page), &len) == PGB_ERR_IO);
    pgb_close(&b);
    CHECK(pgb_page_read(&b, 0, page, sizeof(page), &len) == PGB_ERR_IO);
    CHECK(fake_fs_open_handles() == 0);
}

static void test_file_size(void)
{
    pgb_book_t b;
    uint8_t big[MAX_FILE];

    put(DATA, SIZE - 1u);
    CHECK(open_fixture(&b) == PGB_ERR_SIZE);

    memcpy(big, DATA, SIZE);
    big[SIZE] = 0;
    put(big, SIZE + 1u);
    CHECK(open_fixture(&b) == PGB_ERR_SIZE);

    put(DATA, 30u);
    CHECK(open_fixture(&b) == PGB_ERR_IO);
    put(DATA, 0u);
    CHECK(open_fixture(&b) == PGB_ERR_IO);
    CHECK(fake_fs_open_handles() == 0);
}

/* ---- Повреждения ------------------------------------------------------------ */
static void test_meta_corruption(void)
{
    /*
     * Любой испорченный бит заголовка, метаданных или индекса не должен пройти
     * pgb_open. Исключение - сам file_crc32 (байты 32..35): при расчёте meta_crc32
     * он обнуляется, его порча видна только при полной проверке файла.
     */
    uint8_t data[MAX_FILE];
    int all_rejected = 1;
    for (uint32_t i = 0; i < OFF_DATA; i++) {
        if (i >= 32u && i < 36u) { continue; }
        for (int bit = 0; bit < 8; bit++) {
            memcpy(data, DATA, SIZE);
            data[i] ^= (uint8_t)(1u << bit);
            put(data, SIZE);
            pgb_book_t b;
            if (open_fixture(&b) == PGB_OK) { all_rejected = 0; pgb_close(&b); }
        }
    }
    CHECK(all_rejected);
    CHECK(fake_fs_open_handles() == 0);

    memcpy(data, DATA, SIZE);
    data[36] ^= 1;
    put(data, SIZE);
    pgb_book_t b;
    CHECK(open_fixture(&b) == PGB_ERR_META_CRC);

    memcpy(data, DATA, SIZE);
    data[32] ^= 1;
    put(data, SIZE);
    CHECK(open_fixture(&b) == PGB_OK);
    CHECK(pgb_verify_file(&b, NULL, NULL) == PGB_ERR_FILE_CRC);
    pgb_close(&b);
}

/* Без сжатия порча текста видна читателю напрямую. */
static void test_raw_page_corruption(void)
{
    uint8_t data[MAX_FILE];
    uint8_t page[PGB_SCREEN_MAX_PAGE_BYTES];
    size_t len;
    pgb_book_t b;

    memcpy(data, DATA, SIZE);
    uint32_t page2 = entry_offset(data, 2);
    data[page2 + 3u] = 0x01;
    put(data, SIZE);
    CHECK(open_fixture(&b) == PGB_OK);
    CHECK(pgb_page_read(&b, 2, page, sizeof(page), &len) == PGB_ERR_PAGE_DATA);
    CHECK(pgb_page_read(&b, 1, page, sizeof(page), &len) == PGB_OK);
    CHECK(pgb_verify_file(&b, NULL, NULL) == PGB_ERR_FILE_CRC);
    pgb_close(&b);

    memcpy(data, DATA, SIZE);
    data[page2 + 3u] ^= 0x01;
    put(data, SIZE);
    CHECK(open_fixture(&b) == PGB_OK);
    CHECK(pgb_page_read(&b, 2, page, sizeof(page), &len) == PGB_OK);
    CHECK(pgb_verify_file(&b, NULL, NULL) == PGB_ERR_FILE_CRC);
    pgb_close(&b);

    /* страница, оканчивающаяся на LF, и строка длиннее 29 байт (CRC пересчитаны) */
    memcpy(data, DATA, SIZE);
    data[SIZE - 1u] = '\n';
    reseal(data, SIZE);
    put(data, SIZE);
    CHECK(open_fixture(&b) == PGB_OK);
    CHECK(pgb_page_read(&b, 6, page, sizeof(page), &len) == PGB_ERR_PAGE_DATA);
    CHECK(pgb_verify_file(&b, NULL, NULL) == PGB_OK);
    pgb_close(&b);

    memcpy(data, DATA, SIZE);
    for (uint32_t i = 0; i < 30u; i++) { data[page2 + i] = 'A'; }
    reseal(data, SIZE);
    put(data, SIZE);
    CHECK(open_fixture(&b) == PGB_OK);
    CHECK(pgb_page_read(&b, 2, page, sizeof(page), &len) == PGB_ERR_PAGE_DATA);
    pgb_close(&b);
}

/* Сжатые книги: порча любого байта данных страниц не приводит к сбоям и всегда видна в file_crc32. */
static void test_lz4_page_corruption(void)
{
    uint8_t data[MAX_FILE];
    uint8_t page[PGB_SCREEN_MAX_PAGE_BYTES];
    int safe = 1, crc_seen = 1, others_ok = 1;

    for (uint32_t i = OFF_DATA; i < SIZE; i++) {
        uint32_t victim = 0;
        for (uint32_t p = 0; p < PAGES; p++) {
            if (i >= entry_offset(DATA, p) && i < entry_offset(DATA, p) + entry_stored(DATA, p)) { victim = p; }
        }
        memcpy(data, DATA, SIZE);
        data[i] ^= 0x5A;
        put(data, SIZE);
        pgb_book_t b;
        if (open_fixture(&b) != PGB_OK) { safe = 0; continue; }
        for (uint32_t p = 0; p < PAGES; p++) {
            size_t len = 0;
            pgb_status_t st = pgb_page_read(&b, p, page, sizeof(page), &len);
            if (p == victim) {
                if (st != PGB_OK && st != PGB_ERR_DECOMPRESS && st != PGB_ERR_PAGE_DATA) { safe = 0; }
            } else if (st != PGB_OK) {
                others_ok = 0;
            }
        }
        if (pgb_verify_file(&b, NULL, NULL) != PGB_ERR_FILE_CRC) { crc_seen = 0; }
        pgb_close(&b);
    }
    CHECK(safe);
    CHECK(crc_seen);
    CHECK(others_ok);
    CHECK(fake_fs_open_handles() == 0);
}

/* Распакованный текст страницы проходит те же проверки, что и обычный: LZ4 их не обходит. */
static void test_lz4_decoded_text_is_checked(void)
{
    uint8_t data[MAX_FILE];
    uint8_t page[PGB_SCREEN_MAX_PAGE_BYTES];
    size_t len;
    pgb_book_t b;

    /*
     * Последняя страница заменяется корректным блоком LZ4 на 7 байт:
     * литералы "ab?" и совпадение offset=3, длина 17 -> 20 байт текста "ab?ab?ab?...".
     * Файл укорачивается, стыковка с индексом и CRC сохраняются.
     */
    for (int variant = 0; variant < 2; variant++) {
        const uint8_t fill = (variant == 0) ? (uint8_t)' ' : (uint8_t)0x03;
        const uint8_t block[7] = { 0x3D, 'a', 'b', fill, 0x03, 0x00, 0x00 };
        memcpy(data, DATA, SIZE);
        uint32_t off = entry_offset(data, 6);
        memcpy(data + off, block, sizeof(block));
        data[OFF_INDEX + ENTRY * 6u + 4u] = 7; data[OFF_INDEX + ENTRY * 6u + 5u] = 0;
        data[OFF_INDEX + ENTRY * 6u + 6u] = 20; data[OFF_INDEX + ENTRY * 6u + 7u] = 0;
        uint32_t size = off + 7u;
        wr32(data + 28, size);
        reseal(data, size);
        put(data, size);
        CHECK(open_fixture(&b) == PGB_OK);
        pgb_status_t st = pgb_page_read(&b, 6, page, sizeof(page), &len);
        if (variant == 0) {
            CHECK(st == PGB_OK && len == 20u && memcmp(page, "ab ab ab ab ab ab ab", 20) == 0);
        } else {
            CHECK(st == PGB_ERR_PAGE_DATA);      /* блок корректен, но текст содержит код 0x03 */
        }
        CHECK(pgb_verify_file(&b, NULL, NULL) == PGB_OK);
        pgb_close(&b);
    }
}

static void test_tampered_index(void)
{
    uint8_t data[MAX_FILE];
    uint8_t page[PGB_SCREEN_MAX_PAGE_BYTES];
    size_t len;
    pgb_book_t b;

    /* Индекс подменён, CRC пересчитаны: открытие проходит, но стыковка страниц нарушена. */
    memcpy(data, DATA, SIZE);
    wr32(data + OFF_INDEX + ENTRY, entry_offset(DATA, 1) + 1u);
    reseal(data, SIZE);
    put(data, SIZE);
    CHECK(open_fixture(&b) == PGB_OK);
    CHECK(pgb_page_read(&b, 0, page, sizeof(page), &len) == PGB_ERR_INDEX);
    CHECK(pgb_page_read(&b, 1, page, sizeof(page), &len) == PGB_ERR_INDEX);
    CHECK(pgb_page_read(&b, 2, page, sizeof(page), &len) == PGB_OK);
    pgb_close(&b);

    memcpy(data, DATA, SIZE);
    wr32(data + OFF_INDEX, OFF_DATA + 1u);
    reseal(data, SIZE);
    put(data, SIZE);
    CHECK(open_fixture(&b) == PGB_OK);
    CHECK(pgb_page_read(&b, 0, page, sizeof(page), &len) == PGB_ERR_INDEX);
    pgb_close(&b);

    /* нулевые размеры, raw_size изменён, размер больше допустимого, stored > raw */
    for (int variant = 0; variant < 4; variant++) {
        memcpy(data, DATA, SIZE);
        uint8_t *entry = data + OFF_INDEX + ENTRY * 3u;
        if (variant == 0) { entry[4] = 0; entry[5] = 0; entry[6] = 0; entry[7] = 0; }
        if (variant == 1) { entry[6] ^= 1; }
        if (variant == 2) { entry[4] = 0xC0; entry[5] = 0x03; entry[6] = 0xC0; entry[7] = 0x03; }   /* 960 > 719 */
        if (variant == 3) { entry[6] = (uint8_t)(entry[6] - 1u); entry[7] = entry[7]; }              /* raw на 1 меньше */
        reseal(data, SIZE);
        put(data, SIZE);
        CHECK(open_fixture(&b) == PGB_OK);
        pgb_status_t st = pgb_page_read(&b, 3, page, sizeof(page), &len);
        CHECK(st != PGB_OK);
        if (variant == 0 || variant == 2) { CHECK(st == PGB_ERR_INDEX); }
        pgb_close(&b);
    }

    /* последняя страница выходит за file_size */
    memcpy(data, DATA, SIZE);
    data[OFF_INDEX + ENTRY * 6u + 4u] += 1;
    data[OFF_INDEX + ENTRY * 6u + 6u] += 1;
    reseal(data, SIZE);
    put(data, SIZE);
    CHECK(open_fixture(&b) == PGB_OK);
    CHECK(pgb_page_read(&b, 6, page, sizeof(page), &len) == PGB_ERR_INDEX);
    pgb_close(&b);
    CHECK(fake_fs_open_handles() == 0);
}

/* Книга без сжатия с флагом LZ4: страницы «как есть» остаются корректными; геометрия больше scratch отклоняется. */
static void test_lz4_flag_and_geometry(void)
{
    uint8_t data[MAX_FILE];
    uint8_t page[PGB_SCREEN_MAX_PAGE_BYTES];
    size_t len;
    pgb_book_t b;

    memcpy(data, DATA, SIZE);
    data[42] = PGB_COMPRESSION_LZ4;
    reseal(data, SIZE);
    put(data, SIZE);
    CHECK(open_fixture(&b) == PGB_OK);
    CHECK(pgb_page_read(&b, 0, page, sizeof(page), &len) == PGB_OK);
    pgb_close(&b);

    data[40] = 255;    /* cols = 255: страница (255 x 24 + 23 байта) могла бы быть длиннее scratch */
    reseal(data, SIZE);
    put(data, SIZE);
    CHECK(pgb_open(BOOK, 255, 24, &b) == PGB_ERR_GEOMETRY && b.file == NULL);

    /* без сжатия геометрия шире экрана допустима (scratch не нужен) */
    memcpy(data, DATA, SIZE);
    data[40] = 255;
    reseal(data, SIZE);
    put(data, SIZE);
    CHECK(pgb_open(BOOK, 255, 24, &b) == PGB_OK);
    pgb_close(&b);

    /* окно картинок должно совпадать с экраном читалки */
    memcpy(data, DATA, SIZE);
    data[48] = 100;    /* img_w = 100 */
    reseal(data, SIZE);
    put(data, SIZE);
    CHECK(open_fixture(&b) == PGB_ERR_GEOMETRY && b.file == NULL);
    memcpy(data, DATA, SIZE);
    data[50] = 0; data[51] = 2;    /* img_h = 512 */
    reseal(data, SIZE);
    put(data, SIZE);
    CHECK(open_fixture(&b) == PGB_ERR_GEOMETRY && b.file == NULL);
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
    put_fixture();
    CHECK(open_fixture(&b) == PGB_OK);
    CHECK(pgb_verify_file(&b, NULL, NULL) == PGB_OK);
    g_progress_calls = 0; g_progress_last = 0;
    CHECK(pgb_verify_file(&b, progress_ok, NULL) == PGB_OK);
    CHECK(g_progress_calls == (int)((SIZE + 511u) / 512u) && g_progress_last == SIZE);
    CHECK(pgb_verify_file(&b, progress_cancel, NULL) == PGB_ERR_CANCELLED);
    pgb_close(&b);
    CHECK(pgb_verify_file(&b, NULL, NULL) == PGB_ERR_IO);
}

static void test_status_text(void)
{
    int ok = 1;
    for (int s = PGB_OK; s <= PGB_ERR_NOT_CACHED; s++) {
        const char *t = pgb_status_text((pgb_status_t)s);
        if (!t || !t[0]) { ok = 0; }
    }
    CHECK(ok);
    CHECK(pgb_status_text((pgb_status_t)99) != NULL);
}

/*
 * Образ книги в памяти (читалка): страницы из RAM совпадают с pgb_page_read();
 * окно данных - граница в байт; повреждения не выходят за буферы; meta_crc32
 * по образу.
 */
static void test_memory_image(void)
{
    pgb_header_t h;
    CHECK(pgb_header_decode(DATA, SIZE, &h) == PGB_OK);
    CHECK(pgb_meta_check_mem(&h, DATA) == PGB_OK);
    CHECK(pgb_meta_check_mem(NULL, DATA) == PGB_ERR_IO);

    /* Каждый бит области заголовок + метаданные + индекс ловится по meta_crc32. */
    {
        uint8_t copy[MAX_FILE];
        memcpy(copy, DATA, SIZE);
        int missed = 0;
        for (uint32_t i = 0; i < h.data_offset; i++) {
            if (i >= 32 && i < 40) { continue; }   /* сами поля CRC считаются нулевыми */
            for (int bit = 0; bit < 8; bit++) {
                copy[i] ^= (uint8_t)(1u << bit);
                if (pgb_meta_check_mem(&h, copy) == PGB_OK) { missed++; }
                copy[i] ^= (uint8_t)(1u << bit);
            }
        }
        CHECK(missed == 0);
    }

    for (uint32_t n = 0; n < h.page_count; n++) {
        uint8_t ref[1024], mem[1024];
        size_t ref_len = 0, mem_len = 0;
        pgb_index_entry_t e;
        CHECK(pgb_page_locate(&h, DATA, n, &e) == PGB_OK);

        pgb_book_t book;
        CHECK(pgb_open(BOOK, 29, 24, &book) == PGB_OK);
        CHECK(pgb_page_read(&book, n, ref, sizeof ref, &ref_len) == PGB_OK);
        pgb_close(&book);

        /* Целиком книга, ровно окно страницы, окно короче на байт с любого края. */
        CHECK(pgb_page_read_mem(&h, DATA, DATA + h.data_offset, h.data_offset, SIZE - h.data_offset,
                                n, mem, sizeof mem, &mem_len) == PGB_OK);
        CHECK(mem_len == ref_len && memcmp(mem, ref, ref_len) == 0);
        CHECK(pgb_page_read_mem(&h, DATA, DATA + e.offset, e.offset, e.stored_size,
                                n, mem, sizeof mem, &mem_len) == PGB_OK);
        CHECK(mem_len == ref_len && memcmp(mem, ref, ref_len) == 0);
        CHECK(pgb_page_read_mem(&h, DATA, DATA + e.offset + 1u, e.offset + 1u, e.stored_size - 1u,
                                n, mem, sizeof mem, &mem_len) == PGB_ERR_NOT_CACHED && mem_len == 0);
        CHECK(pgb_page_read_mem(&h, DATA, DATA + e.offset, e.offset, e.stored_size - 1u,
                                n, mem, sizeof mem, &mem_len) == PGB_ERR_NOT_CACHED);
        CHECK(pgb_page_read_mem(&h, DATA, DATA, 0xFFFFFFF0u, 0x20u,
                                n, mem, sizeof mem, &mem_len) == PGB_ERR_NOT_CACHED);   /* без переполнения */
        CHECK(pgb_page_read_mem(&h, DATA, DATA, h.data_offset, 0u,
                                n, mem, sizeof mem, &mem_len) == PGB_ERR_NOT_CACHED);
        /* Буфер меньше страницы. */
        CHECK(pgb_page_read_mem(&h, DATA, DATA + h.data_offset, h.data_offset, SIZE - h.data_offset,
                                n, mem, (size_t)e.raw_size - 1u, &mem_len) == PGB_ERR_BUFFER);
    }
    uint8_t buf[1024];
    size_t len = 0;
    CHECK(pgb_page_read_mem(&h, DATA, DATA + h.data_offset, h.data_offset, SIZE - h.data_offset,
                            h.page_count, buf, sizeof buf, &len) == PGB_ERR_PAGE_NUMBER);
    pgb_index_entry_t e;
    CHECK(pgb_page_locate(&h, DATA, h.page_count, &e) == PGB_ERR_PAGE_NUMBER);
    CHECK(pgb_page_read_mem(NULL, DATA, DATA, 0, 0, 0, buf, sizeof buf, &len) == PGB_ERR_IO);
    CHECK(pgb_page_read_mem(&h, DATA, DATA, h.data_offset, 0, 0, buf, sizeof buf, NULL) == PGB_ERR_IO);
    CHECK(strcmp(pgb_status_text(PGB_ERR_NOT_CACHED), "unknown error") != 0);

    /* Каждый байт данных страниц портится: ошибка или иной текст, но не выход за буфер (ASan/UBSan). */
    {
        uint8_t copy[MAX_FILE];
        memcpy(copy, DATA, SIZE);
        for (uint32_t i = h.data_offset; i < SIZE; i++) {
            copy[i] ^= 0xFFu;
            for (uint32_t n = 0; n < h.page_count; n++) {
                size_t l = 0;
                pgb_status_t st = pgb_page_read_mem(&h, DATA, copy + h.data_offset, h.data_offset,
                                                    SIZE - h.data_offset, n, buf, sizeof buf, &l);
                CHECK(st == PGB_OK ? (l > 0 && l <= sizeof buf) : l == 0);
            }
            copy[i] ^= 0xFFu;
        }
    }
}


/* ---- Страницы-картинки (sample_img.pgb) -------------------------------------- */

#define IMG_BOOK_PAGES 4u
#define IMG_OFF_DATA   (OFF_INDEX + ENTRY * IMG_BOOK_PAGES)   /* название «Image book» 10, автор «Pic» 3 */

static uint8_t g_img[MAX_FILE];
static uint32_t g_img_size;

static int load_image_fixture(void)
{
    char path[256];
    snprintf(path, sizeof(path), TEST_DATA_DIR "/sample_img.pgb");
    FILE *f = fopen(path, "rb");
    if (!f) { printf("не найден %s (python3 tests/host/data/make_sample_pgb.py)\n", path); return 0; }
    g_img_size = (uint32_t)fread(g_img, 1, MAX_FILE, f);
    fclose(f);
    return g_img_size > 0u && g_img_size < MAX_FILE;
}

/* Картинки sample_img.pgb: 0 - полосы 176x256, 2 - шум 13x5 (seed 777), 3 - шум 100x60 (seed 4242). */
static int expected_black(unsigned page, unsigned w, unsigned x, unsigned y)
{
    if (page == 0u) { return ((x + y) / 8u) % 2u == 0u; }
    uint32_t state = (page == 2u) ? 777u : 4242u;
    for (unsigned i = 0; i <= y * w + x; i++) { state = state * 1664525u + 1013904223u; }
    return (int)((state >> 16) & 1u);
}

static int image_matches(const uint8_t *bits, unsigned page, unsigned w, unsigned h)
{
    const unsigned row_bytes = PGB_IMG_ROW_BYTES(w);
    for (unsigned y = 0; y < h; y++) {
        for (unsigned x = 0; x < w; x++) {
            const int got = (bits[y * row_bytes + x / 8u] >> (7u - x % 8u)) & 1;
            if (got != expected_black(page, w, x, y)) { return 0; }
        }
    }
    return 1;
}

static void put_img(const uint8_t *data, uint32_t size) { put(data, size); }

static void test_image_pages(void)
{
    static const struct { uint16_t w, h; } dims[IMG_BOOK_PAGES] = { { 176, 256 }, { 0, 0 }, { 13, 5 }, { 100, 60 } };
    uint8_t page[PGB_SCREEN_MAX_PAGE_BYTES];
    size_t len = 0;
    pgb_book_t b;
    pgb_header_t h;

    CHECK(PGB_SCREEN_MAX_PAGE_BYTES == 5632u && PGB_SCREEN_MAX_TEXT_BYTES == 719u);
    CHECK(PGB_IMG_ROW_BYTES(176) == 22u && PGB_IMG_ROW_BYTES(13) == 2u && PGB_IMG_ROW_BYTES(8) == 1u);

    CHECK(pgb_header_decode(g_img, g_img_size, &h) == PGB_OK);
    CHECK(h.page_count == IMG_BOOK_PAGES && h.img_w == 176u && h.img_h == 256u && h.data_offset == IMG_OFF_DATA);

    put_img(g_img, g_img_size);
    CHECK(pgb_open(BOOK, PGB_SCREEN_COLS, PGB_SCREEN_ROWS, &b) == PGB_OK);
    CHECK(pgb_verify_file(&b, NULL, NULL) == PGB_OK);

    for (uint32_t p = 0; p < IMG_BOOK_PAGES; p++) {
        pgb_index_entry_t e;
        CHECK(pgb_page_locate(&h, g_img, p, &e) == PGB_OK);
        CHECK(e.width == dims[p].w && e.height == dims[p].h);
        CHECK(PGB_ENTRY_IS_IMAGE(&e) == (dims[p].w != 0u));
        CHECK(pgb_page_read(&b, p, page, sizeof(page), &len) == PGB_OK);
        CHECK(len == e.raw_size);
        if (dims[p].w != 0u) {
            CHECK(len == PGB_MAX_IMAGE_BYTES(dims[p].w, dims[p].h));
            CHECK(image_matches(page, p, dims[p].w, dims[p].h));
        } else {
            CHECK(len == 14u && memcmp(page, "Between\nimages", 14) == 0);
        }
        /* то же из образа в памяти */
        uint8_t mem[PGB_SCREEN_MAX_PAGE_BYTES];
        size_t mem_len = 0;
        CHECK(pgb_page_read_mem(&h, g_img, g_img + h.data_offset, h.data_offset, g_img_size - h.data_offset,
                                p, mem, sizeof(mem), &mem_len) == PGB_OK);
        CHECK(mem_len == len && memcmp(mem, page, len) == 0);
        /* буфер меньше страницы */
        CHECK(pgb_page_read(&b, p, page, e.raw_size - 1u, &len) == PGB_ERR_BUFFER);
    }
    /* Полосы сжимаются, шум хранится как есть. */
    CHECK(rd16(g_img + OFF_INDEX + 4u) < rd16(g_img + OFF_INDEX + 6u) / 4u);
    CHECK(rd16(g_img + OFF_INDEX + ENTRY * 3u + 4u) == rd16(g_img + OFF_INDEX + ENTRY * 3u + 6u));
    pgb_close(&b);

    /* Список книг видит окно картинок. */
    pgb_info_t info;
    CHECK(pgb_read_info(BOOK, &info) == PGB_OK && info.img_w == 176u && info.img_h == 256u && info.page_count == 4u);

    /* Проверка картинок: без повреждений каждая картинка проходит pgb_page_check_image(). */
    CHECK(pgb_page_check_image(g_img + 0, 0, 0, 0) == PGB_ERR_PAGE_DATA);
    {
        uint8_t px[4] = { 0xFF, 0xF8, 0xFF, 0xF8 };                  /* 13x2, биты выравнивания нулевые */
        CHECK(pgb_page_check_image(px, 4, 13, 2) == PGB_OK);
        CHECK(pgb_page_check_image(px, 3, 13, 2) == PGB_ERR_PAGE_DATA);   /* размер не тот */
        px[3] = 0xF9;
        CHECK(pgb_page_check_image(px, 4, 13, 2) == PGB_ERR_PAGE_DATA);   /* бит выравнивания */
        CHECK(pgb_page_check_image(px, 4, 16, 2) == PGB_OK);              /* ширина кратна 8: выравнивания нет */
    }

    /* Порча записей индекса при пересчитанных CRC: открытие проходит, чтение страницы - нет. */
    static const struct { uint32_t page, field; uint16_t value; pgb_status_t want; } bad[] = {
        { 0, 8, 0, PGB_ERR_INDEX },       /* width = 0 при height != 0 */
        { 0, 10, 0, PGB_ERR_INDEX },      /* height = 0 при width != 0 */
        { 0, 8, 177, PGB_ERR_INDEX },     /* шире окна */
        { 0, 10, 257, PGB_ERR_INDEX },    /* выше окна */
        { 0, 8, 100, PGB_ERR_INDEX },     /* raw_size не соответствует размерам */
        { 1, 8, 5, PGB_ERR_INDEX },       /* текстовая страница с width != 0, height == 0 */
        { 1, 10, 5, PGB_ERR_INDEX },
        { 3, 10, 59, PGB_ERR_INDEX },
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        uint8_t data[MAX_FILE];
        memcpy(data, g_img, g_img_size);
        data[OFF_INDEX + ENTRY * bad[i].page + bad[i].field] = (uint8_t)bad[i].value;
        data[OFF_INDEX + ENTRY * bad[i].page + bad[i].field + 1u] = (uint8_t)(bad[i].value >> 8);
        reseal_at(data, g_img_size, IMG_OFF_DATA);
        put_img(data, g_img_size);
        CHECK(pgb_open(BOOK, PGB_SCREEN_COLS, PGB_SCREEN_ROWS, &b) == PGB_OK);
        CHECK(pgb_page_read(&b, bad[i].page, page, sizeof(page), &len) == bad[i].want && len == 0u);
        pgb_close(&b);
        CHECK(pgb_page_locate(&h, data, bad[i].page, &(pgb_index_entry_t){ 0 }) == bad[i].want);
    }

    /* Ненулевой бит выравнивания в странице 13x5 (хранится как есть): PGB_ERR_PAGE_DATA. */
    {
        uint8_t data[MAX_FILE];
        memcpy(data, g_img, g_img_size);
        const uint32_t off = rd32(data + OFF_INDEX + ENTRY * 2u);
        data[off + 1u] |= 0x01u;     /* младший бит первого ряда: правее ширины 13 */
        reseal_at(data, g_img_size, IMG_OFF_DATA);
        put_img(data, g_img_size);
        CHECK(pgb_open(BOOK, PGB_SCREEN_COLS, PGB_SCREEN_ROWS, &b) == PGB_OK);
        CHECK(pgb_page_read(&b, 2, page, sizeof(page), &len) == PGB_ERR_PAGE_DATA && len == 0u);
        CHECK(pgb_page_read(&b, 3, page, sizeof(page), &len) == PGB_OK);
        pgb_close(&b);
        CHECK(pgb_page_read_mem(&h, data, data + IMG_OFF_DATA, IMG_OFF_DATA, g_img_size - IMG_OFF_DATA,
                                2, page, sizeof(page), &len) == PGB_ERR_PAGE_DATA);
    }

    /* Каждый байт данных страниц портится: ошибка или иное содержимое, но без выхода за буферы (ASan/UBSan). */
    {
        uint8_t copy[MAX_FILE];
        memcpy(copy, g_img, g_img_size);
        for (uint32_t i = IMG_OFF_DATA; i < g_img_size; i++) {
            copy[i] ^= 0xFFu;
            for (uint32_t n = 0; n < IMG_BOOK_PAGES; n++) {
                size_t l = 0;
                pgb_status_t st = pgb_page_read_mem(&h, g_img, copy + IMG_OFF_DATA, IMG_OFF_DATA,
                                                    g_img_size - IMG_OFF_DATA, n, page, sizeof(page), &l);
                CHECK(st == PGB_OK ? (l > 0u && l <= sizeof(page)) : l == 0u);
            }
            copy[i] ^= 0xFFu;
        }
    }

    /* Сжатая картинка с окном 176x256 не помещается в scratch меньшего размера: проверено при открытии. */
    {
        uint8_t data[MAX_FILE];
        memcpy(data, g_img, g_img_size);
        data[48] = 100;
        reseal_at(data, g_img_size, IMG_OFF_DATA);
        put_img(data, g_img_size);
        CHECK(pgb_open(BOOK, PGB_SCREEN_COLS, PGB_SCREEN_ROWS, &b) == PGB_ERR_GEOMETRY && b.file == NULL);
    }
    CHECK(fake_fs_open_handles() == 0);
}

int main(void)
{
    if (!load_fixtures() || !load_image_fixture()) { return 1; }

    test_crc();
    test_lz4_vectors();
    test_status_text();

    for (size_t i = 0; i < sizeof(g_fix) / sizeof(g_fix[0]); i++) {
        g_fx = &g_fix[i];
        printf("-- %s\n", g_fx->file);
        test_header_decode();
        test_open_and_info();
        test_read_all_pages();
        test_file_size();
        test_meta_corruption();
        test_tampered_index();
        test_verify_file();
        test_memory_image();
        if (g_fx->compressed) {
            test_lz4_fuzz();
            test_lz4_page_corruption();
            test_lz4_decoded_text_is_checked();
        } else {
            test_raw_page_corruption();
            test_lz4_flag_and_geometry();
        }
    }
    printf("-- sample_img.pgb\n");
    test_image_pages();
    return test_finish("test_pgb");
}
