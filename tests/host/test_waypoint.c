/*
 * Host-тесты навигации к путевой точке (waypoint.c, menu_waypoint.c, gpx_parser.c):
 *   - форматирование расстояния, румбы, геометрия и угол стрелки;
 *   - чтение <wpt> из GPX (кавычки, порядок атрибутов, самозакрывающиеся теги,
 *     некорректные координаты, обрезка имён по границе UTF-8, переполнение списка);
 *   - цель в PURRGO.CFG (сохранение, загрузка, защита от неполных и неверных ключей);
 *   - расчёт расстояния и пеленга по текущему решению;
 *   - меню WAYPOINT (строки, выбор, прокрутка, сохранение, HERE, MAP CENTER);
 *   - запись точки MAP CENTER в WAYPTS (MPnnnnnn.GPX) и её чтение обратно;
 *   - иконка цели (35, остриё в точке цели) и перекрестие центра карты (47) в
 *     ручном режиме карты, блок цели на экране Trip computer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/app_ui.h"
#include "purrgo/config.h"
#include "purrgo/geo.h"
#include "purrgo/gfx_renderer.h"
#include "purrgo/gpx_parser.h"
#include "purrgo/hardware_config.h"
#include "purrgo/map_controller.h"
#include "purrgo/menu_controller.h"
#include "purrgo/purrgo_poi_icons.h"
#include "purrgo/ui/ui_dirty.h"
#include "purrgo/waypoint.h"
#include "purrgo/wpt_layer.h"
#include "map_projection.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

static uint32_t g_now = 100000;
uint32_t purrgo_system_time_ms(void) { return g_now; }

static const char *CFG = "PURRGO.CFG";

static void put_route(const char *name, const char *text)
{
    char path[160];
    snprintf(path, sizeof path, "routes/%s", name);
    fake_fs_put(path, text);
}

static const char *cfg_text(void)
{
    uint32_t n;
    const uint8_t *d = fake_fs_data(CFG, &n);
    return d ? (const char *)d : "";
}

/* ----------------------------------------------------------- помощники --- */

static void test_format_and_cardinal(void)
{
    char b[24];

    purrgo_wpt_format_distance(b, sizeof b, 0);      CHECK(strcmp(b, "0 m") == 0);
    purrgo_wpt_format_distance(b, sizeof b, 999);    CHECK(strcmp(b, "999 m") == 0);
    purrgo_wpt_format_distance(b, sizeof b, 1000);   CHECK(strcmp(b, "1.00 km") == 0);
    purrgo_wpt_format_distance(b, sizeof b, 1234);   CHECK(strcmp(b, "1.23 km") == 0);
    purrgo_wpt_format_distance(b, sizeof b, 9999);   CHECK(strcmp(b, "9.99 km") == 0);
    purrgo_wpt_format_distance(b, sizeof b, 10000);  CHECK(strcmp(b, "10.0 km") == 0);
    purrgo_wpt_format_distance(b, sizeof b, 12345);  CHECK(strcmp(b, "12.3 km") == 0);
    purrgo_wpt_format_distance(b, sizeof b, 99999);  CHECK(strcmp(b, "99.9 km") == 0);
    purrgo_wpt_format_distance(b, sizeof b, 100000); CHECK(strcmp(b, "100 km") == 0);
    purrgo_wpt_format_distance(b, sizeof b, 20015100); CHECK(strcmp(b, "20015 km") == 0);

    /* Малый буфер: обрезка без выхода за границу, ноль байт - без записи. */
    char small[4] = "xxx";
    purrgo_wpt_format_distance(small, sizeof small, 12345);
    CHECK(strlen(small) < sizeof small);
    purrgo_wpt_format_distance(small, 0, 5);
    purrgo_wpt_format_distance(NULL, 5, 5);

    CHECK(strcmp(purrgo_wpt_cardinal(0), "N") == 0);
    CHECK(strcmp(purrgo_wpt_cardinal(22), "N") == 0);
    CHECK(strcmp(purrgo_wpt_cardinal(23), "NE") == 0);
    CHECK(strcmp(purrgo_wpt_cardinal(45), "NE") == 0);
    CHECK(strcmp(purrgo_wpt_cardinal(90), "E") == 0);
    CHECK(strcmp(purrgo_wpt_cardinal(135), "SE") == 0);
    CHECK(strcmp(purrgo_wpt_cardinal(180), "S") == 0);
    CHECK(strcmp(purrgo_wpt_cardinal(225), "SW") == 0);
    CHECK(strcmp(purrgo_wpt_cardinal(270), "W") == 0);
    CHECK(strcmp(purrgo_wpt_cardinal(315), "NW") == 0);
    CHECK(strcmp(purrgo_wpt_cardinal(337), "NW") == 0);
    CHECK(strcmp(purrgo_wpt_cardinal(338), "N") == 0);
    CHECK(strcmp(purrgo_wpt_cardinal(359), "N") == 0);
    CHECK(strcmp(purrgo_wpt_cardinal(360), "N") == 0);
    CHECK(strcmp(purrgo_wpt_cardinal(450), "E") == 0);
}

static int ab(int v) { return v < 0 ? -v : v; }

static void test_arrow_geometry(void)
{
    purrgo_wpt_pt_t p[4];

    /* Вверх. */
    purrgo_wpt_arrow_points(100, 100, 20, 0, p);
    CHECK(p[1].x == 100 && p[1].y == 80);           /* остриё */
    CHECK(p[0].x == 100 && p[0].y == 112);          /* хвост: 0,6 r назад */
    CHECK(p[2].y == p[3].y && p[2].y > p[1].y);     /* наконечник ниже острия */
    CHECK(p[2].x < 100 && p[3].x > 100 && p[2].x + p[3].x == 200);

    /* Вправо, вниз, влево. */
    purrgo_wpt_arrow_points(100, 100, 20, 90, p);
    CHECK(p[1].x == 120 && p[1].y == 100 && p[0].x < 100);
    purrgo_wpt_arrow_points(100, 100, 20, 180, p);
    CHECK(p[1].x == 100 && p[1].y == 120 && p[0].y < 100);
    purrgo_wpt_arrow_points(100, 100, 20, 270, p);
    CHECK(p[1].x == 80 && p[1].y == 100 && p[0].x > 100);

    /* При любом угле остриё на расстоянии r от центра, наконечник симметричен оси. */
    for (int a = 0; a < 360; a += 7) {
        purrgo_wpt_arrow_points(150, 150, 30, (uint16_t)a, p);
        const int dx = p[1].x - 150, dy = p[1].y - 150;
        const int d2 = dx * dx + dy * dy;
        CHECK(d2 >= 29 * 29 && d2 <= 31 * 31);

        const int wl = (p[2].x - p[1].x) * (p[2].x - p[1].x) + (p[2].y - p[1].y) * (p[2].y - p[1].y);
        const int wr = (p[3].x - p[1].x) * (p[3].x - p[1].x) + (p[3].y - p[1].y) * (p[3].y - p[1].y);
        CHECK(ab(wl - wr) <= 60);       /* округление координат до пикселя */
    }

    /* Угол больше 360 эквивалентен остатку. */
    purrgo_wpt_pt_t q[4];
    purrgo_wpt_arrow_points(100, 100, 20, 450, p);
    purrgo_wpt_arrow_points(100, 100, 20, 90, q);
    CHECK(p[1].x == q[1].x && p[1].y == q[1].y);
}

static purrgo_gnss_solution_t fix_with_course(bool course_valid, int32_t course_deg_100, int32_t speed_kn_100)
{
    purrgo_gnss_solution_t f;
    memset(&f, 0, sizeof f);
    f.valid = true;
    f.course_valid = course_valid;
    f.course_deg_100 = course_deg_100;
    f.speed_knots = speed_kn_100;
    return f;
}

static void test_arrow_angle(void)
{
    uint16_t a = 999;

    /* Нет фикса (NULL) или курс недействителен: от севера. */
    CHECK(!purrgo_wpt_arrow_angle(NULL, 125, &a) && a == 125);
    purrgo_gnss_solution_t f = fix_with_course(false, 9000, 500);
    CHECK(!purrgo_wpt_arrow_angle(&f, 125, &a) && a == 125);

    /* Курс есть, но скорость мала: от севера. */
    f = fix_with_course(true, 9000, PURRGO_WPT_COURSE_MIN_SPEED_KNOTS_100 - 1);
    CHECK(!purrgo_wpt_arrow_angle(&f, 125, &a) && a == 125);

    /* Движение на восток (90), цель на юго-восток (125): стрелка вправо-вниз, 35. */
    f = fix_with_course(true, 9000, PURRGO_WPT_COURSE_MIN_SPEED_KNOTS_100);
    CHECK(purrgo_wpt_arrow_angle(&f, 125, &a) && a == 35);

    /* Цель позади: 180. Цель слева от курса: отрицательная разность приводится в 0..359. */
    f = fix_with_course(true, 0, 500);
    CHECK(purrgo_wpt_arrow_angle(&f, 180, &a) && a == 180);
    f = fix_with_course(true, 9000, 500);
    CHECK(purrgo_wpt_arrow_angle(&f, 45, &a) && a == 315);
    f = fix_with_course(true, 35999, 500);
    CHECK(purrgo_wpt_arrow_angle(&f, 0, &a) && a == 1);

    /* NULL вместо выхода не падает. */
    (void)purrgo_wpt_arrow_angle(&f, 10, NULL);
}

/* ----------------------------------------------------------------- GPX --- */

static void test_gpx_list(void)
{
    fake_fs_reset();
    CHECK(purrgo_wpt_list_load(NULL) == 0);
    CHECK(purrgo_wpt_list_load("") == 0);
    CHECK(purrgo_wpt_list_load("../x.gpx") == 0);
    CHECK(purrgo_wpt_list_load("a/b.gpx") == 0);
    CHECK(purrgo_wpt_list_load(".hid.gpx") == 0);
    CHECK(purrgo_wpt_list_load("none.gpx") == 0);        /* файла нет */
    CHECK(fake_fs_open_handles() == 0);

    put_route("p.gpx",
        "<?xml version=\"1.0\"?><gpx>"
        "<metadata><name>Track name</name></metadata>"
        "<wpt lat=\"53.9006\" lon=\"27.5590\"><ele>245.7</ele><name>Camp</name></wpt>"
        "<wpt lon='28.1' lat='-54.25'><name>Single Quotes</name><ele>-12</ele></wpt>"
        "<wpt lat=\"10.5\" lon=\"20.5\"/>"
        "<wpt lat=\"1\"><name>No lon</name></wpt>"
        "<wpt lat=\"91\" lon=\"10\"><name>Bad lat</name></wpt>"
        "<wpt lat=\"10\" lon=\"181\"><name>Bad lon</name></wpt>"
        "<wpt lat=\"abc\" lon=\"10\"><name>Not a number</name></wpt>"
        "<wpt lat=\"0\" lon=\"0\"><name></name></wpt>"
        "<wpt lat=\"-0.0000001\" lon=\"179.9999999\"><name>Edge</name></wpt>"
        "<rte><name>Route</name><rtept lat=\"5\" lon=\"5\"><name>rtept</name></rtept></rte>"
        "<trk><name>Trk</name><trkseg><trkpt lat=\"6\" lon=\"6\"/></trkseg></trk>"
        "</gpx>");

    CHECK(purrgo_wpt_list_load("p.gpx") == 5);
    CHECK(!purrgo_wpt_list_truncated());
    CHECK(fake_fs_open_handles() == 0);

    const purrgo_waypoint_t *w = purrgo_wpt_list_get(0);
    CHECK(w && w->lat_1e7 == 539006000 && w->lon_1e7 == 275590000);
    CHECK(w && strcmp(w->name, "Camp") == 0 && w->ele_m == 245);

    w = purrgo_wpt_list_get(1);
    CHECK(w && w->lat_1e7 == -542500000 && w->lon_1e7 == 281000000);
    CHECK(w && strcmp(w->name, "Single Quotes") == 0 && w->ele_m == -12);

    w = purrgo_wpt_list_get(2);                     /* самозакрывающийся, без имени */
    CHECK(w && w->lat_1e7 == 105000000 && w->lon_1e7 == 205000000);
    CHECK(w && strcmp(w->name, "WPT 3") == 0);

    w = purrgo_wpt_list_get(3);                     /* пустое имя: подставляется номер */
    CHECK(w && w->lat_1e7 == 0 && w->lon_1e7 == 0 && strcmp(w->name, "WPT 4") == 0);

    w = purrgo_wpt_list_get(4);
    CHECK(w && w->lat_1e7 == -1 && w->lon_1e7 == 1799999999 && strcmp(w->name, "Edge") == 0);

    CHECK(purrgo_wpt_list_get(5) == NULL);

    /* Повторная загрузка заменяет список; неверное имя очищает. */
    put_route("q.gpx", "<wpt lat=\"1\" lon=\"2\"><name>Only</name></wpt>");
    CHECK(purrgo_wpt_list_load("q.gpx") == 1);
    CHECK(purrgo_wpt_list_load("") == 0 && purrgo_wpt_list_count() == 0);
}

static void test_gpx_names_utf8(void)
{
    fake_fs_reset();

    /* 30 кириллических букв = 60 байт: имя укорочено до целого числа символов (23 байта -> 22). */
    char text[512];
    snprintf(text, sizeof text, "<wpt lat=\"1\" lon=\"2\"><name>");
    for (int i = 0; i < 30; i++) { strcat(text, "\xD0\x96"); }       /* "Ж" */
    strcat(text, "</name></wpt>");
    put_route("u.gpx", text);

    CHECK(purrgo_wpt_list_load("u.gpx") == 1);
    const purrgo_waypoint_t *w = purrgo_wpt_list_get(0);
    CHECK(w != NULL);
    if (w) {
        const size_t n = strlen(w->name);
        CHECK(n > 0 && n < PURRGO_WPT_NAME_MAX && n % 2 == 0);       /* целые двухбайтные символы */
        int bad = 0;
        for (size_t i = 0; i < n; i += 2) {
            if ((unsigned char)w->name[i] != 0xD0 || (unsigned char)w->name[i + 1] != 0x96) { bad++; }
        }
        CHECK(bad == 0);
    }

    /* Короткое русское имя целиком. */
    put_route("u2.gpx", "<wpt lat=\"1\" lon=\"2\"><name>\xD0\x94\xD0\xBE\xD0\xBC</name></wpt>");
    CHECK(purrgo_wpt_list_load("u2.gpx") == 1);
    CHECK(strcmp(purrgo_wpt_list_get(0)->name, "\xD0\x94\xD0\xBE\xD0\xBC") == 0);
}

static void test_gpx_overflow_and_chunks(void)
{
    fake_fs_reset();

    /* Больше точек, чем список; файл заведомо длиннее блока чтения, теги пересекают границы. */
    static char big[16384];
    size_t n = (size_t)snprintf(big, sizeof big, "<gpx>");
    const int total = PURRGO_WPT_LIST_MAX + 10;
    for (int i = 0; i < total; i++) {
        n += (size_t)snprintf(big + n, sizeof big - n,
                              "<wpt lat=\"%d.5\" lon=\"%d.25\"><ele>%d</ele><name>Point number %d</name></wpt>\n",
                              i % 80, i % 170, i, i);
    }
    snprintf(big + n, sizeof big - n, "</gpx>");
    CHECK(strlen(big) > 512 * 4);
    put_route("big.gpx", big);

    CHECK(purrgo_wpt_list_load("big.gpx") == (size_t)PURRGO_WPT_LIST_MAX);
    CHECK(purrgo_wpt_list_truncated());
    const purrgo_waypoint_t *w = purrgo_wpt_list_get(PURRGO_WPT_LIST_MAX - 1);
    CHECK(w && strcmp(w->name, "Point number 63") == 0);
    CHECK(w && w->lat_1e7 == 63 * 10000000 + 5000000 && w->lon_1e7 == 63 * 10000000 + 2500000);
    CHECK(purrgo_wpt_list_get(0) && strcmp(purrgo_wpt_list_get(0)->name, "Point number 0") == 0);
    CHECK(fake_fs_open_handles() == 0);
}

/* ------------------------------------------------------- цель и конфиг --- */

static void test_target_and_config(void)
{
    fake_fs_reset();
    purrgo_config_init();
    CHECK(!purrgo_wpt_is_set() && purrgo_wpt_target() == NULL);

    CHECK(purrgo_wpt_set(539006000, 275590000, "Camp site"));
    CHECK(purrgo_wpt_is_set());
    const purrgo_waypoint_t *t = purrgo_wpt_target();
    CHECK(t && t->lat_1e7 == 539006000 && t->lon_1e7 == 275590000 && strcmp(t->name, "Camp site") == 0);

    CHECK(purrgo_config_save());
    CHECK(strstr(cfg_text(), "WPT_SET=1\n") != NULL);
    CHECK(strstr(cfg_text(), "WPT_LAT=539006000\n") != NULL);
    CHECK(strstr(cfg_text(), "WPT_LON=275590000\n") != NULL);
    CHECK(strstr(cfg_text(), "WPT_NAME=Camp site\n") != NULL);

    /* После перезапуска цель восстанавливается. */
    purrgo_wpt_clear();
    CHECK(!purrgo_wpt_is_set());
    CHECK(purrgo_config_load());
    CHECK(purrgo_wpt_is_set());
    t = purrgo_wpt_target();
    CHECK(t && t->lat_1e7 == 539006000 && t->lon_1e7 == 275590000 && strcmp(t->name, "Camp site") == 0);

    /* Южное и западное полушарие, русское имя. */
    CHECK(purrgo_wpt_set(-337000000, -1511000000, "\xD0\x94\xD0\xBE\xD0\xBC"));
    CHECK(purrgo_config_save());
    purrgo_wpt_clear();
    CHECK(purrgo_config_load());
    t = purrgo_wpt_target();
    CHECK(t && t->lat_1e7 == -337000000 && t->lon_1e7 == -1511000000);
    CHECK(t && strcmp(t->name, "\xD0\x94\xD0\xBE\xD0\xBC") == 0);

    /* Снятие цели сохраняется. */
    purrgo_wpt_clear();
    CHECK(purrgo_config_save());
    CHECK(strstr(cfg_text(), "WPT_SET=0\n") != NULL);
    purrgo_wpt_set(1, 1, "x");
    CHECK(purrgo_config_load());
    CHECK(!purrgo_wpt_is_set());

    /* Неверные координаты и имя. */
    CHECK(!purrgo_wpt_set(900000001, 0, "x") && !purrgo_wpt_is_set());
    CHECK(!purrgo_wpt_set(0, 1800000001, "x") && !purrgo_wpt_is_set());
    CHECK(!purrgo_wpt_set(-900000001, 0, "x"));
    CHECK(!purrgo_wpt_set(0, -1800000001, "x"));

    CHECK(purrgo_wpt_set(1, 2, NULL));
    CHECK(strcmp(purrgo_wpt_target()->name, "WPT") == 0);
    CHECK(purrgo_wpt_set(1, 2, "   "));
    CHECK(strcmp(purrgo_wpt_target()->name, "WPT") == 0);
    CHECK(purrgo_wpt_set(1, 2, "A\nB\r=C"));                 /* перевод строки не ломает конфиг */
    CHECK(strchr(purrgo_wpt_target()->name, '\n') == NULL && strchr(purrgo_wpt_target()->name, '\r') == NULL);
    CHECK(purrgo_config_save());
    purrgo_wpt_clear();
    CHECK(purrgo_config_load() && purrgo_wpt_is_set());
    CHECK(strstr(purrgo_wpt_target()->name, "A B") == purrgo_wpt_target()->name);

    /* Длинное имя обрезается до размера поля и целого символа. */
    CHECK(purrgo_wpt_set(1, 2, "0123456789012345678901234567890123456789"));
    CHECK(strlen(purrgo_wpt_target()->name) == PURRGO_WPT_NAME_MAX - 1);

    /* Разбор файла: неполные и неверные наборы ключей цель не включают. */
    fake_fs_put(CFG, "WPT_SET=1\nWPT_LAT=100\nWPT_NAME=x\n");               /* нет LON */
    CHECK(purrgo_config_load() && !purrgo_wpt_is_set());
    fake_fs_put(CFG, "WPT_LAT=100\nWPT_LON=200\nWPT_NAME=x\n");             /* нет SET */
    CHECK(purrgo_config_load() && !purrgo_wpt_is_set());
    fake_fs_put(CFG, "WPT_SET=1\nWPT_LAT=999999999\nWPT_LON=200\n");        /* широта вне диапазона */
    CHECK(purrgo_config_load() && !purrgo_wpt_is_set());
    fake_fs_put(CFG, "WPT_SET=1\nWPT_LAT=100\nWPT_LON=1900000000\n");       /* долгота вне диапазона */
    CHECK(purrgo_config_load() && !purrgo_wpt_is_set());
    fake_fs_put(CFG, "WPT_SET=0\nWPT_LAT=100\nWPT_LON=200\n");
    CHECK(purrgo_config_load() && !purrgo_wpt_is_set());
    fake_fs_put(CFG, "WPT_SET=1\nWPT_LAT=100\nWPT_LON=200\nWPT_NAME=0123456789012345678901234567890\n");
    CHECK(purrgo_config_load() && purrgo_wpt_is_set() && app_config.wpt_name[0] == '\0');   /* слишком длинное имя игнорируется */
    fake_fs_put(CFG, "WPT_SET=1\nWPT_LAT=100\nWPT_LON=200\n");              /* без имени - допустимо */
    CHECK(purrgo_config_load() && purrgo_wpt_is_set());
    CHECK(purrgo_wpt_target()->lat_1e7 == 100 && purrgo_wpt_target()->lon_1e7 == 200);

    /* Старый конфиг без ключей: цели нет. */
    fake_fs_put(CFG, "TZ_MIN=120\n");
    CHECK(purrgo_config_load() && !purrgo_wpt_is_set());
}

static void test_nav_state(void)
{
    purrgo_gnss_solution_t fix;
    memset(&fix, 0, sizeof fix);
    fix.valid = true;
    fix.lat_1e7 = 539006000;
    fix.lon_1e7 = 275590000;

    fake_fs_reset();
    purrgo_config_init();

    purrgo_wpt_update(&fix);
    CHECK(!purrgo_wpt_nav()->valid);                /* цели нет */

    purrgo_wpt_set(539106000, 275590000, "N");       /* 0,01 градуса на север, ~1112 м */
    CHECK(!purrgo_wpt_nav()->valid);                /* до первого обновления */
    purrgo_wpt_update(&fix);
    const purrgo_wpt_nav_t *n = purrgo_wpt_nav();
    CHECK(n->valid && n->nav.distance_to_wp_m >= 1105 && n->nav.distance_to_wp_m <= 1120);
    CHECK(n->nav.bearing_to_wp_deg == 0 && !n->nav.is_arrived);

    fix.valid = false;                              /* фикс потерян */
    purrgo_wpt_update(&fix);
    CHECK(!purrgo_wpt_nav()->valid);
    purrgo_wpt_update(NULL);
    CHECK(!purrgo_wpt_nav()->valid);

    fix.valid = true;                               /* в радиусе прибытия */
    fix.lat_1e7 = 539106000 - 100;
    purrgo_wpt_update(&fix);
    CHECK(purrgo_wpt_nav()->valid && purrgo_wpt_nav()->nav.is_arrived);

    purrgo_wpt_clear();
    CHECK(!purrgo_wpt_nav()->valid);
}

/* -------------------------------------------------------------- рендер --- */

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
    return (x >= 0 && x < W && y >= 0 && y < H) ? ((uint8_t *)fb)[y * W + x] : 0;
}

static void canvas(void)
{
    memset(g_fb, 1, sizeof g_fb);       /* "серый" фон: 1 */
    (void)gfx_init(&g_gfx, W, H, g_fb, cb_draw, cb_read);
    gfx_set_color(&g_gfx, BLACK, WHITE);
}

static int count_px(uint8_t c)
{
    int n = 0;
    for (int i = 0; i < W * H; i++) { if (g_fb[i] == c) { n++; } }
    return n;
}

/*
 * Сверяет кадр с иконкой icon_no, нарисованной остриём в (tip_x, tip_y) (булавка:
 * остриё - нижний центральный пиксель): все непрозрачные пиксели иконки имеют её цвет,
 * прозрачные не тронуты (фон 1). Возвращает число несовпадений.
 */
static int icon_mismatch(int icon_no, int tip_x, int tip_y)
{
    int bad = 0;

    for (int r = 0; r < 11; r++) {
        for (int c = 0; c < 11; c++) {
            const uint8_t px = purrgo_poi_icons[icon_no][r][c];
            const int x = tip_x - 5 + c, y = tip_y - 10 + r;
            const uint8_t expect = (px & PURRGO_POI_ALPHA) ? (uint8_t)(px & 3u) : 1u;

            if (x >= 0 && x < W && y >= 0 && y < H && g_fb[y * W + x] != expect) {
                bad++;
            }
        }
    }
    return bad;
}

static int icon_opaque_count(int icon_no)
{
    int n = 0;

    for (int r = 0; r < 11; r++) {
        for (int c = 0; c < 11; c++) {
            if (purrgo_poi_icons[icon_no][r][c] & PURRGO_POI_ALPHA) { n++; }
        }
    }
    return n;
}

static void test_render(void)
{
    purrgo_bbox_t cam = { 280000000, 530000000, 280064000, 530048000 };
    purrgo_viewport_t vp = { W, H, 0, 0 };

    CHECK(PURRGO_WPT_TARGET_ICON == 35);

    fake_fs_reset();
    purrgo_config_init();

    /* Цели нет: ничего не рисуется; NULL не падают. */
    canvas();
    purrgo_wpt_render(&g_gfx, &cam, &vp);
    CHECK(count_px(BLACK) == 0 && count_px(WHITE) == 0);

    purrgo_wpt_set(530024000, 280032000, "C");        /* центр кадра: (32, 24) */
    purrgo_wpt_render(NULL, &cam, &vp);
    purrgo_wpt_render(&g_gfx, NULL, &vp);
    purrgo_wpt_render(&g_gfx, &cam, NULL);

    /* Иконка 35 пиксель в пиксель, остриё (нижний центр) ровно в точке цели. */
    canvas();
    purrgo_wpt_render(&g_gfx, &cam, &vp);
    CHECK(icon_mismatch(PURRGO_WPT_TARGET_ICON, 32, 24) == 0);
    CHECK(cb_read(g_fb, 32, 24) == BLACK);             /* остриё */
    CHECK(cb_read(g_fb, 32, 23) == BLACK && cb_read(g_fb, 32, 14) == BLACK);
    CHECK(count_px(BLACK) + count_px(WHITE) == icon_opaque_count(PURRGO_WPT_TARGET_ICON));   /* лишнего нет */
    CHECK(cb_read(g_fb, 32, 25) == 1 && cb_read(g_fb, 32, 13) == 1 && cb_read(g_fb, 0, 0) == 1);

    /* Цель за пределами кадра: ничего не рисуется. */
    purrgo_wpt_set(535000000, 285000000, "far");
    canvas();
    purrgo_wpt_render(&g_gfx, &cam, &vp);
    CHECK(count_px(BLACK) == 0 && count_px(WHITE) == 0);

    /* У самого края: иконка частично видна и не портит память. */
    purrgo_wpt_set(530024000, 280000000, "edge");
    canvas();
    purrgo_wpt_render(&g_gfx, &cam, &vp);
    CHECK(count_px(BLACK) > 5);
    CHECK(icon_mismatch(PURRGO_WPT_TARGET_ICON, 0, 24) == 0);

    /* Смещённое окно карты (строка статуса сверху): положение считается от окна. */
    purrgo_viewport_t vp2 = { W, H - 9, 0, 9 };
    purrgo_wpt_set(530021000, 280032000, "c2");
    int16_t tx, ty;
    project_to_screen(280032000, 530021000, &cam, &vp2, &tx, &ty);
    canvas();
    purrgo_wpt_render(&g_gfx, &cam, &vp2);
    CHECK(count_px(BLACK) > 20);
    CHECK(icon_mismatch(PURRGO_WPT_TARGET_ICON, tx, ty) == 0);
    int top_rows_clean = 1;
    for (int y = 0; y < 9; y++) for (int x = 0; x < W; x++) if (g_fb[y * W + x] != 1) top_rows_clean = 0;
    CHECK(top_rows_clean);                              /* строка статуса не тронута */
}

/* --------------------------------------------- запись точки в WAYPTS --- */

static void parse_one(const char *file, purrgo_waypoint_t *out, size_t *count)
{
    uint32_t n = 0;
    const uint8_t *d = fake_fs_data(file, &n);
    purrgo_gpx_parser_t parser;

    *count = 0;
    CHECK(d != NULL);
    if (d == NULL) { return; }
    purrgo_gpx_parser_init(&parser, out, 1);
    purrgo_gpx_parser_feed(&parser, (const char *)d, n);
    *count = parser.total_seen;
}

static void test_save_to_waypts(void)
{
    char name[PURRGO_WPT_NAME_MAX];
    purrgo_waypoint_t w;
    size_t cnt;

    fake_fs_reset();
    purrgo_config_init();

    /* Первая точка: MP000001.GPX, имя MP1, координаты читаются обратно как записаны. */
    CHECK(purrgo_wpt_save_to_waypts(539006123, 275590456, name, sizeof name));
    CHECK(strcmp(name, "MP1") == 0);
    parse_one("waypts/MP000001.GPX", &w, &cnt);
    CHECK(cnt == 1 && w.lat_1e7 == 539006123 && w.lon_1e7 == 275590456 && strcmp(w.name, "MP1") == 0);
    CHECK(fake_fs_open_handles() == 0 && fake_fs_open_dirs() == 0);

    /* Файл - корректный GPX 1.1 (закрыт тегом gpx), цель сохранение не задаёт. */
    uint32_t n = 0;
    const uint8_t *d = fake_fs_data("waypts/MP000001.GPX", &n);
    CHECK(d != NULL && strstr((const char *)d, "<gpx version=\"1.1\"") != NULL &&
          strstr((const char *)d, "</gpx>") != NULL);
    CHECK(!purrgo_wpt_is_set());

    /* Следующая - номер 2; южное и западное полушария, малые по модулю значения. */
    CHECK(purrgo_wpt_save_to_waypts(-123456789, -5, name, sizeof name));
    CHECK(strcmp(name, "MP2") == 0);
    parse_one("waypts/MP000002.GPX", &w, &cnt);
    CHECK(cnt == 1 && w.lat_1e7 == -123456789 && w.lon_1e7 == -5);

    /* Предельные координаты проходят, выход за диапазон - нет, файл не создаётся. */
    CHECK(purrgo_wpt_save_to_waypts(900000000, 1800000000, NULL, 0));
    CHECK(!purrgo_wpt_save_to_waypts(900000001, 0, name, sizeof name));
    CHECK(!purrgo_wpt_save_to_waypts(0, -1800000001, name, sizeof name));
    CHECK(fake_fs_data("waypts/MP000004.GPX", &n) == NULL);

    /* Номер - максимум среди имеющихся (любой регистр) + 1; чужие файлы игнорируются. */
    fake_fs_reset();
    fake_fs_put("waypts/mp000010.gpx", "x");
    fake_fs_put("waypts/MP000003.GPX", "x");
    fake_fs_put("waypts/other.gpx", "x");
    fake_fs_put("waypts/MP12.GPX", "x");
    CHECK(purrgo_wpt_save_to_waypts(1, 2, name, sizeof name) && strcmp(name, "MP11") == 0);
    CHECK(fake_fs_data("waypts/MP000011.GPX", &n) != NULL);

    /* Сбои записи, открытия: false, цель не меняется, дескрипторы не текут. */
    fake_fs_reset();
    g_fault.fail_write = 1;
    CHECK(!purrgo_wpt_save_to_waypts(1, 2, name, sizeof name));
    CHECK(fake_fs_open_handles() == 0);
    fake_fs_reset();
    g_fault.fail_sync = 1;
    CHECK(!purrgo_wpt_save_to_waypts(1, 2, name, sizeof name));
    fake_fs_reset();
    g_fault.fail_open_all = 1;
    CHECK(!purrgo_wpt_save_to_waypts(1, 2, name, sizeof name));
    CHECK(fake_fs_open_handles() == 0 && fake_fs_open_dirs() == 0);

    /* Слой Waypoints видит записанную точку после purrgo_wpt_layer_clear(). */
    fake_fs_reset();
    purrgo_wpt_layer_clear();
    CHECK(!purrgo_wpt_layer_sync(""));
    CHECK(purrgo_wpt_save_to_waypts(530024000, 280032000, name, sizeof name));
    purrgo_wpt_layer_clear();
    CHECK(purrgo_wpt_layer_sync("") && purrgo_wpt_layer_count() == 1);
    CHECK(purrgo_wpt_layer_get(0)->lat_1e7 == 530024000 && purrgo_wpt_layer_get(0)->lon_1e7 == 280032000);
    purrgo_wpt_layer_clear();
}

/* ---------------------------------------------------------------- меню --- */

static purrgo_state_t press(purrgo_state_t st, purrgo_action_t a)
{
    purrgo_state_t next = st;
    (void)purrgo_menu_controller_handle_button(st, a, &next);
    return next;
}

static int find_main_item(const char *prefix)
{
    char buf[128];
    for (int i = 0; i < menu_app_get_main_count(); i++) {
        menu_app_get_main_item_text(i, buf, sizeof buf);
        if (strncmp(buf, prefix, strlen(prefix)) == 0) { return i; }
    }
    return -1;
}

static purrgo_state_t open_wpt_menu(void)
{
    purrgo_state_t st = APP_STATE_MENU_MAIN;
    purrgo_menu_controller_on_enter(APP_STATE_MENU_MAIN);
    const int idx = find_main_item("WAYPOINT");
    for (int i = 0; i < idx; i++) { st = press(st, PURRGO_ACTION_DOWN); }
    st = press(st, PURRGO_ACTION_OK);
    return st;
}

static const char *item(int i)
{
    static char b[64];
    menu_app_get_wpt_item_text(i, b, sizeof b);
    return b;
}

static void test_menu(void)
{
    fake_fs_reset();
    purrgo_config_init();
    purrgo_menu_controller_init();

    put_route("pts.gpx",
        "<wpt lat=\"53.9\" lon=\"27.5\"><name>Alpha</name></wpt>"
        "<wpt lat=\"54.1\" lon=\"28.5\"><name>Beta</name></wpt>"
        "<wpt lat=\"-10\" lon=\"-20\"><name>Gamma</name></wpt>");
    snprintf(app_config.route_file, sizeof app_config.route_file, "%s", "pts.gpx");

    /* Пункт в главном меню: идёт после ROUTE, показывает цель. */
    purrgo_menu_controller_on_enter(APP_STATE_MENU_MAIN);
    const int idx = find_main_item("WAYPOINT");
    CHECK(idx == find_main_item("ROUTE") + 1);
    char buf[128];
    menu_app_get_main_item_text(idx, buf, sizeof buf);
    CHECK(strcmp(buf, "WAYPOINT: NONE") == 0);

    purrgo_state_t st = open_wpt_menu();
    CHECK(st == APP_STATE_MENU_WPT_SELECT);
    CHECK(menu_app_get_wpt_count() == 6);
    CHECK(menu_app_get_wpt_cursor() == 0 && menu_app_get_wpt_top() == 0);
    CHECK(strcmp(item(0), "(none) *") == 0);            /* цели нет: отмечено */
    CHECK(strcmp(item(1), "HERE (no fix)") == 0);
    CHECK(strcmp(item(2), "MAP CENTER") == 0);
    CHECK(strcmp(item(3), "Alpha") == 0 && strcmp(item(4), "Beta") == 0 && strcmp(item(5), "Gamma") == 0);

    /* Границы курсора без циклического перехода. */
    st = press(st, PURRGO_ACTION_UP);
    CHECK(menu_app_get_wpt_cursor() == 0);
    for (int i = 0; i < 10; i++) { st = press(st, PURRGO_ACTION_DOWN); }
    CHECK(menu_app_get_wpt_cursor() == 5);

    /* HERE без фикса: остаёмся в списке, цель не задана. */
    for (int i = 0; i < 5; i++) { st = press(st, PURRGO_ACTION_UP); }
    st = press(st, PURRGO_ACTION_DOWN);
    CHECK(menu_app_get_wpt_cursor() == 1);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_WPT_SELECT && !purrgo_wpt_is_set());

    /* Выбор точки файла (Beta): цель задана, конфиг сохранён, карта помечена, возврат в меню. */
    st = press(st, PURRGO_ACTION_DOWN);
    st = press(st, PURRGO_ACTION_DOWN);
    st = press(st, PURRGO_ACTION_DOWN);
    CHECK(menu_app_get_wpt_cursor() == 4);
    purrgo_ui_dirty_clear(UI_DIRTY_MAP);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(purrgo_wpt_is_set());
    CHECK(app_config.wpt_lat_1e7 == 541000000 && app_config.wpt_lon_1e7 == 285000000);
    CHECK(strcmp(app_config.wpt_name, "Beta") == 0);
    CHECK(strstr(cfg_text(), "WPT_NAME=Beta\n") != NULL);
    CHECK(purrgo_ui_dirty_test(UI_DIRTY_MAP));
    menu_app_get_main_item_text(idx, buf, sizeof buf);
    CHECK(strcmp(buf, "WAYPOINT: Beta") == 0);

    /* Повторный вход: отмечена выбранная точка, "(none)" без отметки. */
    st = open_wpt_menu();
    CHECK(st == APP_STATE_MENU_WPT_SELECT);
    CHECK(strcmp(item(0), "(none)") == 0);
    CHECK(strcmp(item(3), "Alpha") == 0 && strcmp(item(4), "Beta *") == 0);

    /* BACK ничего не меняет. */
    st = press(st, PURRGO_ACTION_DOWN);
    st = press(st, PURRGO_ACTION_MENU);
    CHECK(st == APP_STATE_MENU_MAIN && strcmp(app_config.wpt_name, "Beta") == 0);

    /* "(none)" снимает цель. */
    st = open_wpt_menu();
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_MAIN && !purrgo_wpt_is_set());
    CHECK(strstr(cfg_text(), "WPT_SET=0\n") != NULL);

    /* Без выбранного маршрута точек из файла нет. */
    app_config.route_file[0] = '\0';
    st = open_wpt_menu();
    CHECK(menu_app_get_wpt_count() == 3);
    st = press(st, PURRGO_ACTION_MENU);
}

static void test_menu_scroll(void)
{
    fake_fs_reset();
    purrgo_config_init();
    purrgo_menu_controller_init();

    static char big[8192];
    size_t n = 0;
    for (int i = 0; i < 40; i++) {
        n += (size_t)snprintf(big + n, sizeof big - n,
                              "<wpt lat=\"%d\" lon=\"%d\"><name>P%02d</name></wpt>", 10 + i, 20 + i, i);
    }
    put_route("many.gpx", big);
    snprintf(app_config.route_file, sizeof app_config.route_file, "%s", "many.gpx");

    purrgo_state_t st = open_wpt_menu();
    const int vis = menu_app_get_wpt_visible();
    CHECK(menu_app_get_wpt_count() == 43);
    CHECK(vis >= 8 && vis <= 16);                       /* помещается в ui_rows */

    int ok = 1;
    for (int i = 0; i < 42; i++) {
        st = press(st, PURRGO_ACTION_DOWN);
        const int c = menu_app_get_wpt_cursor(), t = menu_app_get_wpt_top();
        if (!(c >= t && c < t + vis)) { ok = 0; }
    }
    CHECK(ok);
    CHECK(menu_app_get_wpt_cursor() == 42 && menu_app_get_wpt_top() == 43 - vis);
    CHECK(strcmp(item(42), "P39") == 0);

    for (int i = 0; i < 42; i++) {
        st = press(st, PURRGO_ACTION_UP);
        const int c = menu_app_get_wpt_cursor(), t = menu_app_get_wpt_top();
        if (!(c >= t && c < t + vis)) { ok = 0; }
    }
    CHECK(ok && menu_app_get_wpt_cursor() == 0 && menu_app_get_wpt_top() == 0);

    /* Последняя точка выбирается. */
    for (int i = 0; i < 42; i++) { st = press(st, PURRGO_ACTION_DOWN); }
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_MAIN && strcmp(app_config.wpt_name, "P39") == 0);
    CHECK(app_config.wpt_lat_1e7 == 49 * 10000000 && app_config.wpt_lon_1e7 == 59 * 10000000);
}

/* ----------------------------------------------- приложение целиком --- */

#define FW 176
#define FH 264
static uint8_t g_ffb[FW * FH];

static void fcb_draw(void *fb, int16_t x, int16_t y, gfx_color_t c)
{
    (void)fb;
    if (x >= 0 && x < FW && y >= 0 && y < FH) { g_ffb[y * FW + x] = (uint8_t)(c + 1); }
}
static gfx_color_t fcb_read(void *fb, int16_t x, int16_t y)
{
    (void)fb;
    return (x >= 0 && x < FW && y >= 0 && y < FH) ? (gfx_color_t)(g_ffb[y * FW + x] - 1) : 0;
}

static int fcount(int x0, int y0, int x1, int y1)
{
    int n = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) if (g_ffb[y * FW + x] == BLACK + 1) n++;
    return n;
}

static void feed(const char *body)
{
    char line[160];
    unsigned cs = 0;
    for (const char *p = body; *p; p++) { cs ^= (unsigned char)*p; }
    snprintf(line, sizeof line, "$%s*%02X", body, cs);
    for (size_t i = 0; line[i]; i++) { purrgo_app_feed_gnss_byte((uint8_t)line[i]); }
    purrgo_app_feed_gnss_byte('\r');
    purrgo_app_feed_gnss_byte('\n');
}

/* Фикс: 53 град 54.036 мин с.ш. = 53.9006, 27 град 33.54 мин в.д. = 27.559; курс 90, скорость 5 узлов. */
static void feed_fix(void)
{
    feed("GPRMC,093015.00,A,5354.03600,N,02733.54000,E,5.00,90.00,191026,,,A");
    feed("GPGGA,093015.00,5354.03600,N,02733.54000,E,1,08,1.1,100.0,M,18.9,M,,");
}

static void run_ticks(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 100) { g_now += 100; purrgo_app_tick(g_now); }
}

static void test_app_here_map_center_and_trip(void)
{
    gfx_context_t g;

    fake_fs_reset();
    memset(&app_config, 0, sizeof app_config);
    app_config.last_lat_1e7 = 539006000;
    app_config.last_lon_1e7 = 275590000;
    g_now = 200000;
    purrgo_app_init();
    run_ticks(1000);
    feed_fix();
    CHECK(purrgo_app_get_gnss_solution()->valid);

    /* MAP CENTER: цель - центр карты. */
    purrgo_state_t st = open_wpt_menu();
    CHECK(st == APP_STATE_MENU_WPT_SELECT);
    CHECK(strcmp(item(1), "HERE") == 0);                /* фикс есть */
    st = press(st, PURRGO_ACTION_DOWN);
    st = press(st, PURRGO_ACTION_DOWN);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_MAIN && purrgo_wpt_is_set());
    CHECK(app_config.wpt_lat_1e7 == map_app_get_map_center_lat());
    CHECK(app_config.wpt_lon_1e7 == map_app_get_map_center_lon());
    /* Точка записана в WAYPTS как MP1 и стала целью под тем же именем. */
    CHECK(strcmp(app_config.wpt_name, "MP1") == 0);
    {
        purrgo_waypoint_t saved;
        size_t cnt;
        parse_one("waypts/MP000001.GPX", &saved, &cnt);
        CHECK(cnt == 1 && saved.lat_1e7 == map_app_get_map_center_lat() &&
              saved.lon_1e7 == map_app_get_map_center_lon() && strcmp(saved.name, "MP1") == 0);
    }

    /* Повторный выбор MAP CENTER - новый файл, а не перезапись. */
    st = open_wpt_menu();
    st = press(st, PURRGO_ACTION_DOWN);
    st = press(st, PURRGO_ACTION_DOWN);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(strcmp(app_config.wpt_name, "MP2") == 0);
    {
        uint32_t sz;
        CHECK(fake_fs_data("waypts/MP000001.GPX", &sz) != NULL && fake_fs_data("waypts/MP000002.GPX", &sz) != NULL);
    }

    /* HERE: цель - текущая позиция, расстояние около нуля, прибытие. */
    st = open_wpt_menu();
    st = press(st, PURRGO_ACTION_DOWN);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_MAIN && strcmp(app_config.wpt_name, "HERE") == 0);
    const purrgo_gnss_solution_t *fx = purrgo_app_get_gnss_solution();
    CHECK(app_config.wpt_lat_1e7 == fx->lat_1e7 && app_config.wpt_lon_1e7 == fx->lon_1e7);

    run_ticks(2000);
    CHECK(purrgo_wpt_nav()->valid && purrgo_wpt_nav()->nav.distance_to_wp_m <= 2);
    CHECK(purrgo_wpt_nav()->nav.is_arrived);

    /* Цель в 0,01 градуса севернее (~1112 м): пересчёт идёт в основном цикле. */
    purrgo_wpt_set(fx->lat_1e7 + 100000, fx->lon_1e7, "North");
    run_ticks(2000);
    CHECK(purrgo_wpt_nav()->valid);
    CHECK(purrgo_wpt_nav()->nav.distance_to_wp_m >= 1105 && purrgo_wpt_nav()->nav.distance_to_wp_m <= 1120);
    CHECK(purrgo_wpt_nav()->nav.bearing_to_wp_deg == 0 && !purrgo_wpt_nav()->nav.is_arrived);

    /* Экран Trip computer: блок цели рисует имя, расстояние, пеленг и стрелку. */
    memset(g_ffb, 0, sizeof g_ffb);
    (void)gfx_init(&g, FW, FH, g_ffb, fcb_draw, fcb_read);
    CHECK(purrgo_app_get_state() == APP_STATE_MAP);
    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);     /* MENU: MAP -> TRIP COMPUTER */
    CHECK(purrgo_app_get_state() == APP_STATE_TRIP_COMPUTER);
    purrgo_app_ui_render(&g, purrgo_app_get_gnss_solution(), NULL);

    CHECK(fcount(40, 106, 130, 118) > 15);              /* имя "North" */
    CHECK(fcount(40, 118, 130, 130) > 15);              /* "1.11 km" */
    CHECK(fcount(40, 130, 130, 142) > 15);              /* "000 deg N" */
    CHECK(fcount(128, 38, 174, 86) > 30);               /* стрелка и метка */

    /* Цель снята: блок пуст, стрелки нет. */
    purrgo_wpt_clear();
    run_ticks(1500);
    memset(g_ffb, 0, sizeof g_ffb);
    purrgo_app_ui_render(&g, purrgo_app_get_gnss_solution(), NULL);
    CHECK(fcount(128, 38, 174, 86) == 0);
    CHECK(fcount(40, 118, 130, 142) == 0);              /* DST и BRG пусты */
    CHECK(fcount(40, 106, 130, 118) > 5);               /* "NONE" */
}

/* Сверка кадра приложения с иконкой с центром в (cx, cy). */
static int fmismatch_icon(int icon_no, int cx, int cy)
{
    int bad = 0;

    for (int r = 0; r < 11; r++) {
        for (int c = 0; c < 11; c++) {
            const uint8_t px = purrgo_poi_icons[icon_no][r][c];
            const int x = cx - 5 + c, y = cy - 5 + r;

            if ((px & PURRGO_POI_ALPHA) && g_ffb[y * FW + x] != (uint8_t)((px & 3u) + 1u)) {
                bad++;
            }
        }
    }
    return bad;
}

static void test_map_crosshair(void)
{
    gfx_context_t g;
    const int cx = PURRGO_HW_DISPLAY_WIDTH_PX / 2;
    const int cy = PURRGO_MAP_STATUS_BAR_H_PX + PURRGO_MAP_VIEWPORT_HEIGHT_PX / 2;

    fake_fs_reset();
    memset(&app_config, 0, sizeof app_config);
    app_config.last_lat_1e7 = 539006000;
    app_config.last_lon_1e7 = 275590000;
    app_config.layers = 0;
    g_now = 300000;
    purrgo_app_init();
    run_ticks(1000);
    feed_fix();
    CHECK(purrgo_app_get_state() == APP_STATE_MAP);
    (void)gfx_init(&g, FW, FH, g_ffb, fcb_draw, fcb_read);

    /* Автоследование включено: перекрестия нет. */
    CHECK(!map_app_is_manual_pan_active());

    /* Сдвиг карты кнопкой: ручной режим, перекрестие (иконка 47) в центре окна карты. */
    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_LEFT));
    CHECK(map_app_is_manual_pan_active() && purrgo_ui_dirty_test(UI_DIRTY_MAP));
    memset(g_ffb, 0, sizeof g_ffb);
    purrgo_app_ui_render(&g, purrgo_app_get_gnss_solution(), NULL);
    CHECK(fmismatch_icon(47, cx, cy) == 0);
    CHECK(fcount(cx - 5, cy - 5, cx + 6, cy + 6) > 20);

    /* Точка, взятая через MAP CENTER, - это координаты под перекрестием. */
    CHECK(map_app_get_map_center_lon() < app_config.last_lon_1e7);

    /* OK возвращает автоследование: перекрестие снимается полной перерисовкой. */
    purrgo_ui_dirty_clear(UI_DIRTY_MAP);
    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_OK));
    CHECK(!map_app_is_manual_pan_active());
    CHECK(purrgo_ui_dirty_test(UI_DIRTY_MAP));
    purrgo_app_ui_render(&g, purrgo_app_get_gnss_solution(), NULL);
    CHECK(fcount(cx - 5, cy - 5, cx + 6, cy + 6) == 0);
}

int main(void)
{
    test_format_and_cardinal();
    test_arrow_geometry();
    test_arrow_angle();
    test_gpx_list();
    test_gpx_names_utf8();
    test_gpx_overflow_and_chunks();
    test_target_and_config();
    test_nav_state();
    test_render();
    test_save_to_waypts();
    test_menu();
    test_menu_scroll();
    test_app_here_map_center_and_trip();
    test_map_crosshair();
    return test_finish("test_waypoint");
}
