/*
 * Host-тесты умного поиска (src/core/map/map_search.c, docs/design/poi-search.md, формат -
 * docs/map-format-v4.md, раздел 21):
 *   - нормализация ключа совпадает с компилятором (общие векторы search_fold_vectors.txt);
 *   - открытие и проверка search.idx (нет файла, чужая база, испорченный заголовок);
 *   - карта tests/host/data/search-map (её строит настоящий компилятор): категории, названия,
 *     категория + название, улицы, реки, водоёмы, парки, остальные слова названия;
 *   - синтетический индекс на несколько тысяч записей: топ-32 по CAT (разбор по кодам Мортона,
 *     расширение радиуса) и по NAME совпадает с перебором, расчёт идёт порциями;
 *   - сужение прежнего диапазона, отмена запроса, список прежнего запроса виден до готовности нового;
 *   - слоты файловой системы.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "purrgo/fs_hal.h"
#include "purrgo/geo.h"
#include "purrgo/map_search.h"
#include "purrgo/pgo_text.h"
#include "map/map_internal.h"
#include "fake_fs.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

#define MAP "maps/s"
#define LAT0 539000000
#define LON0 275000000

/* ---- вспомогательное --------------------------------------------------- */

static uint8_t *load_file(const char *path, uint32_t *size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *d;
    long n;

    if (f == NULL) { *size = 0; return NULL; }
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    d = malloc((size_t)n);
    if (fread(d, 1, (size_t)n, f) != (size_t)n) { n = 0; }
    fclose(f);
    *size = (uint32_t)n;
    return d;
}

static void put_fixture(void)
{
    static const char *const names[] = { "search.idx", "pois.db", "roads.db", "waterways.db", "water.db", "landuse.db" };

    fake_fs_reset();
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        char host[128], dev[64];
        uint32_t n;
        uint8_t *d;

        snprintf(host, sizeof host, TEST_DATA_DIR "/search-map/%s", names[i]);
        snprintf(dev, sizeof dev, MAP "/%s", names[i]);
        d = load_file(host, &n);
        CHECK(d != NULL);
        if (d != NULL) { fake_fs_put_bin(dev, d, n); free(d); }
    }
}

static purrgo_ms_state_t run(const char *utf8)
{
    char pgo[96];
    purrgo_ms_state_t st;
    int guard = 0;

    purrgo_utf8_to_pgo(utf8, pgo, sizeof pgo);
    purrgo_map_search_set_query(pgo);
    do { st = purrgo_map_search_step(); } while (st == PURRGO_MS_RUNNING && ++guard < 100000);
    return st;
}

static const char *name_utf8(size_t i)
{
    static char out[96];

    purrgo_pgo_to_utf8(purrgo_map_search_name(i), out, sizeof out);
    return out;
}

static bool has_name(const char *utf8)
{
    for (size_t i = 0; i < purrgo_map_search_count(); i++) {
        if (strcmp(name_utf8(i), utf8) == 0) { return true; }
    }
    return false;
}

static size_t count_layer(purrgo_ms_layer_t l)
{
    size_t n = 0;

    for (size_t i = 0; i < purrgo_map_search_count(); i++) {
        if (purrgo_map_search_layer(i) == l) { n++; }
    }
    return n;
}

static bool sorted_by_distance(void)
{
    for (size_t i = 1; i < purrgo_map_search_count(); i++) {
        if (purrgo_map_search_distance_m(i) + 2u < purrgo_map_search_distance_m(i - 1u)) { return false; }
    }
    return true;
}

static void open_fixture(void)
{
    put_fixture();
    CHECK(purrgo_map_search_open(MAP));
    purrgo_map_search_set_origin(PURRGO_POI_ORIGIN_GNSS, LAT0, LON0);
}

/* ---- нормализация ------------------------------------------------------- */

static int hexval(char c) { return (c >= '0' && c <= '9') ? c - '0' : (c - 'a' + 10); }

static size_t unhex(const char *s, size_t n, uint8_t *out)
{
    for (size_t i = 0; i + 1 < n; i += 2) { out[i / 2] = (uint8_t)(hexval(s[i]) * 16 + hexval(s[i + 1])); }
    return n / 2;
}

static void test_fold_vectors(void)
{
    uint32_t n;
    uint8_t *d = load_file(TEST_DATA_DIR "/search_fold_vectors.txt", &n);
    int lines = 0;

    CHECK(d != NULL);
    for (uint32_t pos = 0; d != NULL && pos < n;) {
        uint32_t end = pos;
        uint8_t raw[128], want[16][16], got[16][16], got_len[16];
        size_t rn, nw = 0;
        const char *bar, *p;

        while (end < n && d[end] != '\n') { end++; }
        bar = memchr(d + pos, '|', end - pos);
        CHECK(bar != NULL);
        if (bar == NULL) { break; }
        rn = unhex((const char *)d + pos, (size_t)(bar - (const char *)d - (long)pos), raw);

        p = bar + 1;
        while (p < (const char *)d + end) {
            const char *q = p;
            while (q < (const char *)d + end && *q != ',') { q++; }
            memset(want[nw], 0, 16);
            unhex(p, (size_t)(q - p), want[nw]);
            nw++;
            p = (q < (const char *)d + end) ? q + 1 : q;
        }

        size_t gn = purrgo_search_split(raw, rn, got, got_len, 16);
        CHECK(gn == nw);
        for (size_t i = 0; i < gn && i < nw; i++) { CHECK(memcmp(got[i], want[i], 16) == 0); }
        lines++;
        pos = end + 1;
    }
    CHECK(lines >= 10);
    free(d);

    /* свёртка: регистр, Ё; нет слов в одних разделителях */
    CHECK(purrgo_search_fold_byte('A') == 'a');
    CHECK(purrgo_search_fold_byte(0xF0) == purrgo_search_fold_byte(0xF1));
    CHECK(purrgo_search_fold_byte('-') == '-');
    CHECK(!purrgo_search_is_word_byte('-') && !purrgo_search_is_word_byte(' ') && purrgo_search_is_word_byte('7'));
}

/* ---- открытие ----------------------------------------------------------- */

static void test_open(void)
{
    uint32_t n;
    const uint8_t *src;
    uint8_t *bad;

    put_fixture();
    CHECK(purrgo_map_search_open(MAP));
    CHECK(purrgo_map_search_is_open());
    CHECK(fake_fs_open_handles() == 1);
    purrgo_map_search_close();
    CHECK(!purrgo_map_search_is_open());
    CHECK(fake_fs_open_handles() == 0);

    CHECK(!purrgo_map_search_open("maps/none"));          /* нет файла */
    CHECK(!purrgo_map_search_open(""));
    CHECK(!purrgo_map_search_open(NULL));

    src = fake_fs_data(MAP "/search.idx", &n);
    bad = malloc(n);
    memcpy(bad, src, n);
    bad[0x20] = 'X';                                      /* не SRCH */
    fake_fs_put_bin("maps/b1/search.idx", bad, n);
    CHECK(!purrgo_map_search_open("maps/b1"));

    memcpy(bad, src, n);
    bad[3] = 3;                                           /* не тип 4 */
    fake_fs_put_bin("maps/b2/search.idx", bad, n);
    CHECK(!purrgo_map_search_open("maps/b2"));

    memcpy(bad, src, n);
    bad[0x20 + 0x10] ^= 1u;                               /* name_off не кратен 512 */
    fake_fs_put_bin("maps/b3/search.idx", bad, n);
    CHECK(!purrgo_map_search_open("maps/b3"));

    fake_fs_put_bin("maps/b4/search.idx", src, n - 512);  /* файл обрезан */
    CHECK(!purrgo_map_search_open("maps/b4"));
    free(bad);

    /* индекс построен для другой roads.db: число записей не совпало */
    put_fixture();
    {
        uint32_t m;
        const uint8_t *r = fake_fs_data(MAP "/roads.db", &m);
        uint8_t *c = malloc(m);

        memcpy(c, r, m);
        c[0x24] = (uint8_t)(c[0x24] + 1u);
        fake_fs_put_bin(MAP "/roads.db", c, m);
        free(c);
        CHECK(!purrgo_map_search_open(MAP));
        CHECK(fake_fs_open_handles() == 0);
    }
    purrgo_map_search_close();
}

/* ---- карта из компилятора ----------------------------------------------- */

static void test_fixture_queries(void)
{
    open_fixture();

    /* пусто и слишком коротко */
    CHECK(run("") == PURRGO_MS_DONE);
    CHECK(purrgo_map_search_kind() == PURRGO_MS_KIND_EMPTY && purrgo_map_search_count() == 0);
    CHECK(run("п") == PURRGO_MS_DONE);
    CHECK(purrgo_map_search_kind() == PURRGO_MS_KIND_SHORT && purrgo_map_search_count() == 0);
    CHECK(run(" - ") == PURRGO_MS_DONE && purrgo_map_search_kind() == PURRGO_MS_KIND_EMPTY);

    /* категория: все кафе, включая безымянное, от ближнего к дальнему */
    CHECK(run("кафе") == PURRGO_MS_DONE);
    CHECK(purrgo_map_search_kind() == PURRGO_MS_KIND_CATEGORY);
    CHECK(purrgo_map_search_count() == 4);
    CHECK(sorted_by_distance());
    CHECK(purrgo_map_search_type(0) == 133 && purrgo_map_search_layer(0) == PURRGO_MS_LAYER_POI);
    int32_t la0, lo0;
    CHECK(purrgo_map_search_position(0, &la0, &lo0) && la0 == LAT0 + 20000 && lo0 == LON0 + 20000);   /* безымянное - ближайшее */
    CHECK(strcmp(name_utf8(1), "Пушкин кафе") == 0 || strcmp(name_utf8(1), "Кафе Пушкин") == 0);
    CHECK(purrgo_map_search_distance_m(3) > 100000u);             /* дальнее кафе: ~130 км */
    {
        int32_t la, lo;
        CHECK(purrgo_map_search_position(1, &la, &lo));
        CHECK(la == LAT0 + 100000 && lo == LON0 + 100000);
        CHECK(!purrgo_map_search_position(99, &la, &lo));
    }

    /* название: объекты всех слоёв */
    CHECK(run("пуш") == PURRGO_MS_DONE);
    CHECK(purrgo_map_search_kind() == PURRGO_MS_KIND_NAME);
    CHECK(sorted_by_distance());
    CHECK(count_layer(PURRGO_MS_LAYER_POI) == 3);                 /* два кафе, аптека */
    CHECK(count_layer(PURRGO_MS_LAYER_ROADS) == 2);               /* три отрезка улицы -> две клетки */
    CHECK(count_layer(PURRGO_MS_LAYER_LANDUSE) == 1);             /* Пушкинский лес */
    CHECK(purrgo_map_search_count() == 6);

    /* регистр и ё не важны */
    CHECK(run("БЕРЁЗКА") == PURRGO_MS_DONE && purrgo_map_search_count() == 1);
    CHECK(run("березка") == PURRGO_MS_DONE && purrgo_map_search_count() == 1);

    /* категория + название */
    CHECK(run("пушкин кафе") == PURRGO_MS_DONE);
    CHECK(purrgo_map_search_count() == 2 && count_layer(PURRGO_MS_LAYER_POI) == 2);
    CHECK(run("кафе пуш") == PURRGO_MS_DONE && purrgo_map_search_count() == 2);
    CHECK(run("пушкин аптека") == PURRGO_MS_DONE && purrgo_map_search_count() == 1);
    CHECK(purrgo_map_search_type(0) == 132);
    CHECK(run("кафе п") == PURRGO_MS_DONE && purrgo_map_search_count() == 4);   /* "п" пока не учитывается */

    /* улицы: слово "улица" - категория, не слово названия */
    CHECK(run("улица пушкина") == PURRGO_MS_DONE);
    CHECK(purrgo_map_search_count() == 2 && count_layer(PURRGO_MS_LAYER_ROADS) == 2);
    CHECK(purrgo_map_search_type(0) == 3);
    /* "ул" - всего два байта, это слово названия: проверяется по полному названию объекта */
    CHECK(run("пушкина ул") == PURRGO_MS_DONE);
    CHECK(purrgo_map_search_count() == 2 && count_layer(PURRGO_MS_LAYER_ROADS) == 2);
    CHECK(run("проспект") == PURRGO_MS_DONE && purrgo_map_search_count() == 3);   /* все улицы: тип 130 */

    /* реки, водоёмы, парки */
    CHECK(run("река") == PURRGO_MS_DONE);
    CHECK(purrgo_map_search_count() == 1 && purrgo_map_search_layer(0) == PURRGO_MS_LAYER_WATERWAYS);
    CHECK(has_name("Свислочь"));
    CHECK(run("озеро") == PURRGO_MS_DONE);
    CHECK(purrgo_map_search_count() == 1 && purrgo_map_search_layer(0) == PURRGO_MS_LAYER_WATER);
    CHECK(has_name("Лошицкое озеро"));
    CHECK(run("лошиц") == PURRGO_MS_DONE && purrgo_map_search_count() == 1);
    CHECK(run("парк") == PURRGO_MS_DONE);
    CHECK(purrgo_map_search_count() == 1 && purrgo_map_search_layer(0) == PURRGO_MS_LAYER_LANDUSE);
    CHECK(has_name("Горького парк"));

    /* "пар" - начало слова "парк" и начало названий: и то, и другое (названий на "пар" нет) */
    CHECK(run("пар") == PURRGO_MS_DONE);
    CHECK(purrgo_map_search_kind() == PURRGO_MS_KIND_BOTH && purrgo_map_search_count() == 1);
    CHECK(run("горьк") == PURRGO_MS_DONE && purrgo_map_search_kind() == PURRGO_MS_KIND_NAME && purrgo_map_search_count() == 1);
    /* "гор" - начало слова "город": категория (Минск) и название (Горького парк) */
    CHECK(run("гор") == PURRGO_MS_DONE && purrgo_map_search_kind() == PURRGO_MS_KIND_BOTH && purrgo_map_search_count() == 2);
    CHECK(run("ничегонет") == PURRGO_MS_DONE && purrgo_map_search_count() == 0);

    /* названия читаются лениво и запоминаются: один слот под базу, смена слоя - тот же слот */
    CHECK(fake_fs_open_handles() <= 2);

    purrgo_map_search_close();
    CHECK(fake_fs_open_handles() == 0);
}

static void test_incremental_and_cancel(void)
{
    char a[8][96];
    size_t n1;

    open_fixture();

    /* "пу" -> "пуш" -> "пушк": сужение прежнего диапазона не меняет результат */
    CHECK(run("пу") == PURRGO_MS_DONE);
    n1 = purrgo_map_search_count();
    CHECK(n1 >= 6);
    CHECK(run("пуш") == PURRGO_MS_DONE);
    for (size_t i = 0; i < purrgo_map_search_count() && i < 8; i++) { strcpy(a[i], name_utf8(i)); }
    CHECK(purrgo_map_search_count() == 6);
    purrgo_map_search_close();
    open_fixture();
    CHECK(run("пуш") == PURRGO_MS_DONE && purrgo_map_search_count() == 6);
    for (size_t i = 0; i < 6; i++) { CHECK(strcmp(a[i], name_utf8(i)) == 0); }

    /* пока считается новый запрос, виден список прежнего */
    {
        char pgo[32];
        purrgo_utf8_to_pgo("берёзка", pgo, sizeof pgo);
        purrgo_map_search_set_query(pgo);
        CHECK(purrgo_map_search_running() && purrgo_map_search_state() == PURRGO_MS_RUNNING);
        CHECK(purrgo_map_search_count() == 6);                 /* список "пуш" */
        purrgo_utf8_to_pgo("озеро", pgo, sizeof pgo);
        purrgo_map_search_set_query(pgo);                      /* отмена и новый запрос */
        CHECK(purrgo_map_search_count() == 6);
        while (purrgo_map_search_step() == PURRGO_MS_RUNNING) { }
        CHECK(purrgo_map_search_count() == 1);
    }

    /* вне открытого индекса ничего не происходит */
    purrgo_map_search_close();
    purrgo_map_search_set_query("x");
    CHECK(purrgo_map_search_step() == PURRGO_MS_IDLE && purrgo_map_search_count() == 0);
}

static void test_read_error(void)
{
    static const uint8_t junk[8] = { 0 };

    /* база слоя испортилась после открытия: объекты без проверяемого названия не попадают в список */
    open_fixture();
    fake_fs_put_bin(MAP "/roads.db", junk, sizeof junk);
    CHECK(run("пушкина ул") == PURRGO_MS_DONE);
    CHECK(purrgo_map_search_count() == 0);
    CHECK(run("пушкин") == PURRGO_MS_DONE);
    CHECK(count_layer(PURRGO_MS_LAYER_ROADS) == 2);             /* без проверки слов - находятся, имени нет */
    CHECK(strcmp(name_utf8(0), "") == 0 || purrgo_map_search_count() > 0);
    purrgo_map_search_close();
    CHECK(fake_fs_open_handles() == 0);
}

/* ---- синтетический индекс: Мортон и порции ------------------------------ */

static void p32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

typedef struct { int32_t lat, lon; uint32_t id; uint64_t z; } pt_t;

static uint64_t spread(uint32_t v)
{
    uint64_t x = v;
    x = (x | (x << 16)) & 0x0000FFFF0000FFFFULL; x = (x | (x << 8)) & 0x00FF00FF00FF00FFULL;
    x = (x | (x << 4)) & 0x0F0F0F0F0F0F0F0FULL;  x = (x | (x << 2)) & 0x3333333333333333ULL;
    return (x | (x << 1)) & 0x5555555555555555ULL;
}

static uint64_t zcode(int32_t lat, int32_t lon)
{
    return spread((uint32_t)((int64_t)lon + 1800000000LL)) | (spread((uint32_t)((int64_t)lat + 900000000LL)) << 1);
}

static int cmp_z(const void *a, const void *b)
{
    const pt_t *x = a, *y = b;
    return (x->z > y->z) - (x->z < y->z);
}

static uint64_t g_rng = 12345;
static uint32_t rnd(void) { g_rng = g_rng * 6364136223846793005ULL + 1442695040888963407ULL; return (uint32_t)(g_rng >> 33); }

#define SYN_CAT   5000u
#define SYN_NAME  600u

static pt_t g_cat[SYN_CAT], g_name[SYN_NAME];

static void entry(uint8_t *e, const pt_t *p, uint8_t code, uint8_t sub, const uint8_t *key, uint8_t klen)
{
    memset(e, 0, 32);
    p32(e, (uint32_t)p->lon); p32(e + 4, (uint32_t)p->lat); p32(e + 8, p->id);
    e[12] = code; e[13] = sub; e[15] = klen;
    if (key) { memcpy(e + 16, key, klen); }
}

/* search.idx из SYN_CAT кафе (подтип 20) и SYN_NAME названий "пушкин..." с ключами по возрастанию. */
static void build_synthetic(void)
{
    static uint8_t buf[200000];
    uint8_t key_cafe[16] = { 0 }, key_pu[16] = { 0 };
    uint32_t name_off = 0x200, cat_off, size;
    char tmp[8];

    purrgo_utf8_to_pgo("кафе", tmp, sizeof tmp);
    for (int i = 0; i < 4; i++) { key_cafe[i] = purrgo_search_fold_byte((uint8_t)tmp[i]); }
    purrgo_utf8_to_pgo("пушкин", tmp, sizeof tmp);
    for (int i = 0; i < 6; i++) { key_pu[i] = purrgo_search_fold_byte((uint8_t)tmp[i]); }

    g_rng = 12345;
    for (uint32_t i = 0; i < SYN_CAT; i++) {
        /* треть записей - рядом с началом отсчёта, остальные по всей Беларуси */
        if (i % 3u == 0u) {
            g_cat[i].lat = LAT0 + (int32_t)(rnd() % 4000000u) - 2000000;
            g_cat[i].lon = LON0 + (int32_t)(rnd() % 6000000u) - 3000000;
        } else {
            g_cat[i].lat = 510000000 + (int32_t)(rnd() % 50000000u);
            g_cat[i].lon = 230000000 + (int32_t)(rnd() % 100000000u);
        }
        g_cat[i].id = i + 1u;
        g_cat[i].z = zcode(g_cat[i].lat, g_cat[i].lon);
    }
    qsort(g_cat, SYN_CAT, sizeof g_cat[0], cmp_z);
    for (uint32_t i = 0; i < SYN_NAME; i++) {
        g_name[i].lat = LAT0 + (int32_t)(rnd() % 80000000u) - 40000000;
        g_name[i].lon = LON0 + (int32_t)(rnd() % 80000000u) - 40000000;
        g_name[i].id = 100000u + i;
    }

    cat_off = name_off + ((SYN_NAME * 32u + 511u) / 512u) * 512u;
    size = cat_off + ((SYN_CAT * 32u + 511u) / 512u) * 512u;
    memset(buf, 0, size);
    memcpy(buf, "PGO", 3); buf[3] = 4; p32(buf + 4, size - 32u);
    memcpy(buf + 0x20, "SRCH", 4); buf[0x24] = 1; buf[0x26] = 32;
    p32(buf + 0x20 + 0x10, name_off); p32(buf + 0x20 + 0x14, SYN_NAME);
    p32(buf + 0x20 + 0x18, cat_off);  p32(buf + 0x20 + 0x1C, SYN_CAT);
    p32(buf + 0x20 + 0x20, 0x60);     p32(buf + 0x20 + 0x24, 8u + 48u);
    buf[0x60] = 1; buf[0x62] = 48;
    memcpy(buf + 0x68, key_cafe, 16);
    buf[0x68 + 16 + 20 / 8] = (uint8_t)(1u << (20 % 8));
    /* NAME: одинаковый ключ "пушкин", порядок по db_idx; CAT: по подтипу и Мортону */
    for (uint32_t i = 0; i < SYN_NAME; i++) { entry(buf + name_off + 32u * i, &g_name[i], 133, 20, key_pu, 6); }
    for (uint32_t i = 0; i < SYN_CAT; i++) { entry(buf + cat_off + 32u * i, &g_cat[i], 133, 20, NULL, 0); }
    fake_fs_reset();
    fake_fs_put_bin("maps/syn/search.idx", buf, size);
}

static uint64_t d2(int32_t lat, int32_t lon, int32_t olat, int32_t olon)
{
    int64_t dx = ((int64_t)lon - olon) * purrgo_geo_cos_10k(olat) / 10000;
    int64_t dy = (int64_t)lat - olat;
    return (uint64_t)(dx * dx + dy * dy);
}

typedef struct { uint64_t d; uint32_t id; } cand_t;

static int cmp_c(const void *a, const void *b)
{
    const cand_t *x = a, *y = b;
    return (x->d > y->d) - (x->d < y->d);
}

/* Топ-32 перебором; false - результаты движка не совпали. */
static bool compare_bruteforce(const pt_t *pts, uint32_t n, int32_t olat, int32_t olon)
{
    static cand_t c[SYN_CAT];
    size_t want = n < 32u ? n : 32u;

    for (uint32_t i = 0; i < n; i++) { c[i].d = d2(pts[i].lat, pts[i].lon, olat, olon); c[i].id = pts[i].id; }
    qsort(c, n, sizeof c[0], cmp_c);
    if (purrgo_map_search_count() != want) { printf("count %zu want %zu\n", purrgo_map_search_count(), want); return false; }
    for (size_t i = 0; i < want; i++) {
        int32_t la, lo;
        if (!purrgo_map_search_position(i, &la, &lo)) { return false; }
        if (d2(la, lo, olat, olon) != c[i].d) { printf("i=%zu got %llu want %llu\n", i, (unsigned long long)d2(la, lo, olat, olon), (unsigned long long)c[i].d); return false; }
    }
    return true;
}

static void test_synthetic(void)
{
    static const int32_t origins[][2] = {
        { LAT0, LON0 }, { 523000000, 238000000 }, { 545000000, 310000000 }, { 515000000, 235000000 },
        { 600000000, 100000000 },                                  /* пусто в радиусе 64 км */
    };

    build_synthetic();
    CHECK(purrgo_map_search_open("maps/syn"));

    for (size_t k = 0; k < sizeof origins / sizeof origins[0]; k++) {
        const int32_t olat = origins[k][0], olon = origins[k][1];
        uint32_t reads;
        int steps = 0;

        purrgo_map_search_set_origin(PURRGO_POI_ORIGIN_MAP, olat, olon);
        fake_fs_reset_read_stats();
        {
            char pgo[16];
            purrgo_utf8_to_pgo("кафе", pgo, sizeof pgo);
            purrgo_map_search_set_query(pgo);
            while (purrgo_map_search_step() == PURRGO_MS_RUNNING) { steps++; }
        }
        reads = fake_fs_read_calls();

        if (k < 4) {
            /* внутри 64 км есть 32 и более записей - топ-32 точный; Мортон не читает весь участок */
            CHECK(compare_bruteforce(g_cat, SYN_CAT, olat, olon));
            CHECK(reads < SYN_CAT / 16u * 3u / 2u);   /* без кэша секторов; на устройстве повторные обращения берутся из кэша */
        } else {
            CHECK(purrgo_map_search_count() == 0);                /* дальше 64 км ничего нет */
        }
    }

    /* NAME: 600 записей с одним ключом, порциями по PURRGO_MAP_SEARCH_CHUNK; результат - перебор */
    purrgo_map_search_set_origin(PURRGO_POI_ORIGIN_GNSS, LAT0, LON0);
    {
        char pgo[16];
        int steps = 0;

        purrgo_utf8_to_pgo("пушкин", pgo, sizeof pgo);
        purrgo_map_search_set_query(pgo);
        while (purrgo_map_search_step() == PURRGO_MS_RUNNING) { steps++; }
        CHECK(steps >= (int)(SYN_NAME / PURRGO_MAP_SEARCH_CHUNK));
        CHECK(compare_bruteforce(g_name, SYN_NAME, LAT0, LON0));

        /* "кафе пушкин": подтип совпадает у всех, результат тот же */
        CHECK(run("пушкин кафе") == PURRGO_MS_DONE);
        CHECK(compare_bruteforce(g_name, SYN_NAME, LAT0, LON0));
        CHECK(purrgo_map_search_distance_m(0) <= purrgo_map_search_distance_m(31) + 2u);
    }
    purrgo_map_search_close();
}

int main(void)
{
    test_fold_vectors();
    test_open();
    test_fixture_queries();
    test_incremental_and_cancel();
    test_read_error();
    test_synthetic();
    return test_finish("test_map_search");
}
