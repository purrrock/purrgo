/*
 * Host-тесты пользовательского маршрута:
 *   - разбор GPX (trkpt/rtept, порядок и кавычки атрибутов, разрывы сегментов,
 *     произвольные границы фрагментов, мусор);
 *   - прореживание при переполнении массива;
 *   - загрузка из каталога ROUTES и purrgo_route_sync();
 *   - отрисовка (чёрная линия 3 px с белым центром, отсечение вне камеры);
 *   - ключ ROUTE_FILE в PURRGO.CFG;
 *   - выбор файла в меню (страницы, "(none)", включение слоя Route).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include "purrgo/menu_controller.h"
#include "purrgo/gfx_renderer.h"
#include "purrgo/route.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

/* ----------------------------------------------------------- холст --- */

#define W 64
#define H 48
static uint8_t g_fb[W * H];
static gfx_context_t g_gfx;

static void cb_draw(void *fb, int16_t x, int16_t y, gfx_color_t c)
{
    if (x >= 0 && x < W && y >= 0 && y < H) { ((uint8_t *)fb)[y * W + x] = (uint8_t)c; }
}
static gfx_color_t cb_read(void *fb, int16_t x, int16_t y)
{
    if (x >= 0 && x < W && y >= 0 && y < H) { return ((uint8_t *)fb)[y * W + x]; }
    return 0;
}

static void canvas(void)
{
    memset(g_fb, WHITE, sizeof g_fb);
    (void)gfx_init(&g_gfx, W, H, g_fb, cb_draw, cb_read);
    gfx_set_color(&g_gfx, BLACK, WHITE);
}

static int count_px(uint8_t color)
{
    int n = 0;
    for (int i = 0; i < W * H; i++) { if (g_fb[i] == color) { n++; } }
    return n;
}

/* ---------------------------------------------------------- помощники --- */

static size_t points(void)    /* записи без разрывов */
{
    size_t n = 0;
    purrgo_route_point_t p;
    for (size_t i = 0; purrgo_route_get_entry(i, &p); i++) {
        if (p.lat_1e7 != PURRGO_ROUTE_BREAK_LAT) { n++; }
    }
    return n;
}

static size_t breaks(void)
{
    size_t n = 0;
    purrgo_route_point_t p;
    for (size_t i = 0; purrgo_route_get_entry(i, &p); i++) {
        if (p.lat_1e7 == PURRGO_ROUTE_BREAK_LAT) { n++; }
    }
    return n;
}

static void parse_whole(const char *text)
{
    purrgo_route_begin();
    purrgo_route_feed(text, strlen(text));
    purrgo_route_end();
}

static const char *GPX_BASIC =
    "<?xml version=\"1.0\"?>\n"
    "<gpx version=\"1.1\" creator=\"x\">\n"
    " <wpt lat=\"1.0\" lon=\"1.0\"><name>ignored</name></wpt>\n"
    " <trk><name>t</name>\n"
    "  <trkseg>\n"
    "   <trkpt lat=\"53.1000000\" lon=\"28.2000000\"><ele>1</ele></trkpt>\n"
    "   <trkpt lon='28.2010000' lat='53.1010000'/>\n"
    "   <trkpt\n      lat=\"-0.5\"\n      lon=\"-179.9999999\">\n</trkpt>\n"
    "  </trkseg>\n"
    "  <trkseg>\n"
    "   <trkpt lat=\"53.2\" lon=\"28.3\"/>\n"
    "   <trkpt lat=\"53.2005\" lon=\"28.3005\"/>\n"
    "  </trkseg>\n"
    " </trk>\n"
    " <rte><rtept lat=\"10\" lon=\"20\"/><rtept lat=\"10.5\" lon=\"20.5\"/></rte>\n"
    "</gpx>\n";

/* ---------------------------------------------------------------- тесты --- */

static void test_parser(void)
{
    purrgo_route_point_t p;

    parse_whole(GPX_BASIC);
    /* wpt не входит; 3 + 2 точки трека и 2 точки маршрута; разрывы: между seg1/seg2, trk/rte */
    CHECK(points() == 7);
    CHECK(breaks() == 2);
    CHECK(purrgo_route_get_entry(0, &p) && p.lat_1e7 == 531000000 && p.lon_1e7 == 282000000);
    CHECK(purrgo_route_get_entry(1, &p) && p.lat_1e7 == 531010000 && p.lon_1e7 == 282010000);   /* кавычки ' и порядок lon/lat */
    CHECK(purrgo_route_get_entry(2, &p) && p.lat_1e7 == -5000000 && p.lon_1e7 == -1799999999);  /* знак, перенос строк в теге */
    CHECK(purrgo_route_get_entry(3, &p) && p.lat_1e7 == PURRGO_ROUTE_BREAK_LAT);
    CHECK(purrgo_route_get_entry(4, &p) && p.lat_1e7 == 532000000 && p.lon_1e7 == 283000000);
    CHECK(purrgo_route_get_entry(5, &p) && p.lat_1e7 == 532005000 && p.lon_1e7 == 283005000);
    CHECK(purrgo_route_get_entry(6, &p) && p.lat_1e7 == PURRGO_ROUTE_BREAK_LAT);
    CHECK(purrgo_route_get_entry(7, &p) && p.lat_1e7 == 100000000 && p.lon_1e7 == 200000000);
    CHECK(purrgo_route_get_entry(8, &p) && p.lat_1e7 == 105000000 && p.lon_1e7 == 205000000);
    CHECK(!purrgo_route_get_entry(9, &p));          /* хвостовой разрыв убран */
    CHECK(purrgo_route_entry_count() == 9);

    /* Те же данные байт за байтом (граница фрагмента внутри тега и числа). */
    size_t n_all = purrgo_route_entry_count();
    purrgo_route_point_t first[9];
    for (size_t i = 0; i < n_all; i++) { (void)purrgo_route_get_entry(i, &first[i]); }

    purrgo_route_begin();
    for (const char *c = GPX_BASIC; *c; c++) { purrgo_route_feed(c, 1); }
    purrgo_route_end();
    CHECK(purrgo_route_entry_count() == n_all);
    CHECK(n_all == 9);
    for (size_t i = 0; i < n_all && i < 9; i++) {
        CHECK(purrgo_route_get_entry(i, &p) && memcmp(&p, &first[i], sizeof p) == 0);
    }

    /* Мусор: нет атрибутов, не число, вне диапазона, широта > 90, пустой ввод. */
    parse_whole("<gpx><trkseg>"
                "<trkpt lat=\"53\"/>"
                "<trkpt lat=\"abc\" lon=\"1\"/>"
                "<trkpt lat=\"91\" lon=\"1\"/>"
                "<trkpt lat=\"1\" lon=\"181\"/>"
                "<trkpt lat=\"1\" lon=\"1\"/>"
                "<trkptx lat=\"2\" lon=\"2\"/>"
                "</trkseg></gpx>");
    CHECK(points() == 1);
    parse_whole("");
    CHECK(purrgo_route_entry_count() == 0);
    parse_whole("<trkpt lat=\"1\" lon=\"1\"/>");   /* без сегмента и без закрытия */
    CHECK(points() == 1);

    /* Повторная точка подряд не дублируется. */
    parse_whole("<trkpt lat=\"1\" lon=\"1\"/><trkpt lat=\"1\" lon=\"1\"/><trkpt lat=\"1\" lon=\"2\"/>");
    CHECK(points() == 2);

    /* Первые 7 знаков после запятой, остальные отбрасываются; 0 и граничные значения. */
    parse_whole("<trkpt lat=\"0\" lon=\"0\"/><trkpt lat=\"90\" lon=\"180\"/>"
                "<trkpt lat=\"53.123456789\" lon=\"-.5\"/>");
    CHECK(purrgo_route_get_entry(0, &p) && p.lat_1e7 == 0 && p.lon_1e7 == 0);
    CHECK(purrgo_route_get_entry(1, &p) && p.lat_1e7 == 900000000 && p.lon_1e7 == 1800000000);
    CHECK(purrgo_route_get_entry(2, &p) && p.lat_1e7 == 531234567 && p.lon_1e7 == -5000000);
}

static void test_decimation(void)
{
    /* 3 сегмента по 5000 плотных точек (шаг ~1 м) - больше вместимости. */
    const size_t per_seg = 5000;
    const int32_t lat0[3] = { 530000000, 531000000, 532000000 };
    char tag[96];

    purrgo_route_begin();
    purrgo_route_feed("<trk>", 5);
    for (int s = 0; s < 3; s++) {
        purrgo_route_feed("<trkseg>", 8);
        for (size_t i = 0; i < per_seg; i++) {
            int n = snprintf(tag, sizeof tag, "<trkpt lat=\"%d.%07d\" lon=\"%d.%07d\"/>",
                             (int)(lat0[s] / 10000000), (int)(lat0[s] % 10000000),
                             28, (int)(i * 10 + 1));
            purrgo_route_feed(tag, (size_t)n);
        }
        purrgo_route_feed("</trkseg>", 9);
    }
    purrgo_route_feed("</trk>", 6);
    purrgo_route_end();

    CHECK(purrgo_route_entry_count() <= PURRGO_ROUTE_MAX_POINTS);
    CHECK(purrgo_route_entry_count() > 100);
    CHECK(purrgo_route_decimation_step() > 0);
    CHECK(breaks() == 2);

    /* Начало и конец каждого сегмента остались. */
    purrgo_route_point_t p;
    size_t seg = 0, idx = 0;
    bool seg_first_ok = true, seg_last_ok = true;
    int32_t prev_lon = 0;
    bool have_prev = false;
    for (idx = 0; purrgo_route_get_entry(idx, &p); idx++) {
        if (p.lat_1e7 == PURRGO_ROUTE_BREAK_LAT) {
            seg_last_ok = seg_last_ok && have_prev && prev_lon == (int32_t)((per_seg - 1) * 10 + 1);
            have_prev = false;
            seg++;
            continue;
        }
        if (!have_prev) { seg_first_ok = seg_first_ok && p.lon_1e7 == 280000001; }
        have_prev = true;
        prev_lon = p.lon_1e7 - 280000000;
    }
    seg_last_ok = seg_last_ok && have_prev && prev_lon == (int32_t)((per_seg - 1) * 10 + 1);
    CHECK(seg == 2);
    CHECK(seg_first_ok);
    CHECK(seg_last_ok);
}

static const char *ROUTES_DIR = "routes";

static void put_route(const char *name, const char *text)
{
    char path[160];
    snprintf(path, sizeof path, "%s/%s", ROUTES_DIR, name);
    fake_fs_put(path, text);
}

static void test_load_and_sync(void)
{
    fake_fs_reset();
    put_route("a.gpx", GPX_BASIC);
    put_route("one.gpx", "<trkpt lat=\"1\" lon=\"1\"/>");   /* нет ни одного отрезка */

    CHECK(purrgo_route_load("a.gpx"));
    CHECK(points() == 7);
    CHECK(fake_fs_open_handles() == 0);

    CHECK(!purrgo_route_load("one.gpx"));
    CHECK(!purrgo_route_load("missing.gpx"));
    CHECK(purrgo_route_entry_count() == 0);
    CHECK(!purrgo_route_load("../a.gpx"));
    CHECK(!purrgo_route_load("sub/a.gpx"));
    CHECK(!purrgo_route_load(".hidden.gpx"));
    CHECK(!purrgo_route_load(""));
    CHECK(fake_fs_open_handles() == 0);

    /* sync: то же имя - без повторного чтения файла */
    purrgo_route_clear();
    fake_fs_reset();
    put_route("a.gpx", GPX_BASIC);
    CHECK(purrgo_route_sync("") == false);
    CHECK(purrgo_route_sync("a.gpx"));
    const int opens = fake_fs_open_calls();
    CHECK(opens == 1);
    CHECK(purrgo_route_sync("a.gpx"));
    CHECK(fake_fs_open_calls() == opens);

    /* пустое имя убирает маршрут из памяти, повторная установка читает файл заново */
    CHECK(!purrgo_route_sync(""));
    CHECK(purrgo_route_entry_count() == 0);
    CHECK(purrgo_route_sync("a.gpx"));
    CHECK(fake_fs_open_calls() == opens + 1);

    /* ошибка загрузки (SD не готова) повторяется на следующем вызове */
    fake_fs_reset();
    CHECK(!purrgo_route_sync("late.gpx"));
    CHECK(!purrgo_route_sync("late.gpx"));
    CHECK(fake_fs_open_calls() == 2);
    put_route("late.gpx", GPX_BASIC);
    CHECK(purrgo_route_sync("late.gpx"));
    CHECK(points() == 7);

    /* смена имени */
    put_route("b.gpx", "<trkpt lat=\"5\" lon=\"5\"/><trkpt lat=\"6\" lon=\"6\"/>");
    CHECK(purrgo_route_sync("b.gpx"));
    CHECK(points() == 2);
    purrgo_route_clear();
    CHECK(purrgo_route_sync("") == false);
}

/* Камера 64x48 px: 1 px = 1000 единиц (1e-7 градуса); lat 53.0..53.0048, lon 28.0..28.0064. */
static void camera_viewport(purrgo_bbox_t *cam, purrgo_viewport_t *vp)
{
    cam->min_x = 280000000; cam->max_x = 280064000;
    cam->min_y = 530000000; cam->max_y = 530048000;
    vp->width = W; vp->height = H; vp->offset_x = 0; vp->offset_y = 0;
}

static void test_render(void)
{
    purrgo_bbox_t cam;
    purrgo_viewport_t vp;
    camera_viewport(&cam, &vp);

    /* Горизонтальный отрезок через середину кадра. */
    parse_whole("<trkpt lat=\"53.0024\" lon=\"28.0000\"/><trkpt lat=\"53.0024\" lon=\"28.0064\"/>");
    canvas();
    purrgo_route_render(&g_gfx, &cam, &vp);

    const int x = 32;
    int y_black_first = -1, y_black_last = -1, whites_inside = 0;
    for (int y = 0; y < H; y++) {
        if (cb_read(g_fb, (int16_t)x, (int16_t)y) == BLACK) {
            if (y_black_first < 0) { y_black_first = y; }
            y_black_last = y;
        }
    }
    CHECK(y_black_first >= 0);
    CHECK(y_black_last - y_black_first == 2);       /* 3 строки: чёрная, белая, чёрная */
    for (int y = y_black_first; y <= y_black_last; y++) {
        if (cb_read(g_fb, (int16_t)x, (int16_t)y) == WHITE) { whites_inside++; }
    }
    CHECK(whites_inside == 1);
    CHECK(cb_read(g_fb, (int16_t)x, (int16_t)(y_black_first + 1)) == WHITE);   /* белая линия в центре */
    CHECK(count_px(BLACK) >= 2 * (W - 4));

    /* Разрыв: две отдельные линии не соединяются. */
    parse_whole("<trkseg><trkpt lat=\"53.0010\" lon=\"28.0000\"/><trkpt lat=\"53.0010\" lon=\"28.0020\"/></trkseg>"
                "<trkseg><trkpt lat=\"53.0040\" lon=\"28.0044\"/><trkpt lat=\"53.0040\" lon=\"28.0064\"/></trkseg>");
    canvas();
    purrgo_route_render(&g_gfx, &cam, &vp);
    CHECK(cb_read(g_fb, 32, 10) == WHITE && cb_read(g_fb, 32, 40) == WHITE);   /* между сегментами пусто */
    int black_left = 0, black_right = 0;
    for (int yy = 0; yy < H; yy++) {
        for (int xx = 0; xx < W; xx++) {
            if (cb_read(g_fb, (int16_t)xx, (int16_t)yy) == BLACK) { if (xx < 32) { black_left++; } else { black_right++; } }
        }
    }
    CHECK(black_left > 0 && black_right > 0);

    /* Маршрут целиком вне камеры: ничего не рисуется. */
    parse_whole("<trkpt lat=\"10\" lon=\"10\"/><trkpt lat=\"10.1\" lon=\"10.1\"/>");
    canvas();
    purrgo_route_render(&g_gfx, &cam, &vp);
    CHECK(count_px(BLACK) == 0);

    /* Отрезок, лишь частично попадающий в кадр, и очень далёкая вершина не ломают рисование. */
    parse_whole("<trkpt lat=\"53.0024\" lon=\"28.0032\"/><trkpt lat=\"53.0024\" lon=\"29.9\"/>");
    canvas();
    purrgo_route_render(&g_gfx, &cam, &vp);
    CHECK(count_px(BLACK) > 0);

    /* Пустой маршрут и NULL-аргументы. */
    purrgo_route_clear();
    canvas();
    purrgo_route_render(&g_gfx, &cam, &vp);
    purrgo_route_render(NULL, &cam, &vp);
    purrgo_route_render(&g_gfx, NULL, &vp);
    purrgo_route_render(&g_gfx, &cam, NULL);
    CHECK(count_px(BLACK) == 0);
}

#define CFG "PURRGO.CFG"

static void test_config_key(void)
{
    uint32_t n;
    const uint8_t *d;

    fake_fs_reset();
    purrgo_config_init();
    CHECK(app_config.route_file[0] == '\0');

    snprintf(app_config.route_file, sizeof app_config.route_file, "%s", "Hike 2026.gpx");
    CHECK(purrgo_config_save());
    d = fake_fs_data(CFG, &n);
    CHECK(d != NULL && strstr((const char *)d, "ROUTE_FILE=Hike 2026.gpx\n") != NULL);

    app_config.route_file[0] = '\0';
    CHECK(purrgo_config_load());
    CHECK(strcmp(app_config.route_file, "Hike 2026.gpx") == 0);

    /* Имя с путём или скрытое отбрасывается: остаётся значение по умолчанию (нет маршрута). */
    fake_fs_put(CFG, "ROUTE_FILE=../x.gpx\nROUTE_FILE=a/b.gpx\nROUTE_FILE=.hid.gpx\n");
    CHECK(purrgo_config_load());
    CHECK(app_config.route_file[0] == '\0');

    /* Пустое значение снимает маршрут (с CRLF). */
    fake_fs_put(CFG, "ROUTE_FILE=\r\nTZ_MIN=60\r\n");
    CHECK(purrgo_config_load());
    CHECK(app_config.route_file[0] == '\0');

    /* Слишком длинное имя не помещается и игнорируется. */
    char longname[160];
    memset(longname, 'a', sizeof longname);
    memcpy(longname + 150, ".gpx", 5);
    char cfg[256];
    snprintf(cfg, sizeof cfg, "ROUTE_FILE=%s\n", longname);
    fake_fs_put(CFG, cfg);
    CHECK(purrgo_config_load());
    CHECK(app_config.route_file[0] == '\0');

    /* Худший случай: длинные MAP_DIR и ROUTE_FILE помещаются в буфер сохранения. */
    memset(app_config.map_dir, 'm', sizeof app_config.map_dir - 1);
    app_config.map_dir[sizeof app_config.map_dir - 1] = '\0';
    memset(app_config.route_file, 'r', sizeof app_config.route_file - 1);
    app_config.route_file[sizeof app_config.route_file - 1] = '\0';
    CHECK(purrgo_config_save());
}

/* ---------------------------------------------------------------- меню --- */

static purrgo_state_t press(purrgo_state_t st, purrgo_action_t a)
{
    purrgo_state_t next = st;
    (void)purrgo_menu_controller_handle_button(st, a, &next);
    return next;
}

static const char *cur_name(void)
{
    purrgo_fs_dirent_t *l;
    int c = menu_app_get_dir_list(&l);
    int k = menu_app_get_dir_cursor();
    return (k >= 0 && k < c) ? l[k].name : "<none-cursor>";
}

static void test_route_menu(void)
{
    fake_fs_reset();
    purrgo_config_init();
    purrgo_menu_controller_init();

    /* 12 gpx (в т.ч. заглавное расширение), а также файлы, которые не показываются. */
    char name[32];
    for (int i = 0; i < 12; i++) {
        snprintf(name, sizeof name, "r%02d.%s", i, (i == 5) ? "GPX" : "gpx");
        put_route(name, "<trkpt lat=\"1\" lon=\"1\"/><trkpt lat=\"2\" lon=\"2\"/>");
    }
    put_route("notes.txt", "x");
    put_route("._r00.gpx", "x");
    put_route("gpx", "x");

    /* Пункт ROUTE в меню CONFIG (индекс 6) открывает выбор маршрута. */
    purrgo_state_t st = APP_STATE_MENU_MAIN;
    purrgo_menu_controller_on_enter(APP_STATE_MENU_MAIN);
    for (int i = 0; i < 6; i++) { st = press(st, PURRGO_ACTION_DOWN); }
    CHECK(menu_app_get_main_cursor() == 6);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_ROUTE_SELECT);
    CHECK(fake_fs_open_dirs() == 1);

    /* Первая запись - "(none)", далее файлы страницы: всего 10 записей. */
    purrgo_fs_dirent_t *list;
    CHECK(menu_app_get_dir_list(&list) == 10);
    CHECK(cur_name()[0] == '\0');

    /* Проход вниз: none, r00..r11 строго по порядку (на границе страницы подгружается следующая). */
    for (int k = 1; k <= 12; k++) {
        st = press(st, PURRGO_ACTION_DOWN);
        snprintf(name, sizeof name, "r%02d.%s", k - 1, (k - 1 == 5) ? "GPX" : "gpx");
        CHECK(strcmp(cur_name(), name) == 0);
    }
    CHECK(st == APP_STATE_MENU_ROUTE_SELECT);
    CHECK(menu_app_get_dir_list(&list) == 3);              /* последняя страница: r09..r11 */
    st = press(st, PURRGO_ACTION_DOWN);                      /* дальше конца не уходит */
    CHECK(strcmp(cur_name(), "r11.gpx") == 0);

    /* Вверх обратно до "(none)": страницы сходятся без потерь и дублей. */
    for (int k = 11; k >= 1; k--) {
        st = press(st, PURRGO_ACTION_UP);
        snprintf(name, sizeof name, "r%02d.%s", k - 1, (k - 1 == 5) ? "GPX" : "gpx");
        CHECK(strcmp(cur_name(), name) == 0);
    }
    st = press(st, PURRGO_ACTION_UP);
    CHECK(cur_name()[0] == '\0');
    CHECK(menu_app_get_dir_list(&list) == 10);
    st = press(st, PURRGO_ACTION_UP);
    CHECK(cur_name()[0] == '\0');

    /* Выбор файла: имя в конфиге, слой Route включается, возврат в CONFIG, каталог закрыт. */
    CHECK(!purrgo_layer_on(app_config.layers, PURRGO_LAYER_ROUTE));
    for (int k = 0; k < 4; k++) { st = press(st, PURRGO_ACTION_DOWN); }   /* r03 */
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(strcmp(app_config.route_file, "r03.gpx") == 0);
    CHECK(purrgo_layer_on(app_config.layers, PURRGO_LAYER_ROUTE));
    CHECK(fake_fs_open_dirs() == 0);
    {
        uint32_t n;
        const uint8_t *d = fake_fs_data(CFG, &n);
        CHECK(d != NULL && strstr((const char *)d, "ROUTE_FILE=r03.gpx\n") != NULL);
        CHECK(d != NULL && strstr((const char *)d, "LAYER_ROUTE=1\n") != NULL);
    }

    /* Файл со страницы 2 (после подгрузки). */
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_ROUTE_SELECT);
    for (int k = 0; k < 11; k++) { st = press(st, PURRGO_ACTION_DOWN); }  /* r10 */
    CHECK(strcmp(cur_name(), "r10.gpx") == 0);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(strcmp(app_config.route_file, "r10.gpx") == 0);

    /* BACK без выбора ничего не меняет. */
    st = press(st, PURRGO_ACTION_OK);
    st = press(st, PURRGO_ACTION_DOWN);
    st = press(st, PURRGO_ACTION_MENU);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(strcmp(app_config.route_file, "r10.gpx") == 0);
    CHECK(fake_fs_open_dirs() == 0);

    /* "(none)" снимает маршрут (слой остаётся как был). */
    st = press(st, PURRGO_ACTION_OK);
    CHECK(cur_name()[0] == '\0');
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(app_config.route_file[0] == '\0');
    CHECK(purrgo_layer_on(app_config.layers, PURRGO_LAYER_ROUTE));

    /* Каталога ROUTES нет (нет файлов): список состоит из одной записи "(none)". */
    fake_fs_reset();
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_ROUTE_SELECT);
    CHECK(menu_app_get_dir_list(&list) == 1);
    CHECK(cur_name()[0] == '\0');
    st = press(st, PURRGO_ACTION_DOWN);
    CHECK(menu_app_get_dir_cursor() == 0);
    st = press(st, PURRGO_ACTION_MENU);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(fake_fs_open_dirs() == 0);

    /* Выбор каталога карт (DIR) после маршрутов снова показывает каталоги, а не файлы. */
    st = APP_STATE_MENU_MAIN;
    purrgo_menu_controller_on_enter(APP_STATE_MENU_MAIN);
    for (int k = 0; k < 5; k++) { st = press(st, PURRGO_ACTION_DOWN); }   /* MAP DIR */
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_DIR_SELECT);
    CHECK(menu_app_get_dir_list(&list) == 0);               /* в fake-ФС нет подкаталогов */
    st = press(st, PURRGO_ACTION_MENU);
    CHECK(fake_fs_open_dirs() == 0);
}

int main(void)
{
    test_parser();
    test_decimation();
    test_load_and_sync();
    test_render();
    test_config_key();
    test_route_menu();
    return test_finish("test_route");
}
