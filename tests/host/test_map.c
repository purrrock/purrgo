/*
 * Host-тесты парсера и рендерера карты (формат PurrGO V4, docs/map-format-v4.md):
 *   - корректная карта V4 в памяти рисуется; выбор LOD по масштабу и детализации;
 *   - блочное дерево: многоуровневые деревья, пропуск поддеревьев без чтения их блоков;
 *   - повреждённые заголовки PGO/SQT, узлы навигации (раздел 7.1), геометрия MLP
 *     отвергаются без падений и без выхода за границы; карта V3 отвергается;
 *   - подписи: сокращение до 22 символов, "_" -> пробел (раздел 10.6.1);
 *   - «фаззинг»: усечение файла в каждой точке и подмена каждого байта .idx/.mlp/.db
 *     (проверяется ASan/UBSan: любое чтение за границей - ошибка теста).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "purrgo/map.h"
#include "purrgo/fs_hal.h"
#include "purrgo/map_style.h"
#include "purrgo/purrgo_poi_icons.h"
#include "map/map_internal.h"
#include "map/map_culling.h"
#include "map/map_idx.h"
#include "fake_fs.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

/* ----------------------------------------------------------- графика --- */

#define SCR_W 64
#define SCR_H 64
static uint8_t g_fb[SCR_W * SCR_H];
static long g_pixels;
static bool g_ok;      /* результат последнего purrgo_map_render_layer() */

static void cb_draw(void *fb, int16_t x, int16_t y, gfx_color_t c)
{
    (void)fb;
    if (x >= 0 && x < SCR_W && y >= 0 && y < SCR_H) { g_fb[y * SCR_W + x] = (uint8_t)(c + 1); g_pixels++; }
}
static gfx_color_t cb_read(void *fb, int16_t x, int16_t y)
{
    (void)fb;
    if (x >= 0 && x < SCR_W && y >= 0 && y < SCR_H) { return (gfx_color_t)(g_fb[y * SCR_W + x]); }
    return 0;
}

static gfx_context_t g_gfx;
static purrgo_map_render_params_t g_params;
static const purrgo_bbox_t CAM = { 0, 0, 1000, 1000 };
static const purrgo_viewport_t VP = { SCR_W, SCR_H, 0, 0 };

static void gfx_reset(void)
{
    memset(g_fb, 0, sizeof g_fb);
    g_pixels = 0;
    (void)gfx_init(&g_gfx, SCR_W, SCR_H, g_fb, cb_draw, cb_read);
    gfx_set_color(&g_gfx, 1, 3);
}

/* ------------------------------------------- построитель карты V4 в RAM --- */

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

/* IDX с одним набором узлов данных (не больше 20) во всех четырёх LOD. */
static void build_idx(blob_t *o, const uint8_t *nodes, uint32_t count)
{
    ib_t b;
    ib_begin(&b, o);
    for (int i = 0; i < 4; i++) { ib_tree(&b, nodes, count, 20, 18); }
    ib_end(&b);
}

/* Смещение секции LOD lod в собранном .idx. */
static uint32_t lod_at(const blob_t *o, int lod)
{
    const uint8_t *h = o->d + 8 + 4 * lod;
    return (uint32_t)h[0] | (uint32_t)h[1] << 8 | (uint32_t)h[2] << 16 | (uint32_t)h[3] << 24;
}

#define GEOM_V1 8u   /* смещение тела геометрии относительно 32 */

/* MLP с одной геометрией. parts == NULL -> num_parts = 0. */
static void build_mlp_ex(blob_t *o, int32_t num_parts, const uint32_t *parts, int32_t parts_avail,
                         int32_t num_points, const int32_t *xy, int32_t xy_avail)
{
    memset(o, 0, sizeof *o);
    uint32_t body = 32u + GEOM_V1;
    uint32_t pos = body + 24u;
    put_u32(o->d + body + 16, (uint32_t)num_parts);
    put_u32(o->d + body + 20, (uint32_t)num_points);
    for (int32_t i = 0; i < parts_avail && parts; i++) { put_u32(o->d + pos, parts[i]); pos += 4u; }
    for (int32_t i = 0; i < xy_avail && xy; i++) {
        put_u32(o->d + pos, (uint32_t)xy[2 * i]); put_u32(o->d + pos + 4, (uint32_t)xy[2 * i + 1]); pos += 8u;
    }
    pgo_header(o->d, 2, pos - 32u, NULL);
    o->n = pos;
}

/* Заголовок и данные согласованы: num_parts/num_points точек реально лежат в файле. */
static void build_mlp(blob_t *o, int32_t num_parts, const uint32_t *parts,
                      int32_t num_points, const int32_t *xy)
{
    build_mlp_ex(o, num_parts, parts, num_parts, num_points, xy, num_points);
}

/* Один отрезок (100,100)-(900,900) в одной части. */
static void simple_map(blob_t *idx, blob_t *mlp, uint8_t type)
{
    uint8_t node[25];
    data_node(node, 100, 100, 900, 900, type, GEOM_V1, 0);
    build_idx(idx, node, 1);
    uint32_t parts[1] = { 0 };
    int32_t xy[4] = { 100, 100, 900, 900 };
    build_mlp(mlp, 1, parts, 2, xy);
}

static long render(const blob_t *idx, const blob_t *mlp, const blob_t *db, purrgo_map_layer_t layer)
{
    /* Слой кладётся в фейковую ФС и открывается как обычный файл карты. */
    fake_fs_reset();
    fake_fs_put_bin("l.idx", idx->d, idx->n);
    if (mlp) { fake_fs_put_bin("l.mlp", mlp->d, mlp->n); }
    if (db)  { fake_fs_put_bin("l.db", db->d, db->n); }
    purrgo_file_t *ifs = purrgo_fs_open("l.idx", FS_READ);
    purrgo_file_t *mfs = mlp ? purrgo_fs_open("l.mlp", FS_READ) : NULL;
    purrgo_file_t *dfs = db ? purrgo_fs_open("l.db", FS_READ) : NULL;
    gfx_reset();
    g_ok = purrgo_map_render_layer(ifs, mfs, dfs, &g_gfx, &CAM, &VP, &g_params, layer);
    if (ifs) { (void)purrgo_fs_close(ifs); }
    if (mfs) { (void)purrgo_fs_close(mfs); }
    if (dfs) { (void)purrgo_fs_close(dfs); }
    return g_pixels;
}

static void defaults(void)
{
    memset(&g_params, 0, sizeof g_params);
    g_params.details = PURRGO_MAP_DETAILS_LOW;
    g_params.poi_mode = PURRGO_POI_MODE_CIRCLES;
}

/* Рендер каталога карты с текущими g_params. */
static bool render_dir(const char *dir)
{
    g_params.map_dir = dir;
    return purrgo_map_render_viewport(&g_gfx, &VP, &CAM, &g_params);
}

/* ------------------------------------------------------------- тесты --- */

static void test_valid_line(void)
{
    static blob_t idx, mlp;
    simple_map(&idx, &mlp, 1);                       /* ROAD_MAJOR */
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) > 20);
}

static void test_valid_polygon(void)
{
    static blob_t idx, mlp;
    uint8_t node[25];
    data_node(node, 100, 100, 900, 900, PURRGO_FEATURE_WATER, GEOM_V1, 0);
    build_idx(&idx, node, 1);
    uint32_t parts[1] = { 0 };
    int32_t xy[8] = { 100, 100, 900, 100, 900, 900, 100, 900 };
    build_mlp(&mlp, 1, parts, 4, xy);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_POLYGONS) > 100);
}

static void test_outside_camera_not_drawn(void)
{
    static blob_t idx, mlp;
    uint8_t node[25];
    data_node(node, 5000, 5000, 6000, 6000, 1, GEOM_V1, 0);
    build_idx(&idx, node, 1);
    uint32_t parts[1] = { 0 };
    int32_t xy[4] = { 5000, 5000, 6000, 6000 };
    build_mlp(&mlp, 1, parts, 2, xy);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) == 0);
}

static void test_bad_pgo_header(void)
{
    static blob_t idx, mlp, bad;
    simple_map(&idx, &mlp, 1);
    long ok = render(&idx, &mlp, NULL, MAP_LAYER_LINES);
    CHECK(ok > 0 && g_ok);

    bad = idx; bad.d[0] = 'X';                       /* магия */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; bad.d[3] = 2;                         /* тип файла: ожидается IDX */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; bad.d[3] = 0;
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; put_u32(bad.d + 4, 0xFFFFFFFFu);      /* размер payload: переполнение */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; put_u32(bad.d + 4, idx.n);            /* payload больше файла: размер не кратен 512 */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; put_u32(bad.d + 4, idx.n - 32u + BLK);/* payload больше файла, размер кратен 512 */
    (void)render(&bad, &mlp, NULL, MAP_LAYER_LINES);

    /* Смещения LOD (раздел 1.5). */
    for (int lod = 0; lod < 4; lod++) {
        bad = idx; put_u32(bad.d + 8 + 4 * lod, lod_at(&idx, lod) + 4u);     /* не кратно 512 */
        CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
        bad = idx; put_u32(bad.d + 8 + 4 * lod, 0xFFFFFE00u);                /* за пределами файла */
        CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
        bad = idx; put_u32(bad.d + 8 + 4 * lod, 0);
        CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    }
    bad = idx; put_u32(bad.d + 8, 0x400u);           /* LOD 0 не 0x200 */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; put_u32(bad.d + 12, lod_at(&idx, 0)); /* LOD1 == LOD0 */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; put_u32(bad.d + 20, lod_at(&idx, 2)); /* LOD3 == LOD2 */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; put_u32(bad.d + 20, idx.n);           /* LOD3 == конец файла: секции нет */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);

    /* Зарезервированные поля 0x18..0x1F прошивка не интерпретирует. */
    bad = idx; put_u32(bad.d + 0x18, 0xDEADBEEFu); put_u32(bad.d + 0x1C, 0x12345678u);
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) > 0 && g_ok);

    bad = mlp; bad.d[1] = 'Z';                       /* MLP: плохая магия */
    CHECK(render(&idx, &bad, NULL, MAP_LAYER_LINES) == 0);
    bad = mlp; bad.d[3] = 1;                         /* MLP: тип IDX вместо MLP */
    CHECK(render(&idx, &bad, NULL, MAP_LAYER_LINES) == 0);
}

/* Заголовок секции SQT (раздел 5.1) и пустые LOD. */
static void test_bad_sqt_header(void)
{
    static blob_t idx, mlp, bad;
    simple_map(&idx, &mlp, 1);
    const uint32_t s0 = lod_at(&idx, 0);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) > 0);

    bad = idx; bad.d[s0] = 'Q';                      /* плохая магия */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; bad.d[s0 + 3] = 1;                    /* SQT\x01: карта формата V3 не поддерживается */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; bad.d[s0 + 3] = 3;
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; put_u32(bad.d + s0 + 4, 1);           /* топология */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; put_u32(bad.d + s0 + 16, 256);        /* размер блока */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; put_u32(bad.d + s0 + 20, 2 * BLK);    /* размер секции не равен расстоянию до LOD 1 */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; bad.d[s0 + 24] = 19;                  /* nav_fanout > 18 */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; bad.d[s0 + 24] = 0;
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; bad.d[s0 + 26] = 21;                  /* leaf_fanout > 20 */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; put_u32(bad.d + s0 + 12, 0);          /* корневых узлов нет при depth 1 */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; put_u32(bad.d + s0 + 12, 19);         /* root_count > nav_fanout */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; put_u32(bad.d + s0 + 8, 2);           /* depth не соответствует размеру секции */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    bad = idx; put_u32(bad.d + s0 + 8, 0);           /* depth 0, но секция не пуста */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);

    /* Пустой LOD (depth 0, root_count 0, секция 512 Б) корректен: рисовать нечего, но кадр не ошибка. */
    {
        static blob_t e;
        ib_t b;
        ib_begin(&b, &e);
        for (int i = 0; i < 4; i++) { ib_empty(&b); }
        ib_end(&b);
        CHECK(e.n == 0x200u + 4u * BLK);
        CHECK(render(&e, &mlp, NULL, MAP_LAYER_LINES) == 0 && g_ok);
        bad = e; put_u32(bad.d + lod_at(&e, 0) + 12, 1);   /* root_count у пустого LOD */
        CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    }
}

static void test_bad_mlp_geometry(void)
{
    static blob_t idx, mlp, tmp;
    uint8_t node[25];
    data_node(node, 100, 100, 900, 900, 1, GEOM_V1, 0);
    build_idx(&idx, node, 1);

    uint32_t p1[1] = { 0 };
    int32_t xy[8] = { 100, 100, 900, 900, 100, 900, 900, 100 };

    build_mlp(&mlp, 1, p1, 4, xy);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) > 0);                 /* эталон */

    build_mlp_ex(&tmp, 1, p1, 1, 0, xy, 4);                               /* 0 точек */
    CHECK(render(&idx, &tmp, NULL, MAP_LAYER_LINES) == 0);
    build_mlp_ex(&tmp, 1, p1, 1, -5, xy, 4);                              /* отрицательное число точек */
    CHECK(render(&idx, &tmp, NULL, MAP_LAYER_LINES) == 0);
    build_mlp_ex(&tmp, 1, p1, 1, 2001, xy, 4);                            /* больше PURRGO_MAP_MAX_POINTS */
    CHECK(render(&idx, &tmp, NULL, MAP_LAYER_LINES) == 0);
    build_mlp_ex(&tmp, 1, p1, 1, 0x7FFFFFFF, xy, 4);
    CHECK(render(&idx, &tmp, NULL, MAP_LAYER_LINES) == 0);
    build_mlp_ex(&tmp, 33, NULL, 0, 4, xy, 4);                            /* больше PURRGO_MAP_MAX_PARTS */
    CHECK(render(&idx, &tmp, NULL, MAP_LAYER_LINES) == 0);
    build_mlp_ex(&tmp, -1, NULL, 0, 4, xy, 4);
    CHECK(render(&idx, &tmp, NULL, MAP_LAYER_LINES) == 0);

    uint32_t bad_first[2] = { 1, 2 };                                     /* parts[0] != 0 */
    build_mlp(&tmp, 2, bad_first, 4, xy);
    CHECK(render(&idx, &tmp, NULL, MAP_LAYER_LINES) == 0);
    uint32_t not_increasing[2] = { 0, 0 };
    build_mlp(&tmp, 2, not_increasing, 4, xy);
    CHECK(render(&idx, &tmp, NULL, MAP_LAYER_LINES) == 0);
    uint32_t out_of_range[2] = { 0, 4 };                                  /* part == num_points */
    build_mlp(&tmp, 2, out_of_range, 4, xy);
    CHECK(render(&idx, &tmp, NULL, MAP_LAYER_LINES) == 0);

    /* v1 указывает за конец MLP / переполняет 32 + v1 */
    data_node(node, 100, 100, 900, 900, 1, 0x00FFFFFF, 0);
    build_idx(&idx, node, 1);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) == 0);
    data_node(node, 100, 100, 900, 900, 1, 0xFFFFFFF0u, 0);
    build_idx(&idx, node, 1);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) == 0);

    /* Усечённая геометрия: точек заявлено больше, чем есть в файле */
    build_mlp(&tmp, 1, p1, 4, xy);
    tmp.n -= 12;
    data_node(node, 100, 100, 900, 900, 1, GEOM_V1, 0);
    build_idx(&idx, node, 1);
    (void)render(&idx, &tmp, NULL, MAP_LAYER_LINES);
}

/* Один отрезок (100,100)-(900,900) в цепочке навигационных узлов глубины depth (LOD 0). */
static void build_nav_chain(blob_t *idx, blob_t *mlp, uint32_t depth)
{
    uint8_t leaf[25];
    data_node(leaf, 100, 100, 900, 900, 1, GEOM_V1, 0);
    ib_t b;
    ib_begin(&b, idx);
    ib_chain(&b, depth, leaf);
    ib_empty(&b); ib_empty(&b); ib_empty(&b);
    ib_end(&b);
    uint32_t parts[1] = { 0 };
    int32_t xy[4] = { 100, 100, 900, 900 };
    build_mlp(mlp, 1, parts, 2, xy);
}

static void test_nav_tree_depth_limit(void)
{
    static blob_t idx, mlp;

    for (uint32_t depth = 1; depth <= PURRGO_MAP_MAX_TREE_DEPTH; depth++) {
        build_nav_chain(&idx, &mlp, depth);
        CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) > 0 && g_ok);
    }
    /* Глубже предела читателя: отвергается по заголовку секции, не рекурсией. */
    for (uint32_t depth = PURRGO_MAP_MAX_TREE_DEPTH + 1u; depth <= 40u; depth += 9u) {
        build_nav_chain(&idx, &mlp, depth);
        CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    }
}

/* Нарушения раздела 7.1 и 6.3: узел отвергается, ничего не рисуется и ничего не читается за секцией. */
static void test_index_node_validation(void)
{
    static blob_t idx, mlp, bad;
    uint8_t leaf[25];
    data_node(leaf, 100, 100, 900, 900, 1, GEOM_V1, 0);
    build_idx(&idx, leaf, 1);
    uint32_t parts[1] = { 0 };
    int32_t xy[4] = { 100, 100, 900, 900 };
    build_mlp(&mlp, 1, parts, 2, xy);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) > 0 && g_ok);

    /* Корневой блок LOD 0 по смещению 0x400, единственный узел: child_block, bbox, v1, v2. */
    const uint32_t root = lod_at(&idx, 0) + BLK;
    const uint32_t sec_end = lod_at(&idx, 1);
    const uint32_t child_ok = root + BLK;

    uint32_t child_bad[] = {
        child_ok + 4u,           /* не кратно 512 */
        child_ok + 0x100u,
        root,                    /* указывает на собственный блок: child_block <= B */
        root - BLK,              /* до корневого блока (и в заголовок секции) */
        lod_at(&idx, 0),         /* сектор заголовка секции */
        sec_end,                 /* первый блок следующей секции */
        0xFFFFFE00u,             /* огромное смещение */
        0xFFFFFFF0u,
        0u,
    };
    for (size_t i = 0; i < sizeof child_bad / sizeof child_bad[0]; i++) {
        bad = idx; put_u32(bad.d + root, child_bad[i]);
        CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    }

    /* v1 (уровень) должен быть равен ожидаемому: у корня depth - 1 = 0. */
    uint32_t v1_bad[] = { 1, 2, 0xFFFFFFFFu };
    for (size_t i = 0; i < sizeof v1_bad / sizeof v1_bad[0]; i++) {
        bad = idx; put_u32(bad.d + root + 20, v1_bad[i]);
        CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    }

    /* v2: 1..20 для листового блока. */
    uint32_t v2_bad[] = { 0, 21, 255, 256, 0xFFFFFFFFu };
    for (size_t i = 0; i < sizeof v2_bad / sizeof v2_bad[0]; i++) {
        bad = idx; put_u32(bad.d + root + 24, v2_bad[i]);
        CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    }
    bad = idx; put_u32(bad.d + root + 24, 20);       /* 20 записей: остальные нули, не рисуются */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) > 0 && g_ok);

    /* Узел вне камеры тоже проверяется: ошибка в нём останавливает обход до рисования соседей. */
    {
        uint8_t leaves[2 * 25];
        static const int32_t far_bb[4] = { 5000, 5000, 6000, 6000 };
        data_node(leaves, 100, 100, 900, 900, 1, GEOM_V1, 0);
        data_node(leaves + 25, 5000, 5000, 6000, 6000, 1, GEOM_V1, 0);
        /* Два листовых блока под одним корнем: узел 0 в камере, узел 1 вне камеры. */
        ib_t b;
        ib_begin(&b, &bad);
        uint32_t s = ib_section(&b, 1, 2, 3);   /* корневой блок и два листовых */
        static const int32_t near_bb[4] = { 100, 100, 900, 900 };
        nav_node(bad.d + s + BLK, s + 2u * BLK, near_bb, 0, 1);
        nav_node(bad.d + s + BLK + 28, s + 3u * BLK, far_bb, 0, 1);
        memcpy(bad.d + s + 2u * BLK, leaves, 25);
        memcpy(bad.d + s + 3u * BLK, leaves + 25, 25);
        ib_empty(&b); ib_empty(&b); ib_empty(&b);
        ib_end(&b);
        CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) > 0 && g_ok);   /* целое дерево */
        put_u32(bad.d + s + BLK + 28, s + 3u * BLK + 4u);               /* у дальнего узла не кратно 512 */
        CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0 && !g_ok);
    }
}

/*
 * Блочное дерево: многоуровневые деревья с полным ветвлением 20/18 и малым 2/2 дают тот же
 * кадр, что и плоские списки; поддеревья вне камеры не читаются.
 */
static void test_block_tree(void)
{
    static blob_t idx, flat, mlp;
    static uint8_t leaves[1000 * 25];
    static uint8_t chunk[20 * 25];
    static uint8_t ref_fb[SCR_W * SCR_H];

    /* Сетка POI 30 x 30 с шагом 100 по обе стороны от камеры (0..1000): в кадр попадает около 100. */
    uint32_t n = 0;
    for (int gy = -10; gy < 20; gy++) {
        for (int gx = -10; gx < 20; gx++) {
            data_node(leaves + 25u * n, gx * 100 + 50, gy * 100 + 50, gx * 100 + 50, gy * 100 + 50,
                      PURRGO_FEATURE_POI_SMALL, 0, 0);
            n++;
        }
    }
    defaults();

    /* Эталон: те же узлы плоскими блоками по 20, кадры складываются. */
    memset(ref_fb, 0, sizeof ref_fb);
    for (uint32_t i = 0; i < n; i += 20) {
        uint32_t c = (n - i < 20) ? n - i : 20;
        memcpy(chunk, leaves + 25u * i, 25u * c);
        build_idx(&flat, chunk, c);
        (void)render(&flat, NULL, NULL, MAP_LAYER_POIS);
        for (int j = 0; j < SCR_W * SCR_H; j++) { if (g_fb[j]) { ref_fb[j] = g_fb[j]; } }
    }
    int ref_px = 0;
    for (int j = 0; j < SCR_W * SCR_H; j++) { ref_px += ref_fb[j] != 0; }
    CHECK(ref_px > 200);

    static const struct { uint32_t leaf_fan, nav_fan, count; } shapes[] = {
        { 20, 18, 900 },    /* 45 листовых блока, 3 корня уровня 1 -> depth 2 */
        { 20, 18, 361 },    /* 19 узлов уровня 0 не помещаются в корень -> depth 2 */
        { 20, 18, 360 },    /* ровно 18 корневых узлов -> depth 1 */
        { 2, 2, 17 },       /* малое ветвление: depth 5 */
        { 1, 2, 16 },       /* по одному узлу данных: depth 4 */
    };
    for (size_t k = 0; k < sizeof shapes / sizeof shapes[0]; k++) {
        const uint32_t cnt = shapes[k].count;
        ib_t b;
        ib_begin(&b, &idx);
        ib_tree(&b, leaves, cnt, shapes[k].leaf_fan, shapes[k].nav_fan);
        ib_empty(&b); ib_empty(&b); ib_empty(&b);
        ib_end(&b);
        CHECK(idx.n <= MAXF);

        (void)render(&idx, NULL, NULL, MAP_LAYER_POIS);
        CHECK(g_ok);

        /* Эталон для первых cnt узлов. */
        static uint8_t exp_fb[SCR_W * SCR_H];
        memset(exp_fb, 0, sizeof exp_fb);
        for (uint32_t i = 0; i < cnt; i += 20) {
            uint32_t c = (cnt - i < 20) ? cnt - i : 20;
            memcpy(chunk, leaves + 25u * i, 25u * c);
            build_idx(&flat, chunk, c);
            (void)render(&flat, NULL, NULL, MAP_LAYER_POIS);
            for (int j = 0; j < SCR_W * SCR_H; j++) { if (g_fb[j]) { exp_fb[j] = g_fb[j]; } }
        }
        (void)render(&idx, NULL, NULL, MAP_LAYER_POIS);
        CHECK(memcmp(g_fb, exp_fb, sizeof g_fb) == 0);
    }
    (void)mlp;

    /* Все 900 узлов: тот же кадр, что у эталона. */
    {
        ib_t b;
        ib_begin(&b, &idx);
        ib_tree(&b, leaves, n, 20, 18);
        ib_empty(&b); ib_empty(&b); ib_empty(&b);
        ib_end(&b);
        (void)render(&idx, NULL, NULL, MAP_LAYER_POIS);
        CHECK(g_ok && memcmp(g_fb, ref_fb, sizeof g_fb) == 0);
    }
}

/* Поддерево вне камеры пропускается без чтения его блока (раздел 7.2). */
static void test_block_skipped_without_read(void)
{
    static blob_t idx;
    uint8_t leaves[2 * 25];
    data_node(leaves, 500, 500, 500, 500, PURRGO_FEATURE_POI_BIG, 0, 0);
    data_node(leaves + 25, 5000, 5000, 5000, 5000, PURRGO_FEATURE_POI_BIG, 0, 0);

    /* Два корневых узла, каждый со своим листовым блоком: ближний в камере, дальний нет. */
    ib_t b;
    ib_begin(&b, &idx);
    uint32_t s = ib_section(&b, 1, 2, 3);   /* корневой блок и два листовых */
    static const int32_t near_bb[4] = { 500, 500, 500, 500 };
    static const int32_t far_bb[4] = { 5000, 5000, 5000, 5000 };
    nav_node(idx.d + s + BLK, s + 2u * BLK, near_bb, 0, 1);
    nav_node(idx.d + s + BLK + 28, s + 3u * BLK, far_bb, 0, 1);
    memcpy(idx.d + s + 2u * BLK, leaves, 25);
    memcpy(idx.d + s + 3u * BLK, leaves + 25, 25);
    ib_empty(&b); ib_empty(&b); ib_empty(&b);
    ib_end(&b);

    defaults();
    fake_fs_reset();
    fake_fs_put_bin("l.idx", idx.d, idx.n);
    purrgo_file_t *f = purrgo_fs_open("l.idx", FS_READ);
    gfx_reset();
    fake_fs_reset_read_stats();
    CHECK(purrgo_map_render_layer(f, NULL, NULL, &g_gfx, &CAM, &VP, &g_params, MAP_LAYER_POIS));
    /* Чтения: заголовок PGO, заголовок секции, корневой блок, один листовой блок. */
    CHECK(fake_fs_read_calls() == 4);
    CHECK(fake_fs_max_read_end() == s + 3u * BLK);   /* дальний блок (s + 3 * 512 .. +512) не читался */
    CHECK(g_pixels > 5);
    (void)purrgo_fs_close(f);
}

/*
 * Выбор LOD по масштабу и детализации. В секции каждого LOD один узел данных со своим bbox
 * (100..xe[lod] по x), геометрия общая. Камера, начинающаяся правее конца узла LOD k - 1 и
 * внутри узла LOD k, видит узел только если выбран LOD >= k; камера правее конца узла
 * выбранного LOD ничего не рисует. Так проверяется выбор в обе стороны.
 */
static void test_lod_selection(void)
{
    static blob_t idx, mlp;
    uint8_t leaf[25];
    static const int32_t xe[4] = { 200, 400, 700, 900 };   /* конец узла по x */
    ib_t b;
    ib_begin(&b, &idx);
    for (int lod = 0; lod < 4; lod++) {
        data_node(leaf, 100, 500, xe[lod], 500, 1, GEOM_V1, 0);
        ib_tree(&b, leaf, 1, 20, 18);
    }
    ib_end(&b);
    uint32_t parts[1] = { 0 };
    int32_t xy[4] = { 100, 500, 900, 500 };
    build_mlp(&mlp, 1, parts, 2, xy);

    static const struct { purrgo_map_scale_t scale; purrgo_map_details_t det; int lod; } t[] = {
        { PURRGO_MAP_SCALE_10M,     PURRGO_MAP_DETAILS_LOW,  0 },
        { PURRGO_MAP_SCALE_100M,    PURRGO_MAP_DETAILS_LOW,  0 },
        { PURRGO_MAP_SCALE_200M,    PURRGO_MAP_DETAILS_LOW,  1 },
        { PURRGO_MAP_SCALE_2KM,     PURRGO_MAP_DETAILS_LOW,  1 },
        { PURRGO_MAP_SCALE_5KM,     PURRGO_MAP_DETAILS_LOW,  2 },
        { PURRGO_MAP_SCALE_10KM,    PURRGO_MAP_DETAILS_LOW,  2 },
        { PURRGO_MAP_SCALE_20KM,    PURRGO_MAP_DETAILS_LOW,  3 },
        { PURRGO_MAP_SCALE_10000KM, PURRGO_MAP_DETAILS_LOW,  3 },
        { PURRGO_MAP_SCALE_10M,     PURRGO_MAP_DETAILS_HIGH, 0 },
        { PURRGO_MAP_SCALE_500M,    PURRGO_MAP_DETAILS_HIGH, 0 },
        { PURRGO_MAP_SCALE_1KM,     PURRGO_MAP_DETAILS_HIGH, 1 },
        { PURRGO_MAP_SCALE_5KM,     PURRGO_MAP_DETAILS_HIGH, 1 },
        { PURRGO_MAP_SCALE_10KM,    PURRGO_MAP_DETAILS_HIGH, 2 },
        { PURRGO_MAP_SCALE_20KM,    PURRGO_MAP_DETAILS_HIGH, 2 },
        { PURRGO_MAP_SCALE_50KM,    PURRGO_MAP_DETAILS_HIGH, 3 },
        { PURRGO_MAP_SCALE_10000KM, PURRGO_MAP_DETAILS_HIGH, 3 },
    };
    fake_fs_reset();
    fake_fs_put_bin("l.idx", idx.d, idx.n);
    fake_fs_put_bin("l.mlp", mlp.d, mlp.n);

    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        const int lod = t[i].lod;
        defaults();
        g_params.scale = t[i].scale;
        g_params.details = t[i].det;

        /* Камера внутри узла выбранного LOD: виден. */
        purrgo_bbox_t cam = { xe[lod] - 50, 0, 1000, 1000 };
        purrgo_file_t *fi = purrgo_fs_open("l.idx", FS_READ);
        purrgo_file_t *fm = purrgo_fs_open("l.mlp", FS_READ);
        gfx_reset();
        g_ok = purrgo_map_render_layer(fi, fm, NULL, &g_gfx, &cam, &VP, &g_params, MAP_LAYER_LINES);
        CHECK(g_ok && g_pixels > 0);
        (void)purrgo_fs_close(fi); (void)purrgo_fs_close(fm);

        /* Камера правее конца узла выбранного LOD, но внутри узла следующего: не виден. */
        if (lod < 3) {
            cam.min_x = xe[lod + 1] - 50;
            fi = purrgo_fs_open("l.idx", FS_READ);
            fm = purrgo_fs_open("l.mlp", FS_READ);
            gfx_reset();
            g_ok = purrgo_map_render_layer(fi, fm, NULL, &g_gfx, &cam, &VP, &g_params, MAP_LAYER_LINES);
            CHECK(g_ok && g_pixels == 0);
            (void)purrgo_fs_close(fi); (void)purrgo_fs_close(fm);
        }
    }
}

/* Подпись на карте из поля названия (раздел 10.6.1). */
static void test_label_format(void)
{
    char out[PURRGO_MAP_LABEL_MAX + 1];
    uint8_t f[PURRGO_MAP_LABEL_MAX + 1];

    /* Короткое название: как есть, "_" - пробел. */
    memset(f, 0, sizeof f); memcpy(f, "Minsk_Arena", 11);
    CHECK(map_label_format(f, 11, out) == 11 && strcmp(out, "Minsk Arena") == 0);

    /* Ровно 22 символа не сокращается, 23 - сокращается до 20 и "..". */
    memset(f, 'a', sizeof f); f[22] = 0;
    CHECK(map_label_format(f, 22, out) == 22 && strlen(out) == 22 && out[21] == 'a');
    memset(f, 'a', sizeof f);                          /* 23 байта без завершающего нуля */
    CHECK(map_label_format(f, 23, out) == 22);
    CHECK(strncmp(out, "aaaaaaaaaaaaaaaaaaaa..", 22) == 0 && out[22] == '\0');

    /* Хвостовые "_" перед многоточием убираются, как в компиляторе V3. */
    memcpy(f, "Great_Lake_Of_Minsk_X_Y", 23);
    CHECK(map_label_format(f, 23, out) == 21);          /* первые 20 символов кончаются на "_": он убирается */
    CHECK(strcmp(out, "Great Lake Of Minsk..") == 0);
    memcpy(f, "abcdefghijklmnopqr___xyz", 23);
    CHECK(map_label_format(f, 23, out) > 0 && strcmp(out, "abcdefghijklmnopqr..") == 0);

    /* Пустое поле и поле из нулей: подписи нет. */
    memset(f, 0, sizeof f);
    CHECK(map_label_format(f, 5, out) == 0 && out[0] == '\0');
    CHECK(map_label_format(f, 0, out) == 0 && out[0] == '\0');

    /* Байты PurrGO-256 выше 0x7F сохраняются без изменений. */
    memset(f, 0, sizeof f); f[0] = 0xC0; f[1] = '_'; f[2] = 0xFF;
    CHECK(map_label_format(f, 3, out) == 3);
    CHECK((uint8_t)out[0] == 0xC0 && out[1] == ' ' && (uint8_t)out[2] == 0xFF);
}

static void test_poi_layer(void)
{
    static blob_t idx;
    uint8_t node[25];
    data_node(node, 500, 500, 500, 500, PURRGO_FEATURE_POI_BIG, 0, 0);
    build_idx(&idx, node, 1);

    defaults();
    g_params.poi_mode = PURRGO_POI_MODE_NO;
    CHECK(render(&idx, NULL, NULL, MAP_LAYER_POIS) == 0);
    g_params.poi_mode = PURRGO_POI_MODE_CIRCLES;
    CHECK(render(&idx, NULL, NULL, MAP_LAYER_POIS) > 5);
}

/* Все сочетания «подмена одного байта» для .idx, .mlp и .db. */
static void test_fuzz_truncate_and_flip(void)
{
    static blob_t idx, mlp, db, m;
    simple_map(&idx, &mlp, 1);

    /* .db с двумя записями (129 + 117 * n байт заголовка/записей после 32-байтного PGO). */
    memset(&db, 0, sizeof db);
    pgo_header(db.d, 3, 129u + 117u * 3u, NULL);
    memcpy(db.d + 32u + 129u + 117u * 1u + 17u, "Lake", 5);
    memcpy(db.d + 32u + 129u + 117u * 2u + 17u, "Forest", 7);
    db.n = 32u + 129u + 117u * 3u;

    defaults();
    g_params.layer_poi_labels = true;
    long base = render(&idx, &mlp, &db, MAP_LAYER_LINES);
    CHECK(base > 0);

    /* Усечение каждого файла в каждой точке. */
    for (uint32_t cut = 0; cut <= idx.n; cut++) {
        m = idx; m.n = cut;
        CHECK(render(&m, &mlp, &db, MAP_LAYER_LINES) <= base);
    }
    for (uint32_t cut = 0; cut <= mlp.n; cut++) {
        m = mlp; m.n = cut;
        CHECK(render(&idx, &m, &db, MAP_LAYER_LINES) <= base);
    }

    /* Подмена каждого байта несколькими значениями; LOD 0 (масштаб 10 м) и LOD 3 (10000 км). */
    static const uint8_t vals[] = { 0x00, 0x01, 0x7F, 0x80, 0xFF };
    static const purrgo_map_scale_t scales[] = { PURRGO_MAP_SCALE_10M, PURRGO_MAP_SCALE_10000KM };
    for (size_t sc = 0; sc < sizeof scales / sizeof scales[0]; sc++) {
        g_params.scale = scales[sc];
        for (uint32_t i = 0; i < idx.n; i++) {
            for (size_t v = 0; v < sizeof vals; v++) {
                m = idx; m.d[i] = vals[v];
                (void)render(&m, &mlp, &db, MAP_LAYER_LINES);
                (void)render(&m, &mlp, &db, MAP_LAYER_POLYGONS);
                (void)render(&m, NULL, &db, MAP_LAYER_POIS);
            }
        }
    }
    g_params.scale = PURRGO_MAP_SCALE_10M;
    for (uint32_t i = 0; i < mlp.n; i++) {
        for (size_t v = 0; v < sizeof vals; v++) {
            m = mlp; m.d[i] = vals[v];
            (void)render(&idx, &m, &db, MAP_LAYER_LINES);
            (void)render(&idx, &m, &db, MAP_LAYER_POLYGONS);
        }
    }
    CHECK(1);   /* цель - отсутствие срабатываний ASan/UBSan */
}

/* --- вода: чёрное с тёмно-серой обводкой (g_fb хранит цвет + 1) --- */

#define FB_BLACK     (BLACK + 1)
#define FB_DARK_GRAY (DARK_GRAY + 1)

static int count_fb(uint8_t v)
{
    int n = 0;
    for (int i = 0; i < SCR_W * SCR_H; i++) { if (g_fb[i] == v) { n++; } }
    return n;
}

static void test_water_polygon_style(void)
{
    static blob_t idx, mlp;
    uint8_t node[25];
    data_node(node, 100, 100, 900, 900, PURRGO_FEATURE_WATER, GEOM_V1, 0);
    build_idx(&idx, node, 1);
    uint32_t parts[1] = { 0 };
    int32_t xy[10] = { 100, 100, 900, 100, 900, 900, 100, 900, 100, 100 };
    build_mlp(&mlp, 1, parts, 5, xy);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_POLYGONS) > 100);

    CHECK(g_fb[32 * SCR_W + 32] == FB_BLACK);        /* заливка чёрная */
    CHECK(count_fb(FB_DARK_GRAY) > 30);              /* контур тёмно-серый */
    CHECK(count_fb(FB_BLACK) > count_fb(FB_DARK_GRAY));
    CHECK(g_gfx.color_fg == 1);   /* foreground восстановлен */
}

static void waterway_line(blob_t *idx, blob_t *mlp, uint8_t type)
{
    uint8_t node[25];
    data_node(node, 100, 500, 900, 500, type, GEOM_V1, 0);
    build_idx(idx, node, 1);
    uint32_t parts[1] = { 0 };
    int32_t xy[6] = { 100, 500, 500, 500, 900, 500 };
    build_mlp(mlp, 1, parts, 3, xy);
}

static void test_waterway_line_style(void)
{
    static blob_t idx, mlp;

    /* Малый водоток: чёрная линия 1 px, без обводки. */
    waterway_line(&idx, &mlp, PURRGO_FEATURE_WATERWAY_MINOR);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) > 20);
    CHECK(g_fb[32 * SCR_W + 32] == FB_BLACK);
    CHECK(g_fb[31 * SCR_W + 32] == 0 && g_fb[33 * SCR_W + 32] == 0);
    CHECK(count_fb(FB_DARK_GRAY) == 0);

    /* Крупный водоток: чёрная линия 3 px, без обводки. */
    waterway_line(&idx, &mlp, PURRGO_FEATURE_WATERWAY_MAJOR);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) > 20);
    CHECK(g_fb[31 * SCR_W + 32] == FB_BLACK);
    CHECK(g_fb[32 * SCR_W + 32] == FB_BLACK);
    CHECK(g_fb[33 * SCR_W + 32] == FB_BLACK);
    CHECK(g_fb[30 * SCR_W + 32] == 0 && g_fb[34 * SCR_W + 32] == 0);
    CHECK(count_fb(FB_DARK_GRAY) == 0);

    /* Излом: оба звена чёрные, вершина без разрыва. */
    uint8_t node[25];
    data_node(node, 100, 100, 900, 900, PURRGO_FEATURE_WATERWAY_MINOR, GEOM_V1, 0);
    build_idx(&idx, node, 1);
    uint32_t parts[1] = { 0 };
    int32_t xy[6] = { 100, 500, 500, 500, 500, 900 };
    build_mlp(&mlp, 1, parts, 3, xy);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) > 20);
    CHECK(g_fb[32 * SCR_W + 20] == FB_BLACK);
    CHECK(g_fb[32 * SCR_W + 32] == FB_BLACK);        /* вершина излома */
    CHECK(g_fb[14 * SCR_W + 32] == FB_BLACK);
}

/* Длинные объекты (сотни точек): не отбрасываются, как раньше при лимите 512. */
static void test_long_geometry(void)
{
    static blob_t idx, mlp;
    static int32_t xy[2 * 2000];

    uint8_t node[25];
    data_node(node, 100, 500, 900, 500, PURRGO_FEATURE_WATERWAY_MAJOR, GEOM_V1, 0);
    build_idx(&idx, node, 1);
    uint32_t parts[1] = { 0 };

    /* Река 1500 точек вдоль y = 500. */
    for (int i = 0; i < 1500; i++) { xy[2 * i] = 100 + i * 800 / 1499; xy[2 * i + 1] = 500; }
    build_mlp(&mlp, 1, parts, 1500, xy);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) > 100);
    CHECK(g_fb[32 * SCR_W + 32] == FB_BLACK);

    /* Ровно PURRGO_MAP_MAX_POINTS точек: ещё рисуется, на одну больше - нет. */
    for (int i = 0; i < 2000; i++) { xy[2 * i] = 100 + i * 800 / 1999; xy[2 * i + 1] = 500; }
    build_mlp(&mlp, 1, parts, 2000, xy);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) > 100);
    build_mlp_ex(&mlp, 1, parts, 1, 2001, xy, 2000);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) == 0);

    /* Полигон воды из 1000 точек (окружность): заливка и контур на месте. */
    data_node(node, 100, 100, 900, 900, PURRGO_FEATURE_WATER, GEOM_V1, 0);
    build_idx(&idx, node, 1);
    for (int i = 0; i < 1000; i++) {
        double a = 6.283185307179586 * i / 999.0;
        xy[2 * i] = 500 + (int32_t)(400.0 * cos(a));
        xy[2 * i + 1] = 500 + (int32_t)(400.0 * sin(a));
    }
    xy[2 * 999] = xy[0]; xy[2 * 999 + 1] = xy[1];
    build_mlp(&mlp, 1, parts, 1000, xy);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_POLYGONS) > 500);
    CHECK(g_fb[32 * SCR_W + 32] == FB_BLACK);
}

/* Слои water/waterways через viewport: опциональные, включаются layer_water. */
static void test_water_layers_viewport(void)
{
    static blob_t widx, wmlp, lidx, lmlp;
    uint8_t node[25];
    data_node(node, 100, 100, 900, 900, PURRGO_FEATURE_WATER, GEOM_V1, 0);
    build_idx(&widx, node, 1);
    uint32_t parts[1] = { 0 };
    int32_t xy[10] = { 100, 100, 900, 100, 900, 900, 100, 900, 100, 100 };
    build_mlp(&wmlp, 1, parts, 5, xy);
    waterway_line(&lidx, &lmlp, PURRGO_FEATURE_WATERWAY_MINOR);

    /* Карта без water.* и waterways.*: слой включён, кадр успешен, ошибок нет. */
    fake_fs_reset();
    defaults();
    g_params.layer_water = true;
    g_params.layer_water_labels = true;
    purrgo_map_shutdown();
    gfx_reset();
    CHECK(render_dir("maps/nowater"));
    CHECK(g_pixels == 0);
    int opens = fake_fs_open_calls();
    gfx_reset();
    CHECK(render_dir("maps/nowater"));
    CHECK(fake_fs_open_calls() == opens);            /* повторных попыток нет */
    purrgo_map_shutdown();

    /* Только waterways.* (без water.*): линии рисуются. */
    fake_fs_put_bin("maps/ww/waterways.idx", lidx.d, lidx.n);
    fake_fs_put_bin("maps/ww/waterways.mlp", lmlp.d, lmlp.n);
    gfx_reset();
    CHECK(render_dir("maps/ww"));
    CHECK(g_pixels > 20);
    purrgo_map_shutdown();

    /* Оба слоя: водоём и водоток; слой выключен - не рисуются. */
    fake_fs_put_bin("maps/w/water.idx", widx.d, widx.n);
    fake_fs_put_bin("maps/w/water.mlp", wmlp.d, wmlp.n);
    fake_fs_put_bin("maps/w/waterways.idx", lidx.d, lidx.n);
    fake_fs_put_bin("maps/w/waterways.mlp", lmlp.d, lmlp.n);
    gfx_reset();
    CHECK(render_dir("maps/w"));
    CHECK(g_pixels > 100);
    CHECK(g_fb[10 * SCR_W + 32] == FB_BLACK);        /* водоём */
    purrgo_map_shutdown();

    g_params.layer_water = false;
    gfx_reset();
    CHECK(render_dir("maps/w"));
    CHECK(g_pixels == 0);
    purrgo_map_shutdown();
    CHECK(fake_fs_open_handles() == 0);
}

/* Полный путь purrgo_map_render_viewport через (фейковую) ФС. */
static void test_viewport_via_fs(void)
{
    static blob_t idx, mlp;
    simple_map(&idx, &mlp, 1);

    fake_fs_reset();
    defaults();
    g_params.layer_roads = true;
    purrgo_map_shutdown();
    fake_fs_put_bin("maps/m1/roads.idx", idx.d, idx.n);
    fake_fs_put_bin("maps/m1/roads.mlp", mlp.d, mlp.n);

    gfx_reset();
    bool ok = render_dir("maps/m1");
    CHECK(ok);
    CHECK(g_pixels > 20);
    purrgo_map_shutdown();
    CHECK(fake_fs_open_handles() == 0);                /* shutdown закрывает все файлы */

    /* Повреждённый .idx в файле: кадр без падения, пикселей нет. */
    idx.d[0] = 0;
    fake_fs_put_bin("maps/m2/roads.idx", idx.d, idx.n);
    fake_fs_put_bin("maps/m2/roads.mlp", mlp.d, mlp.n);
    gfx_reset();
    (void)render_dir("maps/m2");
    CHECK(g_pixels == 0);
    purrgo_map_shutdown();
    CHECK(fake_fs_open_handles() == 0);
}

/*
 * Пропуск мелких объектов: камера 1000x1000 на вьюпорте 64x64 даёт 15.6 единицы
 * на пиксель. Порог линий 2 px (31.2 единицы), полигонов 3 px (46.9).
 */
static void test_feature_too_small(void)
{
    const purrgo_bbox_t cam = { 0, 0, 1000, 1000 };
    const purrgo_viewport_t vp = { 64, 64, 0, 0 };

    CHECK(map_feature_too_small(500, 500, 505, 505, &cam, &vp, 2));      /* 0.3 px */
    CHECK(!map_feature_too_small(500, 500, 540, 540, &cam, &vp, 2));     /* 2.6 px */
    CHECK(map_feature_too_small(500, 500, 540, 540, &cam, &vp, 3));      /* 2.6 px < 3 */
    /* Тонкий, но длинный объект рисуется: одна сторона больше порога. */
    CHECK(!map_feature_too_small(100, 500, 900, 501, &cam, &vp, 2));
    CHECK(!map_feature_too_small(500, 100, 501, 900, &cam, &vp, 2));
    /* Ровно порог (2 px = 31.25 единицы) не пропускается: 32 * 64 = 2048 >= 2 * 1000. */
    CHECK(!map_feature_too_small(0, 0, 32, 32, &cam, &vp, 2));
    CHECK(map_feature_too_small(0, 0, 31, 31, &cam, &vp, 2));
    /* Выключено или некорректные параметры: не пропускаем. */
    CHECK(!map_feature_too_small(500, 500, 501, 501, &cam, &vp, 0));
    CHECK(!map_feature_too_small(500, 500, 501, 501, NULL, &vp, 2));
    CHECK(!map_feature_too_small(500, 500, 501, 501, &cam, NULL, 2));
    const purrgo_viewport_t empty = { 0, 0, 0, 0 };
    CHECK(!map_feature_too_small(500, 500, 501, 501, &cam, &empty, 2));
    const purrgo_bbox_t wrap = { 900, 0, 100, 1000 };                    /* через антимеридиан */
    CHECK(!map_feature_too_small(500, 500, 501, 501, &wrap, &vp, 2));
    const purrgo_bbox_t flat = { 0, 5, 1000, 5 };
    CHECK(!map_feature_too_small(500, 500, 501, 501, &flat, &vp, 2));
    /* Крайние координаты: произведения не переполняют int64. */
    const purrgo_bbox_t huge = { INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX };
    const purrgo_viewport_t big = { 65535, 65535, 0, 0 };
    CHECK(!map_feature_too_small(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX, &huge, &big, 3));
    CHECK(map_feature_too_small(0, 0, 1, 1, &huge, &vp, 3));
}

static void test_small_features_skipped(void)
{
    static blob_t idx, mlp;
    uint32_t parts[1] = { 0 };

    defaults();

    /* Линия: 0.3 px пропускается, 2.6 px рисуется. */
    uint8_t node[25];
    int32_t tiny_line[4] = { 500, 500, 505, 505 };
    data_node(node, 500, 500, 505, 505, 1, GEOM_V1, 0);
    build_idx(&idx, node, 1);
    build_mlp(&mlp, 1, parts, 2, tiny_line);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) == 0);

    int32_t line[4] = { 500, 500, 540, 540 };
    data_node(node, 500, 500, 540, 540, 1, GEOM_V1, 0);
    build_idx(&idx, node, 1);
    build_mlp(&mlp, 1, parts, 2, line);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) > 0);

    /* Полигон 2.6 px (< 3) пропускается, 3.8 px рисуется. */
    int32_t small_poly[8] = { 500, 500, 540, 500, 540, 540, 500, 540 };
    data_node(node, 500, 500, 540, 540, PURRGO_FEATURE_WATER, GEOM_V1, 0);
    build_idx(&idx, node, 1);
    build_mlp(&mlp, 1, parts, 4, small_poly);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_POLYGONS) == 0);

    int32_t poly[8] = { 500, 500, 560, 500, 560, 560, 500, 560 };
    data_node(node, 500, 500, 560, 560, PURRGO_FEATURE_WATER, GEOM_V1, 0);
    build_idx(&idx, node, 1);
    build_mlp(&mlp, 1, parts, 4, poly);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_POLYGONS) > 0);
}

/*
 * Через purrgo_map_render_viewport: read-ahead включается только у .mlp, а слой
 * POI при poi_mode == NO не читается с носителя вовсе.
 */
static void test_viewport_read_ahead_and_poi_skip(void)
{
    static blob_t idx, mlp, poi;
    simple_map(&idx, &mlp, 1);

    fake_fs_reset();
    defaults();
    g_params.layer_roads = true;
    purrgo_map_shutdown();
    fake_fs_put_bin("maps/ra/roads.idx", idx.d, idx.n);
    fake_fs_put_bin("maps/ra/roads.mlp", mlp.d, mlp.n);
    gfx_reset();
    CHECK(render_dir("maps/ra"));
#if PURRGO_MAP_MLP_ADAPTIVE_RA
    CHECK(fake_fs_adaptive_read_ahead("maps/ra/roads.mlp") == PURRGO_MAP_MLP_READ_AHEAD_SECTORS);
    CHECK(fake_fs_read_ahead("maps/ra/roads.mlp") == 0);                /* фиксированный не задаётся */
#else
    CHECK(fake_fs_read_ahead("maps/ra/roads.mlp") == PURRGO_MAP_MLP_READ_AHEAD_SECTORS);
#endif
    CHECK(fake_fs_read_ahead("maps/ra/roads.idx") == 0);                /* для .idx не задаётся */
    purrgo_map_shutdown();

    uint8_t node[25];
    data_node(node, 500, 500, 500, 500, PURRGO_FEATURE_POI_BIG, 0, 0);
    build_idx(&poi, node, 1);

    fake_fs_reset();
    defaults();
    g_params.layer_poi = true;
    g_params.poi_mode = PURRGO_POI_MODE_NO;
    purrgo_map_shutdown();
    fake_fs_put_bin("maps/poi/pois.idx", poi.d, poi.n);
    fake_fs_reset_read_stats();
    gfx_reset();
    (void)render_dir("maps/poi");
    CHECK(fake_fs_read_calls() == 0);                                   /* pois.idx не читался */
    CHECK(g_pixels == 0);
    purrgo_map_shutdown();

    g_params.poi_mode = PURRGO_POI_MODE_CIRCLES;
    fake_fs_reset_read_stats();
    gfx_reset();
    (void)render_dir("maps/poi");
    CHECK(fake_fs_read_calls() > 0);
    CHECK(g_pixels > 5);
    purrgo_map_shutdown();
}

/* Раздел 2 (карта): повторные открытия, признак успеха кадра, длинный map_dir. */
static void test_viewport_failures(void)
{
    static blob_t idx, mlp;

    /* Слой включён, файлов нет: один проход открытий, дальше без обращений к ФС. */
    fake_fs_reset();
    defaults();
    g_params.layer_landuse = true;
    purrgo_map_shutdown();
    gfx_reset();
    bool ok = render_dir("maps/none");
    CHECK(ok);                                  /* отсутствие файлов - не ошибка отрисовки */
    int after_first = fake_fs_open_calls();
    CHECK(after_first > 0);
    (void)render_dir("maps/none");
    CHECK(fake_fs_open_calls() == after_first);

    /* Выключение и включение слоя - новая попытка (файл мог появиться). */
    simple_map(&idx, &mlp, 1);
    fake_fs_put_bin("maps/none/landuse.idx", idx.d, idx.n);
    fake_fs_put_bin("maps/none/landuse.mlp", mlp.d, mlp.n);
    (void)render_dir("maps/none");
    CHECK(fake_fs_open_calls() == after_first);  /* память о неудаче сохраняется */
    g_params.layer_landuse = false;
    (void)render_dir("maps/none");
    g_params.layer_landuse = true;
    gfx_reset();
    ok = render_dir("maps/none");
    CHECK(ok);
    CHECK(fake_fs_open_calls() > after_first);     /* файлы открыты заново */
    purrgo_map_shutdown();

    /* Смена каталога сбрасывает память о неудаче. */
    fake_fs_reset();
    defaults();
    g_params.layer_roads = true;
    (void)render_dir("maps/a");
    int calls = fake_fs_open_calls();
    (void)render_dir("maps/b");
    CHECK(fake_fs_open_calls() > calls);
    purrgo_map_shutdown();

    /* Успех кадра - по результату отрисовки: битый индекс -> false, целый -> true. */
    fake_fs_reset();
    defaults();
    simple_map(&idx, &mlp, 1);
    fake_fs_put_bin("maps/good/roads.idx", idx.d, idx.n);
    fake_fs_put_bin("maps/good/roads.mlp", mlp.d, mlp.n);
    idx.d[0] = 0;
    fake_fs_put_bin("maps/bad/roads.idx", idx.d, idx.n);
    fake_fs_put_bin("maps/bad/roads.mlp", mlp.d, mlp.n);
    g_params.layer_roads = true;
    purrgo_map_shutdown();
    gfx_reset();
    CHECK(render_dir("maps/good"));
    CHECK(!render_dir("maps/bad"));
    purrgo_map_shutdown();
    CHECK(fake_fs_open_handles() == 0);

    /* Слой POI: отсутствие pois.idx - не ошибка, битый pois.idx - ошибка. */
    fake_fs_reset();
    defaults();
    g_params.layer_poi = true;
    CHECK(render_dir("maps/nopoi"));
    fake_fs_put_bin("maps/badpoi/pois.idx", idx.d, idx.n);   /* idx.d[0] == 0 */
    CHECK(!render_dir("maps/badpoi"));
    purrgo_map_shutdown();

    /* Слишком длинный map_dir: без обрезки и без открытий, кадр помечается неудачным. */
    fake_fs_reset();
    defaults();
    g_params.layer_roads = true;
    char longdir[300];
    memset(longdir, 'd', sizeof longdir - 1);
    longdir[sizeof longdir - 1] = '\0';
    purrgo_map_shutdown();
    CHECK(!render_dir(longdir));
    CHECK(fake_fs_open_calls() == 0);
    CHECK(fake_fs_open_handles() == 0);

    /* Два каталога с общим длинным префиксом различаются (раньше strncmp их склеивал). */
    char d1[236], d2[236];
    memset(d1, 'e', 235); d1[235] = '\0';
    memcpy(d2, d1, sizeof d1);
    d2[234] = 'f';
    simple_map(&idx, &mlp, 1);
    char path[300];
    snprintf(path, sizeof path, "%s/roads.idx", d1);
    fake_fs_put_bin(path, idx.d, idx.n);
    snprintf(path, sizeof path, "%s/roads.mlp", d1);
    fake_fs_put_bin(path, mlp.d, mlp.n);
    gfx_reset();
    CHECK(render_dir(d1));
    CHECK(g_pixels > 0);
    gfx_reset();
    (void)render_dir(d2);   /* файлов нет -> пусто */
    CHECK(g_pixels == 0);
    purrgo_map_shutdown();
    CHECK(fake_fs_open_handles() == 0);
}

/*
 * Диапазоны feature code по слоям (docs/map-format-v4.md, 8.1) и таблица иконок:
 * номер иконки POI равен feature code, UI-иконки лежат в 192..223, остальные иконки пусты.
 */
static int in_range(uint32_t code, uint32_t first, uint32_t last)
{
    return code >= first && code <= last;
}

static int icon_is_empty(unsigned no)
{
    for (unsigned r = 0; r < PURRGO_POI_ICON_HEIGHT; r++) {
        for (unsigned c = 0; c < PURRGO_POI_ICON_WIDTH; c++) {
            if (purrgo_poi_icons[no][r][c] & PURRGO_POI_ALPHA) { return 0; }
        }
    }
    return 1;
}

static void test_feature_code_ranges(void)
{
    CHECK(in_range(PURRGO_FEATURE_NO_CLASS, PURRGO_FEATURE_ROADS_FIRST, PURRGO_FEATURE_ROADS_LAST));
    CHECK(in_range(PURRGO_FEATURE_ROAD_MAJOR, PURRGO_FEATURE_ROADS_FIRST, PURRGO_FEATURE_ROADS_LAST));
    CHECK(in_range(PURRGO_FEATURE_RAILWAY, PURRGO_FEATURE_ROADS_FIRST, PURRGO_FEATURE_ROADS_LAST));
    CHECK(in_range(PURRGO_FEATURE_WATERWAY_MAJOR, PURRGO_FEATURE_WATERWAYS_FIRST, PURRGO_FEATURE_WATERWAYS_LAST));
    CHECK(in_range(PURRGO_FEATURE_WATERWAY_MINOR, PURRGO_FEATURE_WATERWAYS_FIRST, PURRGO_FEATURE_WATERWAYS_LAST));
    CHECK(in_range(PURRGO_FEATURE_WATER, PURRGO_FEATURE_WATER_FIRST, PURRGO_FEATURE_WATER_LAST));
    CHECK(in_range(PURRGO_FEATURE_WETLAND, PURRGO_FEATURE_WATER_FIRST, PURRGO_FEATURE_WATER_LAST));
    CHECK(in_range(PURRGO_FEATURE_GLACIER, PURRGO_FEATURE_WATER_FIRST, PURRGO_FEATURE_WATER_LAST));
    CHECK(in_range(PURRGO_FEATURE_LANDUSE_NATURAL, PURRGO_FEATURE_LANDUSE_FIRST, PURRGO_FEATURE_LANDUSE_LAST));
    CHECK(in_range(PURRGO_FEATURE_LANDUSE_HUMAN, PURRGO_FEATURE_LANDUSE_FIRST, PURRGO_FEATURE_LANDUSE_LAST));
    CHECK(in_range(PURRGO_FEATURE_SAND, PURRGO_FEATURE_LANDUSE_FIRST, PURRGO_FEATURE_LANDUSE_LAST));
    CHECK(in_range(PURRGO_FEATURE_POI_BIG, PURRGO_FEATURE_POI_FIRST, PURRGO_FEATURE_POI_LAST));
    CHECK(in_range(PURRGO_FEATURE_POI_SMALL, PURRGO_FEATURE_POI_FIRST, PURRGO_FEATURE_POI_LAST));

    /* Каждый код, у которого есть стиль, лежит в диапазоне линий/полигонов. */
    for (uint32_t code = 0; code <= 255; code++) {
        if (!purrgo_map_style_is_visible(code)) { continue; }
        if (code == PURRGO_FEATURE_POI_BIG || code == PURRGO_FEATURE_POI_SMALL) { continue; }
        CHECK(code < PURRGO_FEATURE_POI_FIRST);
    }

    /* Старые коды 8..41 (до разделения диапазонов) стилей больше не имеют. */
    for (uint32_t code = 8; code <= 31; code++) {
        CHECK(!purrgo_map_style_is_visible(code));
    }
    for (uint32_t code = 34; code <= 41; code++) {
        CHECK(!purrgo_map_style_is_visible(code));
    }

    /* Иконки: непустые только в диапазоне POI и у UI-иконок (35, 46, 47). */
    for (unsigned no = 0; no < PURRGO_POI_ICON_COUNT; no++) {
        const int is_ui = in_range(no, PURRGO_UI_ICON_FIRST, PURRGO_UI_ICON_LAST);
        if (!in_range(no, PURRGO_FEATURE_POI_FIRST, PURRGO_FEATURE_POI_LAST) && !is_ui) {
            CHECK(icon_is_empty(no));
        }
    }
    CHECK(!icon_is_empty(PURRGO_FEATURE_POI_BIG));
    CHECK(!icon_is_empty(PURRGO_FEATURE_POI_SMALL));
    CHECK(!icon_is_empty(PURRGO_UI_ICON_WPT_TARGET));
    CHECK(!icon_is_empty(PURRGO_UI_ICON_WPT_LAYER));
    CHECK(!icon_is_empty(PURRGO_UI_ICON_MAP_CROSSHAIR));
    CHECK(PURRGO_UI_ICON_FIRST == PURRGO_FEATURE_UI_FIRST && PURRGO_UI_ICON_LAST == PURRGO_FEATURE_UI_LAST);
}

int main(void)
{
    defaults();
    test_valid_line();
    test_valid_polygon();
    test_outside_camera_not_drawn();
    test_bad_pgo_header();
    test_bad_sqt_header();
    test_bad_mlp_geometry();
    test_nav_tree_depth_limit();
    test_index_node_validation();
    test_block_tree();
    test_block_skipped_without_read();
    test_lod_selection();
    test_label_format();
    test_poi_layer();
    test_fuzz_truncate_and_flip();
    test_water_polygon_style();
    test_waterway_line_style();
    test_long_geometry();
    test_water_layers_viewport();
    test_viewport_via_fs();
    test_viewport_failures();
    test_feature_too_small();
    test_small_features_skipped();
    test_viewport_read_ahead_and_poi_skip();
    test_feature_code_ranges();
    return test_finish("test_map");
}
