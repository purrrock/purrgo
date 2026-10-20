/*
 * Host-тесты парсера и рендерера карты (TODO_refact.md, раздел 5, «Тесты»):
 *   - корректная карта Map Format V3 в памяти рисуется;
 *   - повреждённые заголовки PGO/SQT, геометрия MLP, узлы навигации (глубина
 *     рекурсии, v3_jump) отвергаются без падений и без выхода за границы;
 *   - «фаззинг»: усечение файла в каждой точке и подмена каждого байта .idx/.mlp/.db
 *     (проверяется ASan/UBSan: любое чтение за границей - ошибка теста).
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/map.h"
#include "purrgo/config.h"
#include "purrgo/fs_hal.h"
#include "fake_fs.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

/* ------------------------------------------------------- память как ФС --- */

typedef struct { const uint8_t *data; uint32_t size, pos; } memfile_t;

static uint32_t mem_read(void *h, void *buf, uint32_t n)
{
    memfile_t *f = h;
    if (f->pos >= f->size) { return 0; }
    if (n > f->size - f->pos) { n = f->size - f->pos; }
    memcpy(buf, f->data + f->pos, n);
    f->pos += n;
    return n;
}

/* Как FatFs в режиме чтения: позиция за концом файла ограничивается его размером. */
static bool mem_seek(void *h, uint32_t off)
{
    memfile_t *f = h;
    f->pos = off > f->size ? f->size : off;
    return true;
}

static void mem_open(purrgo_fs_t *fs, memfile_t *f, const uint8_t *d, uint32_t n)
{
    f->data = d; f->size = n; f->pos = 0;
    fs->handle = f; fs->read = mem_read; fs->seek = mem_seek;
}

/* ----------------------------------------------------------- графика --- */

#define SCR_W 64
#define SCR_H 64
static uint8_t g_fb[SCR_W * SCR_H];
static long g_pixels;

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
static const purrgo_bbox_t CAM = { 0, 0, 1000, 1000 };
static const purrgo_viewport_t VP = { SCR_W, SCR_H, 0, 0 };

static void gfx_reset(void)
{
    memset(g_fb, 0, sizeof g_fb);
    g_pixels = 0;
    (void)gfx_init(&g_gfx, SCR_W, SCR_H, g_fb, cb_draw, cb_read);
    gfx_set_color(&g_gfx, 1, 3);
}

/* ------------------------------------------- построитель карты V3 в RAM --- */

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

#define MAXF 32768
typedef struct { uint8_t d[MAXF]; uint32_t n; } blob_t;

static void pgo_header(uint8_t *h, uint8_t type, uint32_t payload, uint32_t l0, uint32_t l1, uint32_t l2)
{
    memset(h, 0, 32);
    h[0] = 'P'; h[1] = 'G'; h[2] = 'O'; h[3] = type;
    put_u32(h + 4, payload); put_u32(h + 8, l0); put_u32(h + 12, l1); put_u32(h + 16, l2);
}

/* Узел данных (25 байт): bbox, тип объекта, v1 (смещение геометрии), v2 (индекс в .db). */
static void data_node(uint8_t *p, int32_t xmin, int32_t ymin, int32_t xmax, int32_t ymax,
                      uint8_t type, uint32_t v1, uint32_t v2)
{
    put_u32(p, (uint32_t)xmin); put_u32(p + 4, (uint32_t)ymin);
    put_u32(p + 8, (uint32_t)xmax); put_u32(p + 12, (uint32_t)ymax);
    p[16] = type; put_u32(p + 17, v1); put_u32(p + 21, v2);
}

/* Навигационный узел (28 байт): jump, bbox, уровень, число потомков. */
static void nav_node(uint8_t *p, uint32_t jump, int32_t xmin, int32_t ymin, int32_t xmax, int32_t ymax,
                     uint32_t level, uint32_t count)
{
    put_u32(p, jump);
    put_u32(p + 4, (uint32_t)xmin); put_u32(p + 8, (uint32_t)ymin);
    put_u32(p + 12, (uint32_t)xmax); put_u32(p + 16, (uint32_t)ymax);
    put_u32(p + 20, level); put_u32(p + 24, count);
}

/* IDX из трёх одинаковых LOD: [SQT-заголовок 16][узлы]. mode > 0 - корень навигационный. */
static void build_idx(blob_t *o, const uint8_t *nodes, uint32_t nodes_len, uint32_t mode, uint32_t count)
{
    uint32_t lod = 16u + nodes_len;
    memset(o, 0, sizeof *o);
    pgo_header(o->d, 1, 3u * lod, 32u, 32u + lod, 32u + 2u * lod);
    for (int i = 0; i < 3; i++) {
        uint8_t *p = o->d + 32u + (uint32_t)i * lod;
        p[0] = 'S'; p[1] = 'Q'; p[2] = 'T'; p[3] = 1;
        put_u32(p + 8, mode); put_u32(p + 12, count);
        memcpy(p + 16, nodes, nodes_len);
    }
    o->n = 32u + 3u * lod;
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
    pgo_header(o->d, 2, pos - 32u, 0, 0, 0);
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
    build_idx(idx, node, sizeof node, 0, 1);
    uint32_t parts[1] = { 0 };
    int32_t xy[4] = { 100, 100, 900, 900 };
    build_mlp(mlp, 1, parts, 2, xy);
}

static long render(const blob_t *idx, const blob_t *mlp, const blob_t *db, purrgo_map_layer_t layer)
{
    purrgo_fs_t ifs, mfs, dfs;
    memfile_t fi, fm, fd;
    mem_open(&ifs, &fi, idx->d, idx->n);
    mem_open(&mfs, &fm, mlp ? mlp->d : (const uint8_t *)"", mlp ? mlp->n : 0);
    if (db) { mem_open(&dfs, &fd, db->d, db->n); }
    gfx_reset();
    purrgo_map_render_layer(&ifs, mlp ? &mfs : NULL, db ? &dfs : NULL, &g_gfx, &CAM, &VP, layer);
    return g_pixels;
}

static void defaults(void)
{
    memset(&app_config, 0, sizeof app_config);
    app_config.map_details = PURRGO_MAP_DETAILS_LOW;
    app_config.poi_mode = PURRGO_POI_MODE_CIRCLES;
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
    data_node(node, 100, 100, 900, 900, 10 /* WATER */, GEOM_V1, 0);
    build_idx(&idx, node, sizeof node, 0, 1);
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
    build_idx(&idx, node, sizeof node, 0, 1);
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
    CHECK(ok > 0);

    bad = idx; bad.d[0] = 'X';                       /* магия */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0);
    bad = idx; bad.d[3] = 2;                         /* тип файла: ожидается IDX */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0);
    bad = idx; bad.d[3] = 0;
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0);
    bad = idx; put_u32(bad.d + 4, 0xFFFFFFFFu);      /* размер payload: переполнение */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0);
    bad = idx; put_u32(bad.d + 4, idx.n);            /* payload больше файла */
    (void)render(&bad, &mlp, NULL, MAP_LAYER_LINES);
    bad = idx; put_u32(bad.d + 8, 0);                /* LOD0 раньше payload */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0);
    bad = idx; put_u32(bad.d + 12, 32);              /* LOD1 == LOD0 (не по возрастанию) */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0);
    bad = idx; put_u32(bad.d + 16, 0xFFFFFFF0u);     /* LOD2 за пределами */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0);

    bad = mlp; bad.d[1] = 'Z';                       /* MLP: плохая магия */
    CHECK(render(&idx, &bad, NULL, MAP_LAYER_LINES) == 0);
    bad = mlp; bad.d[3] = 1;                         /* MLP: тип IDX вместо MLP */
    CHECK(render(&idx, &bad, NULL, MAP_LAYER_LINES) == 0);

    bad = idx; bad.d[32] = 'Q';                      /* SQT: плохая магия */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0);
    bad = idx; bad.d[35] = 2;                        /* SQT: версия */
    CHECK(render(&bad, &mlp, NULL, MAP_LAYER_LINES) == 0);
    bad = idx; put_u32(bad.d + 32 + 12, 1000000u);   /* count больше реального числа узлов */
    (void)render(&bad, &mlp, NULL, MAP_LAYER_LINES);
}

static void test_bad_mlp_geometry(void)
{
    static blob_t idx, mlp, tmp;
    uint8_t node[25];
    data_node(node, 100, 100, 900, 900, 1, GEOM_V1, 0);
    build_idx(&idx, node, sizeof node, 0, 1);

    uint32_t p1[1] = { 0 };
    int32_t xy[8] = { 100, 100, 900, 900, 100, 900, 900, 100 };

    build_mlp(&mlp, 1, p1, 4, xy);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) > 0);                 /* эталон */

    build_mlp_ex(&tmp, 1, p1, 1, 0, xy, 4);                               /* 0 точек */
    CHECK(render(&idx, &tmp, NULL, MAP_LAYER_LINES) == 0);
    build_mlp_ex(&tmp, 1, p1, 1, -5, xy, 4);                              /* отрицательное число точек */
    CHECK(render(&idx, &tmp, NULL, MAP_LAYER_LINES) == 0);
    build_mlp_ex(&tmp, 1, p1, 1, 513, xy, 4);                             /* больше PURRGO_MAP_MAX_POINTS */
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
    build_idx(&idx, node, sizeof node, 0, 1);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) == 0);
    data_node(node, 100, 100, 900, 900, 1, 0xFFFFFFF0u, 0);
    build_idx(&idx, node, sizeof node, 0, 1);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) == 0);

    /* Усечённая геометрия: точек заявлено больше, чем есть в файле */
    build_mlp(&tmp, 1, p1, 4, xy);
    tmp.n -= 12;
    data_node(node, 100, 100, 900, 900, 1, GEOM_V1, 0);
    build_idx(&idx, node, sizeof node, 0, 1);
    (void)render(&idx, &tmp, NULL, MAP_LAYER_LINES);
}

/* Цепочка навигационных узлов глубины depth, в конце - один узел данных. */
static void build_nav_chain(blob_t *idx, blob_t *mlp, int depth)
{
    static uint8_t nodes[MAXF];
    uint32_t n = 0;
    for (int level = depth - 1; level >= 0; level--) {
        /* jump = размер всего поддерева за этим узлом */
        uint32_t below = (uint32_t)level * 28u + 25u;
        nav_node(nodes + n, below, 0, 0, 1000, 1000, (uint32_t)level, 1);
        n += 28;
    }
    data_node(nodes + n, 100, 100, 900, 900, 1, GEOM_V1, 0);
    n += 25;
    build_idx(idx, nodes, n, 1, 1);
    uint32_t parts[1] = { 0 };
    int32_t xy[4] = { 100, 100, 900, 900 };
    build_mlp(mlp, 1, parts, 2, xy);
}

static void test_nav_tree_depth_limit(void)
{
    static blob_t idx, mlp;

    build_nav_chain(&idx, &mlp, 5);                  /* нормальная глубина */
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) > 0);
    build_nav_chain(&idx, &mlp, 31);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) > 0);
    build_nav_chain(&idx, &mlp, 200);                /* патч 0004: глубже PURRGO_MAP_MAX_TREE_DEPTH (32) */
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) == 0);
    build_nav_chain(&idx, &mlp, 280);                /* максимум для MAXF */
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) == 0);
}

static void test_v3_jump_bounds(void)
{
    static blob_t idx, mlp;
    uint8_t nodes[28 + 25];

    /* Навигационный узел ВНЕ камеры с огромным jump: должен быть отвергнут, а не выполнен. */
    nav_node(nodes, 0xFFFFFFF0u, 5000, 5000, 6000, 6000, 0, 1);
    data_node(nodes + 28, 100, 100, 900, 900, 1, GEOM_V1, 0);
    build_idx(&idx, nodes, sizeof nodes, 1, 1);
    uint32_t parts[1] = { 0 };
    int32_t xy[4] = { 100, 100, 900, 900 };
    build_mlp(&mlp, 1, parts, 2, xy);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) == 0);

    /* jump выходит за границу LOD */
    nav_node(nodes, 100000u, 5000, 5000, 6000, 6000, 0, 1);
    build_idx(&idx, nodes, sizeof nodes, 1, 1);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) == 0);

    /* Корректный jump пропускает поддерево ровно на его размер: следующий узел виден. */
    uint8_t two[28 + 25 + 25];
    nav_node(two, 25u, 5000, 5000, 6000, 6000, 0, 1);        /* вне камеры, поддерево = 1 узел данных */
    data_node(two + 28, 100, 100, 900, 900, 1, GEOM_V1, 0);  /* пропускается */
    data_node(two + 53, 100, 900, 900, 100 + 800, 1, GEOM_V1, 0);
    build_idx(&idx, two, sizeof two, 1, 2);
    /* root в режиме nav: оба элемента трактуются как nav-узлы - структура намеренно
       нетипична, достаточно, чтобы обход завершился без выхода за границы */
    (void)render(&idx, &mlp, NULL, MAP_LAYER_LINES);
}

static void test_poi_layer(void)
{
    static blob_t idx;
    uint8_t node[25];
    data_node(node, 500, 500, 500, 500, 11 /* POI_BIG */, 0, 0);
    build_idx(&idx, node, sizeof node, 0, 1);

    defaults();
    app_config.poi_mode = PURRGO_POI_MODE_NO;
    CHECK(render(&idx, NULL, NULL, MAP_LAYER_POIS) == 0);
    app_config.poi_mode = PURRGO_POI_MODE_CIRCLES;
    CHECK(render(&idx, NULL, NULL, MAP_LAYER_POIS) > 5);
}

/* Все сочетания «подмена одного байта» для .idx, .mlp и .db. */
static void test_fuzz_truncate_and_flip(void)
{
    static blob_t idx, mlp, db, m;
    simple_map(&idx, &mlp, 1);

    /* .db с двумя записями (129 + 117 * n байт заголовка/записей после 32-байтного PGO). */
    memset(&db, 0, sizeof db);
    pgo_header(db.d, 3, 129u + 117u * 3u, 0, 0, 0);
    memcpy(db.d + 32u + 129u + 117u * 1u + 17u, "Lake", 5);
    memcpy(db.d + 32u + 129u + 117u * 2u + 17u, "Forest", 7);
    db.n = 32u + 129u + 117u * 3u;

    defaults();
    app_config.layer_poi_labels = true;
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

    /* Подмена каждого байта несколькими значениями. */
    static const uint8_t vals[] = { 0x00, 0x01, 0x7F, 0x80, 0xFF };
    for (uint32_t i = 0; i < idx.n; i++) {
        for (size_t v = 0; v < sizeof vals; v++) {
            m = idx; m.d[i] = vals[v];
            (void)render(&m, &mlp, &db, MAP_LAYER_LINES);
            (void)render(&m, &mlp, &db, MAP_LAYER_POLYGONS);
            (void)render(&m, NULL, &db, MAP_LAYER_POIS);
        }
    }
    for (uint32_t i = 0; i < mlp.n; i++) {
        for (size_t v = 0; v < sizeof vals; v++) {
            m = mlp; m.d[i] = vals[v];
            (void)render(&idx, &m, &db, MAP_LAYER_LINES);
            (void)render(&idx, &m, &db, MAP_LAYER_POLYGONS);
        }
    }
    CHECK(1);   /* цель - отсутствие срабатываний ASan/UBSan */
}

/* Полный путь purrgo_map_render_viewport через (фейковую) ФС. */
static void test_viewport_via_fs(void)
{
    static blob_t idx, mlp;
    simple_map(&idx, &mlp, 1);

    fake_fs_reset();
    defaults();
    app_config.layer_roads = true;
    purrgo_map_shutdown();
    fake_fs_put_bin("maps/m1/roads.idx", idx.d, idx.n);
    fake_fs_put_bin("maps/m1/roads.mlp", mlp.d, mlp.n);

    gfx_reset();
    bool ok = purrgo_map_render_viewport(&g_gfx, &VP, &CAM, "maps/m1");
    CHECK(ok);
    CHECK(g_pixels > 20);
    purrgo_map_shutdown();
    CHECK(fake_fs_open_handles() == 0);                /* shutdown закрывает все файлы */

    /* Повреждённый .idx в файле: кадр без падения, пикселей нет. */
    idx.d[0] = 0;
    fake_fs_put_bin("maps/m2/roads.idx", idx.d, idx.n);
    fake_fs_put_bin("maps/m2/roads.mlp", mlp.d, mlp.n);
    gfx_reset();
    (void)purrgo_map_render_viewport(&g_gfx, &VP, &CAM, "maps/m2");
    CHECK(g_pixels == 0);
    purrgo_map_shutdown();
    CHECK(fake_fs_open_handles() == 0);
}

/* Раздел 2 (карта): повторные открытия, признак успеха кадра, длинный map_dir. */
static void test_viewport_failures(void)
{
    static blob_t idx, mlp;

    /* Слой включён, файлов нет: один проход открытий, дальше без обращений к ФС. */
    fake_fs_reset();
    defaults();
    app_config.layer_landuse = true;
    purrgo_map_shutdown();
    gfx_reset();
    bool ok = purrgo_map_render_viewport(&g_gfx, &VP, &CAM, "maps/none");
    CHECK(ok);                                  /* отсутствие файлов - не ошибка отрисовки */
    int after_first = fake_fs_open_calls();
    CHECK(after_first > 0);
    (void)purrgo_map_render_viewport(&g_gfx, &VP, &CAM, "maps/none");
    CHECK(fake_fs_open_calls() == after_first);

    /* Выключение и включение слоя - новая попытка (файл мог появиться). */
    simple_map(&idx, &mlp, 1);
    fake_fs_put_bin("maps/none/landuse.idx", idx.d, idx.n);
    fake_fs_put_bin("maps/none/landuse.mlp", mlp.d, mlp.n);
    (void)purrgo_map_render_viewport(&g_gfx, &VP, &CAM, "maps/none");
    CHECK(fake_fs_open_calls() == after_first);  /* память о неудаче сохраняется */
    app_config.layer_landuse = false;
    (void)purrgo_map_render_viewport(&g_gfx, &VP, &CAM, "maps/none");
    app_config.layer_landuse = true;
    gfx_reset();
    ok = purrgo_map_render_viewport(&g_gfx, &VP, &CAM, "maps/none");
    CHECK(ok);
    CHECK(fake_fs_open_calls() > after_first);     /* файлы открыты заново */
    purrgo_map_shutdown();

    /* Смена каталога сбрасывает память о неудаче. */
    fake_fs_reset();
    defaults();
    app_config.layer_roads = true;
    (void)purrgo_map_render_viewport(&g_gfx, &VP, &CAM, "maps/a");
    int calls = fake_fs_open_calls();
    (void)purrgo_map_render_viewport(&g_gfx, &VP, &CAM, "maps/b");
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
    app_config.layer_roads = true;
    purrgo_map_shutdown();
    gfx_reset();
    CHECK(purrgo_map_render_viewport(&g_gfx, &VP, &CAM, "maps/good"));
    CHECK(!purrgo_map_render_viewport(&g_gfx, &VP, &CAM, "maps/bad"));
    purrgo_map_shutdown();
    CHECK(fake_fs_open_handles() == 0);

    /* Слой POI: отсутствие pois.idx - не ошибка, битый pois.idx - ошибка. */
    fake_fs_reset();
    defaults();
    app_config.layer_poi = true;
    CHECK(purrgo_map_render_viewport(&g_gfx, &VP, &CAM, "maps/nopoi"));
    fake_fs_put_bin("maps/badpoi/pois.idx", idx.d, idx.n);   /* idx.d[0] == 0 */
    CHECK(!purrgo_map_render_viewport(&g_gfx, &VP, &CAM, "maps/badpoi"));
    purrgo_map_shutdown();

    /* Слишком длинный map_dir: без обрезки и без открытий, кадр помечается неудачным. */
    fake_fs_reset();
    defaults();
    app_config.layer_roads = true;
    char longdir[300];
    memset(longdir, 'd', sizeof longdir - 1);
    longdir[sizeof longdir - 1] = '\0';
    purrgo_map_shutdown();
    CHECK(!purrgo_map_render_viewport(&g_gfx, &VP, &CAM, longdir));
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
    CHECK(purrgo_map_render_viewport(&g_gfx, &VP, &CAM, d1));
    CHECK(g_pixels > 0);
    gfx_reset();
    (void)purrgo_map_render_viewport(&g_gfx, &VP, &CAM, d2);   /* файлов нет -> пусто */
    CHECK(g_pixels == 0);
    purrgo_map_shutdown();
    CHECK(fake_fs_open_handles() == 0);
}

int main(void)
{
    defaults();
    test_valid_line();
    test_valid_polygon();
    test_outside_camera_not_drawn();
    test_bad_pgo_header();
    test_bad_mlp_geometry();
    test_nav_tree_depth_limit();
    test_v3_jump_bounds();
    test_poi_layer();
    test_fuzz_truncate_and_flip();
    test_viewport_via_fs();
    test_viewport_failures();
    return test_finish("test_map");
}
