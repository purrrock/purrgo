/*
 * Host-тесты простого поиска объектов (src/core/map/poi_search.c, docs/design/poi-search.md):
 *   - таблица категорий и маски кодов Type;
 *   - топ-32 по возрастанию расстояния совпадает с перебором, расширение радиуса 1 -> 64 км;
 *   - порционность: один purrgo_poi_search_step() - один проход, до конца поиск RUNNING;
 *   - названия из pois.db читаются лениво, расстояние и азимут;
 *   - антимеридиан, пустой результат, нет pois.idx, отмена;
 *   - поиск не занимает лишних слотов файловой системы (файлы принадлежат карте);
 *   - purrgo_pgo_to_utf8() - обратное преобразование к purrgo_utf8_to_pgo().
 * Построитель синтетического .idx V4 повторяет test_map.c.
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/map.h"
#include "purrgo/fs_hal.h"
#include "purrgo/pgo_text.h"
#include "purrgo/poi_search.h"
#include "map/map_internal.h"
#include "fake_fs.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

#define MAXF 32768
typedef struct { uint8_t d[MAXF]; uint32_t n; } blob_t;

#define BLK 512u

/* Заголовок PGO; l[4] - смещения LOD (для .idx), l == NULL - нули. */
static void pgo_header(uint8_t *h, uint8_t type, uint32_t payload, const uint32_t *l)
{
    memset(h, 0, 32);
    h[0] = 'P'; h[1] = 'G'; h[2] = 'O'; h[3] = type;
    put_u32(h + 4, payload);
    for (int i = 0; l && i < 4; i++) { put_u32(h + 8 + 4 * i, l[i]); }
}

/* Узел данных (25 байт): bbox, тип объекта, v1 (смещение геометрии), v2 (индекс в .db). */
static void data_node(uint8_t *p, int32_t xmin, int32_t ymin, int32_t xmax, int32_t ymax,
                      uint8_t type, uint32_t v1, uint32_t v2)
{
    put_u32(p, (uint32_t)xmin); put_u32(p + 4, (uint32_t)ymin);
    put_u32(p + 8, (uint32_t)xmax); put_u32(p + 12, (uint32_t)ymax);
    p[16] = type; put_u32(p + 17, v1); put_u32(p + 21, v2);
}

/* Навигационный узел (28 байт): child_block, bbox, уровень (v1), число детей (v2). */
static void nav_node(uint8_t *p, uint32_t child_block, const int32_t *bb, uint32_t level, uint32_t count)
{
    put_u32(p, child_block);
    for (int i = 0; i < 4; i++) { put_u32(p + 4 + 4 * i, (uint32_t)bb[i]); }
    put_u32(p + 20, level); put_u32(p + 24, count);
}

/* Построитель .idx: секции LOD добавляются подряд, ib_end пишет заголовок PGO. */
typedef struct {
    blob_t *o;
    uint32_t pos;          /* конец записанного */
    uint32_t lod[4];
    uint32_t lods;
} ib_t;

static void ib_begin(ib_t *b, blob_t *o)
{
    memset(o, 0, sizeof *o);
    b->o = o; b->pos = 0x200u; b->lods = 0;
}

/* Заголовок секции SQT (раздел 5.1) и начало секции; blocks - число блоков после заголовка. */
static uint32_t ib_section(ib_t *b, uint32_t depth, uint32_t roots, uint32_t blocks)
{
    uint32_t s = b->pos;
    uint8_t *h = b->o->d + s;
    b->lod[b->lods++] = s;
    h[0] = 'S'; h[1] = 'Q'; h[2] = 'T'; h[3] = 2;
    put_u32(h + 4, 2); put_u32(h + 8, depth); put_u32(h + 12, roots);
    put_u32(h + 16, BLK); put_u32(h + 20, BLK * (1u + blocks));
    h[24] = 18; h[26] = 20;
    b->pos = s + BLK * (1u + blocks);
    return s;
}

static void ib_empty(ib_t *b) { (void)ib_section(b, 0, 0, 0); }

static void ib_end(ib_t *b)
{
    pgo_header(b->o->d, 1, b->pos - 32u, b->lod);
    b->o->n = b->pos;
}

static void bb_union(int32_t *u, const int32_t *x, int first)
{
    if (first || x[0] < u[0]) { u[0] = x[0]; }
    if (first || x[1] < u[1]) { u[1] = x[1]; }
    if (first || x[2] > u[2]) { u[2] = x[2]; }
    if (first || x[3] > u[3]) { u[3] = x[3]; }
}

#define TN_MAX 512
typedef struct { int32_t bb[4]; uint32_t first, count, child; } tn_t;

/*
 * Секция LOD из n узлов данных (по 25 байт) в заданном порядке: листовые блоки по leaf_fan узлов,
 * затем навигационные уровни по nav_fan узлов, пока в корне не станет не больше nav_fan.
 * Блоки расположены по порядку раздела 7.3 (обход в ширину), как делает компилятор.
 */
static void ib_tree(ib_t *b, const uint8_t *leaves, uint32_t n, uint32_t leaf_fan, uint32_t nav_fan)
{
    static tn_t layer[8][TN_MAX];
    uint32_t cnt[8];
    uint32_t k = 0;

    if (n == 0) { ib_empty(b); return; }

    cnt[0] = (n + leaf_fan - 1u) / leaf_fan;
    for (uint32_t i = 0; i < cnt[0]; i++) {
        tn_t *t = &layer[0][i];
        t->first = i * leaf_fan;
        t->count = (n - t->first < leaf_fan) ? n - t->first : leaf_fan;
        for (uint32_t j = 0; j < t->count; j++) {
            const uint8_t *nd = leaves + (t->first + j) * 25u;
            int32_t bb[4];
            for (int q = 0; q < 4; q++) { bb[q] = (int32_t)((uint32_t)nd[4*q] | (uint32_t)nd[4*q+1] << 8 | (uint32_t)nd[4*q+2] << 16 | (uint32_t)nd[4*q+3] << 24); }
            bb_union(t->bb, bb, j == 0);
        }
    }
    while (cnt[k] > nav_fan) {
        uint32_t m = (cnt[k] + nav_fan - 1u) / nav_fan;
        for (uint32_t i = 0; i < m; i++) {
            tn_t *t = &layer[k + 1][i];
            t->first = i * nav_fan;
            t->count = (cnt[k] - t->first < nav_fan) ? cnt[k] - t->first : nav_fan;
            for (uint32_t j = 0; j < t->count; j++) { bb_union(t->bb, layer[k][t->first + j].bb, j == 0); }
        }
        cnt[++k] = m;
    }

    /* Очередь блоков в порядке 7.3: {слой (k + 1 - лист), первая запись, число записей}. */
    struct { int layer; uint32_t first, count; } q[TN_MAX + 8];
    uint32_t qn = 0;
    q[qn].layer = (int)k; q[qn].first = 0; q[qn].count = cnt[k]; qn++;
    /* Сначала считаем число блоков и назначаем child_block. */
    for (uint32_t i = 0; i < qn; i++) {
        if (q[i].layer < 0) { continue; }
        for (uint32_t j = 0; j < q[i].count; j++) {
            tn_t *t = &layer[q[i].layer][q[i].first + j];
            q[qn].layer = q[i].layer - 1; q[qn].first = t->first; q[qn].count = t->count;
            t->child = b->pos + BLK * (1u + qn);
            qn++;
        }
    }
    uint32_t s = ib_section(b, k + 1u, cnt[k], qn);
    for (uint32_t i = 0; i < qn; i++) {
        uint8_t *blk = b->o->d + s + BLK * (1u + i);
        if (q[i].layer < 0) {
            memcpy(blk, leaves + q[i].first * 25u, q[i].count * 25u);
        } else {
            for (uint32_t j = 0; j < q[i].count; j++) {
                const tn_t *t = &layer[q[i].layer][q[i].first + j];
                nav_node(blk + 28u * j, t->child, t->bb, (uint32_t)q[i].layer, t->count);
            }
        }
    }
}

/* Цепочка из depth навигационных узлов по одному на блок; в конце один узел данных. */
static void ib_chain(ib_t *b, uint32_t depth, const uint8_t *leaf)
{
    uint32_t s = ib_section(b, depth, 1, depth + 1u);   /* depth навигационных блоков и лист */
    static const int32_t bb[4] = { 0, 0, 1000, 1000 };
    for (uint32_t k = 0; k < depth; k++) {
        nav_node(b->o->d + s + BLK * (1u + k), s + BLK * (2u + k), bb, depth - 1u - k, 1);
    }
    memcpy(b->o->d + s + BLK * (1u + depth), leaf, 25);
}

/* ----------------------------------------------------------- карта --- */

#define MAP "maps/m"
#define LAT0 539000000     /* 53.9 N */
#define LON0 275000000     /* 27.5 E */
#define T_FOOD 133
#define T_FUEL 145
#define T_WATER 143
#define T_PASS 155

typedef struct { int32_t lat, lon; uint8_t type; } poi_t;

static poi_t g_pois[256];
static int g_n;
static blob_t g_idx, g_db;

/* Записи в .db нумеруются с 1 в порядке, который выбрал вызывающий: v2 = индекс + 1. */
static void build_map(const poi_t *src, int n)
{
    static uint8_t leaves[256 * 25];

    g_n = n;
    memcpy(g_pois, src, (size_t)n * sizeof(poi_t));
    memset(&g_db, 0, sizeof g_db);
    pgo_header(g_db.d, 3, 129u + 117u * (uint32_t)n, NULL);
    for (int i = 0; i < n; i++) {
        char nm[24];
        (void)snprintf(nm, sizeof nm, "P%d_x", i);
        memcpy(g_db.d + 32u + 129u + 117u * (uint32_t)i + 17u, nm, strlen(nm) + 1u);
        data_node(leaves + 25 * i, g_pois[i].lon, g_pois[i].lat, g_pois[i].lon, g_pois[i].lat,
                  g_pois[i].type, 0, (uint32_t)i + 1u);
    }
    g_db.n = 32u + 129u + 117u * (uint32_t)n;

    /* В дереве порядок пространственный (по широте); v2 при этом остаётся у каждого узла. */
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (unpack_i32_le(leaves + 25 * j + 4) < unpack_i32_le(leaves + 25 * i + 4)) {
                uint8_t t[25];
                memcpy(t, leaves + 25 * i, 25); memcpy(leaves + 25 * i, leaves + 25 * j, 25);
                memcpy(leaves + 25 * j, t, 25);
            }
        }
    }

    ib_t b;
    ib_begin(&b, &g_idx);
    for (int i = 0; i < 4; i++) { ib_tree(&b, leaves, (uint32_t)n, 20, 18); }
    ib_end(&b);
    fake_fs_put_bin(MAP "/pois.idx", g_idx.d, g_idx.n);
    fake_fs_put_bin(MAP "/pois.db", g_db.d, g_db.n);
}

static void run_to_end(void)
{
    int guard = 0;
    while (purrgo_poi_search_step() == PURRGO_POI_SEARCH_RUNNING && ++guard < 20) { }
}

/* Точка со смещением в метрах от начала отсчёта (север, восток). */
static poi_t at(int dn_m, int de_m, uint8_t type)
{
    poi_t p;
    p.lat = LAT0 + dn_m * 90;
    p.lon = LON0 + (int32_t)((int64_t)de_m * 90 * 10000 / purrgo_geo_cos_10k(LAT0));
    p.type = type;
    return p;
}

/* ----------------------------------------------------------- тесты --- */

static void test_categories(void)
{
    CHECK(purrgo_poi_category_count() == 13u);
    CHECK(strstr(purrgo_poi_category_title(0), "Lodging") != NULL);
    CHECK(purrgo_poi_category_mask(6) == (1ULL << (145 - 128)));
    CHECK((purrgo_poi_category_mask(0) & (1ULL << (167 - 128))) != 0u);
    CHECK(purrgo_poi_category_mask(99) == 0u && purrgo_poi_category_title(99)[0] == '\0');
    for (size_t c = 0; c < purrgo_poi_category_count(); c++) {
        CHECK(purrgo_poi_category_mask(c) != 0u);
        CHECK(strlen(purrgo_poi_category_title(c)) <= 27u * 2u);   /* UTF-8: кириллица по 2 байта */
    }
}

static void test_top32_matches_bruteforce(void)
{
    fake_fs_reset();
    poi_t p[200];
    int n = 0;

    /* 60 АЗС на расстояниях 200 м ... 12 км, плюс столовые и вода, которые в выдачу не попадают. */
    for (int i = 0; i < 60; i++) {
        p[n++] = at(((i * 7919) % 11) * 1000 - 5000 + i * 37, ((i * 104729) % 9) * 1100 - 4000 + i * 53, T_FUEL);
    }
    for (int i = 0; i < 40; i++) { p[n++] = at(i * 100, -i * 90, T_FOOD); }
    for (int i = 0; i < 20; i++) { p[n++] = at(-i * 150, i * 80, T_WATER); }
    build_map(p, n);

    CHECK(purrgo_poi_search_start(MAP, 6, PURRGO_POI_ORIGIN_GNSS, LAT0, LON0));
    CHECK(purrgo_poi_search_state() == PURRGO_POI_SEARCH_RUNNING);
    CHECK(purrgo_poi_search_count() == 0u);   /* до конца поиска списка нет */

    int passes = 0;
    while (purrgo_poi_search_step() == PURRGO_POI_SEARCH_RUNNING) { passes++; CHECK(passes < 10); }
    CHECK(purrgo_poi_search_state() == PURRGO_POI_SEARCH_DONE);
    CHECK(passes >= 1);                       /* один проход - не весь поиск */
    CHECK(purrgo_poi_search_count() == 32u);

    /* Эталон: 32 ближайших АЗС перебором. */
    uint64_t best[60];
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (g_pois[i].type != T_FUEL) { continue; }
        int64_t dy = g_pois[i].lat - LAT0;
        int64_t dx = (int64_t)(g_pois[i].lon - LON0) * purrgo_geo_cos_10k(LAT0) / 10000;
        best[m++] = (uint64_t)(dx * dx + dy * dy);
    }
    for (int i = 0; i < m; i++) for (int j = i + 1; j < m; j++) if (best[j] < best[i]) { uint64_t t = best[i]; best[i] = best[j]; best[j] = t; }

    uint32_t prev = 0;
    for (size_t i = 0; i < 32u; i++) {
        int32_t lat, lon;
        CHECK(purrgo_poi_search_position(i, &lat, &lon));
        int64_t dy = lat - LAT0;
        int64_t dx = (int64_t)(lon - LON0) * purrgo_geo_cos_10k(LAT0) / 10000;
        CHECK((uint64_t)(dx * dx + dy * dy) == best[i]);
        CHECK(purrgo_poi_search_type(i) == T_FUEL);
        uint32_t d = purrgo_poi_search_distance_m(i);
        CHECK(d >= prev);
        prev = d;
    }
    CHECK(purrgo_poi_search_radius_km() >= 2u && purrgo_poi_search_radius_km() <= 64u);
    CHECK(purrgo_poi_search_origin() == PURRGO_POI_ORIGIN_GNSS);

    /* Названия: v2 - номер записи; имя читается лениво, "_" -> пробел. */
    const char *nm = purrgo_poi_search_name(0);
    CHECK(nm[0] == 'P' && strstr(nm, " x") != NULL);
    CHECK(purrgo_poi_search_name(0) == nm);
    CHECK(purrgo_poi_search_name(99)[0] == '\0');

    purrgo_poi_search_cancel();
    CHECK(purrgo_poi_search_state() == PURRGO_POI_SEARCH_IDLE && purrgo_poi_search_count() == 0u);
    purrgo_map_shutdown();
    CHECK(fake_fs_open_handles() == 0);
}

static void test_radius_expansion_and_direction(void)
{
    fake_fs_reset();
    poi_t p[4];

    p[0] = at(0, 20000, T_PASS);       /* 20 км на восток */
    p[1] = at(40000, 0, T_PASS);       /* 40 км на север */
    p[2] = at(-3000, 0, T_FOOD);
    p[3] = at(0, -70000, T_PASS);      /* 70 км: за пределом 64 */
    build_map(p, 4);

    CHECK(purrgo_poi_search_start(MAP, 11, PURRGO_POI_ORIGIN_MAP, LAT0, LON0));
    purrgo_poi_search_state_t st = purrgo_poi_search_step();
    CHECK(st == PURRGO_POI_SEARCH_RUNNING && purrgo_poi_search_radius_km() == 1u);
    run_to_end();
    CHECK(purrgo_poi_search_state() == PURRGO_POI_SEARCH_DONE);
    CHECK(purrgo_poi_search_radius_km() == 64u);
    CHECK(purrgo_poi_search_count() == 2u);
    uint32_t d0 = purrgo_poi_search_distance_m(0), d1 = purrgo_poi_search_distance_m(1);
    CHECK(d0 > 19800u && d0 < 20200u);
    CHECK(d1 > 39800u && d1 < 40200u);
    uint16_t a0 = purrgo_poi_search_azimuth_deg(0), a1 = purrgo_poi_search_azimuth_deg(1);
    CHECK(a0 > 88u && a0 < 92u);
    CHECK(a1 <= 1u || a1 >= 359u);
    purrgo_poi_search_cancel();
    purrgo_map_shutdown();
}

static void test_empty_and_errors(void)
{
    fake_fs_reset();
    poi_t p[1] = { at(500, 0, T_FOOD) };
    build_map(p, 1);

    CHECK(purrgo_poi_search_start(MAP, 6, PURRGO_POI_ORIGIN_LAST, LAT0, LON0));
    run_to_end();
    CHECK(purrgo_poi_search_state() == PURRGO_POI_SEARCH_DONE && purrgo_poi_search_count() == 0u);
    CHECK(purrgo_poi_search_radius_km() == 64u);
    CHECK(purrgo_poi_search_step() == PURRGO_POI_SEARCH_DONE);   /* вне RUNNING - ничего не делает */

    CHECK(!purrgo_poi_search_start(MAP, 99, PURRGO_POI_ORIGIN_MAP, LAT0, LON0));
    CHECK(purrgo_poi_search_state() == PURRGO_POI_SEARCH_ERROR);
    CHECK(!purrgo_poi_search_start(NULL, 0, PURRGO_POI_ORIGIN_MAP, LAT0, LON0));

    /* Нет pois.idx в каталоге. */
    CHECK(purrgo_poi_search_start("maps/none", 0, PURRGO_POI_ORIGIN_MAP, LAT0, LON0));
    CHECK(purrgo_poi_search_step() == PURRGO_POI_SEARCH_ERROR);
    CHECK(purrgo_poi_search_count() == 0u);

    /* Мусор вместо индекса. */
    uint8_t junk[64] = { 1, 2, 3 };
    fake_fs_put_bin("maps/bad/pois.idx", junk, sizeof junk);
    CHECK(purrgo_poi_search_start("maps/bad", 0, PURRGO_POI_ORIGIN_MAP, LAT0, LON0));
    CHECK(purrgo_poi_search_step() == PURRGO_POI_SEARCH_ERROR);
    purrgo_poi_search_cancel();
    purrgo_map_shutdown();
    CHECK(fake_fs_open_handles() == 0);
}

static void test_missing_db_still_searches(void)
{
    fake_fs_reset();
    poi_t p[2] = { at(100, 0, T_FUEL), at(300, 0, T_FUEL) };
    build_map(p, 2);
    fake_fs_put_bin("maps/m2/pois.idx", g_idx.d, g_idx.n);   /* pois.db нет */

    CHECK(purrgo_poi_search_start("maps/m2", 6, PURRGO_POI_ORIGIN_GNSS, LAT0, LON0));
    run_to_end();
    CHECK(purrgo_poi_search_count() == 2u);
    CHECK(purrgo_poi_search_name(0)[0] == '\0');
    purrgo_poi_search_cancel();
    purrgo_map_shutdown();
}

static void test_antimeridian(void)
{
    fake_fs_reset();
    poi_t p[2];
    const int32_t lon0 = 1799990000;                  /* 179.999 E */

    p[0].lat = LAT0; p[0].lon = -1799990000; p[0].type = T_FUEL;   /* 179.999 W: ~1.3 км */
    p[1].lat = LAT0; p[1].lon = 1799000000; p[1].type = T_FUEL;    /* 179.9 E: ~6.5 км */
    build_map(p, 2);

    CHECK(purrgo_poi_search_start(MAP, 6, PURRGO_POI_ORIGIN_MAP, LAT0, lon0));
    run_to_end();
    CHECK(purrgo_poi_search_count() == 2u);
    int32_t lat, lon;
    CHECK(purrgo_poi_search_position(0, &lat, &lon) && lon == -1799990000);
    CHECK(purrgo_poi_search_distance_m(0) < 2000u);
    purrgo_poi_search_cancel();
    purrgo_map_shutdown();
}

static void test_shares_map_files(void)
{
    fake_fs_reset();
    poi_t p[1] = { at(100, 0, T_FUEL) };
    build_map(p, 1);

    purrgo_file_t *idx = NULL, *db = NULL;
    CHECK(purrgo_map_poi_files(MAP, &idx, &db) && idx != NULL && db != NULL);
    CHECK(fake_fs_open_handles() == 2);

    CHECK(purrgo_poi_search_start(MAP, 6, PURRGO_POI_ORIGIN_GNSS, LAT0, LON0));
    run_to_end();
    (void)purrgo_poi_search_name(0);
    CHECK(fake_fs_open_handles() == 2);   /* поиск не открывает свои файлы */

    purrgo_file_t *idx2 = NULL, *db2 = NULL;
    CHECK(purrgo_map_poi_files(MAP, &idx2, &db2) && idx2 == idx && db2 == db);
    purrgo_poi_search_cancel();
    purrgo_map_shutdown();
    CHECK(fake_fs_open_handles() == 0);
}

static void test_pgo_to_utf8(void)
{
    char pgo[32], back[64], u[64];

    (void)snprintf(u, sizeof u, "Mir %s", "Минск Ёж ёлка");
    (void)snprintf(pgo, sizeof pgo, "%s", u);
    purrgo_utf8_to_pgo_inplace(pgo);
    CHECK(purrgo_pgo_to_utf8(pgo, back, sizeof back) == strlen(u));
    CHECK(strcmp(back, u) == 0);

    CHECK(purrgo_pgo_to_utf8("abc", back, sizeof back) == 3u && strcmp(back, "abc") == 0);
    CHECK(purrgo_pgo_to_utf8("\xC5", back, sizeof back) == 1u && back[0] == '?');   /* без обратного соответствия */
    /* Символ из двух байт целиком не помещается - отбрасывается, строка завершена. */
    pgo[0] = (char)0x80; pgo[1] = '\0';
    CHECK(purrgo_pgo_to_utf8(pgo, back, 2) == 0u && back[0] == '\0');
    CHECK(purrgo_pgo_to_utf8(pgo, back, 3) == 2u);
    CHECK(purrgo_pgo_to_utf8(NULL, back, sizeof back) == 0u && back[0] == '\0');
}

int main(void)
{
    test_categories();
    test_top32_matches_bruteforce();
    test_radius_expansion_and_direction();
    test_empty_and_errors();
    test_missing_db_still_searches();
    test_antimeridian();
    test_shares_map_files();
    test_pgo_to_utf8();
    return test_finish("test_poi_search");
}
