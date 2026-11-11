/*
 * Host-тесты парсера и рендерера карты (патчи 0004, 0009, 0013):
 *   - корректная карта Map Format V3 в памяти рисуется;
 *   - повреждённые заголовки PGO/SQT, геометрия MLP, узлы навигации (глубина
 *     рекурсии, v3_jump) отвергаются без падений и без выхода за границы;
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
#include "map_internal.h"
#include "map_culling.h"
#include "fake_fs.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

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
    /* Слой кладётся в фейковую ФС и открывается как обычный файл карты. */
    fake_fs_reset();
    fake_fs_put_bin("l.idx", idx->d, idx->n);
    if (mlp) { fake_fs_put_bin("l.mlp", mlp->d, mlp->n); }
    if (db)  { fake_fs_put_bin("l.db", db->d, db->n); }
    purrgo_file_t *ifs = purrgo_fs_open("l.idx", FS_READ);
    purrgo_file_t *mfs = mlp ? purrgo_fs_open("l.mlp", FS_READ) : NULL;
    purrgo_file_t *dfs = db ? purrgo_fs_open("l.db", FS_READ) : NULL;
    gfx_reset();
    purrgo_map_render_layer(ifs, mfs, dfs, &g_gfx, &CAM, &VP, &g_params, layer);
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
    data_node(node, 500, 500, 500, 500, PURRGO_FEATURE_POI_BIG, 0, 0);
    build_idx(&idx, node, sizeof node, 0, 1);

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
    pgo_header(db.d, 3, 129u + 117u * 3u, 0, 0, 0);
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
    build_idx(&idx, node, sizeof node, 0, 1);
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
    build_idx(idx, node, sizeof node, 0, 1);
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
    build_idx(&idx, node, sizeof node, 0, 1);
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
    build_idx(&idx, node, sizeof node, 0, 1);
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
    build_idx(&idx, node, sizeof node, 0, 1);
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
    build_idx(&widx, node, sizeof node, 0, 1);
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
    build_idx(&idx, node, sizeof node, 0, 1);
    build_mlp(&mlp, 1, parts, 2, tiny_line);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) == 0);

    int32_t line[4] = { 500, 500, 540, 540 };
    data_node(node, 500, 500, 540, 540, 1, GEOM_V1, 0);
    build_idx(&idx, node, sizeof node, 0, 1);
    build_mlp(&mlp, 1, parts, 2, line);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_LINES) > 0);

    /* Полигон 2.6 px (< 3) пропускается, 3.8 px рисуется. */
    int32_t small_poly[8] = { 500, 500, 540, 500, 540, 540, 500, 540 };
    data_node(node, 500, 500, 540, 540, PURRGO_FEATURE_WATER, GEOM_V1, 0);
    build_idx(&idx, node, sizeof node, 0, 1);
    build_mlp(&mlp, 1, parts, 4, small_poly);
    CHECK(render(&idx, &mlp, NULL, MAP_LAYER_POLYGONS) == 0);

    int32_t poly[8] = { 500, 500, 560, 500, 560, 560, 500, 560 };
    data_node(node, 500, 500, 560, 560, PURRGO_FEATURE_WATER, GEOM_V1, 0);
    build_idx(&idx, node, sizeof node, 0, 1);
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
    CHECK(fake_fs_read_ahead("maps/ra/roads.mlp") == PURRGO_MAP_MLP_READ_AHEAD_SECTORS);
    CHECK(fake_fs_read_ahead("maps/ra/roads.idx") == 0);                /* для .idx не задаётся */
    purrgo_map_shutdown();

    uint8_t node[25];
    data_node(node, 500, 500, 500, 500, PURRGO_FEATURE_POI_BIG, 0, 0);
    build_idx(&poi, node, sizeof node, 0, 1);

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
 * Диапазоны feature code по слоям (docs/map-format-v3.md, 8.1) и таблица иконок:
 * номер иконки POI равен feature code, а вне диапазона POI занят только UI-иконками.
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
        const int is_ui = (no == 35u || no == 46u || no == 47u);
        if (!in_range(no, PURRGO_FEATURE_POI_FIRST, PURRGO_FEATURE_POI_LAST) && !is_ui) {
            CHECK(icon_is_empty(no));
        }
    }
    CHECK(!icon_is_empty(PURRGO_FEATURE_POI_BIG));
    CHECK(!icon_is_empty(PURRGO_FEATURE_POI_SMALL));
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
