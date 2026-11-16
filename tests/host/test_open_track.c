/*
 * Host-тесты просмотра записанных треков и маршрутов на карте:
 *   - имена файлов треков -> дата и время (track_name.c);
 *   - границы маршрута, загрузка из TRACKS, кэш по паре (имя, каталог);
 *   - подгонка центра и масштаба карты под границы (map_controller);
 *   - меню OPEN TRACK: порядок "новые сверху", страницы, защита файла, который
 *     пишется сейчас, ключ ROUTE_SRC в PURRGO.CFG;
 *   - выбор файла из ROUTES и TRACKS переводит карту на него.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include "purrgo/geo.h"
#include "purrgo/hardware_config.h"
#include "purrgo/map_controller.h"
#include "purrgo/menu_controller.h"
#include "purrgo/purrgo_time.h"
#include "purrgo/route.h"
#include "purrgo/track_logger.h"
#include "purrgo/track_name.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

#define CFG "PURRGO.CFG"

/* ----------------------------------------------------------- помощники --- */

/* Имя файла трека по времени старта (UTC) - так же, как его называет логгер. */
static void track_name(char *out, size_t size,
                       unsigned y, unsigned mo, unsigned d, unsigned h, unsigned mi, unsigned s)
{
    uint32_t e = 0;
    (void)purrgo_time_datetime_to_epoch((uint8_t)y, (uint8_t)mo, (uint8_t)d,
                                        (uint8_t)h, (uint8_t)mi, (uint8_t)s, &e);
    snprintf(out, size, "%08X.gpx", (unsigned)e);
}

static void put_in(const char *dir, const char *name, const char *text)
{
    char path[160];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    fake_fs_put(path, text);
}

/* Линия из двух точек (lat/lon в градусах, строкой). */
static void put_line(const char *dir, const char *name,
                     const char *lat1, const char *lon1, const char *lat2, const char *lon2)
{
    char gpx[400];
    snprintf(gpx, sizeof gpx,
             "<gpx><trk><trkseg>"
             "<trkpt lat=\"%s\" lon=\"%s\"/><trkpt lat=\"%s\" lon=\"%s\"/>"
             "</trkseg></trk></gpx>", lat1, lon1, lat2, lon2);
    put_in(dir, name, gpx);
}

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

static int find_main_item(const char *prefix)
{
    char buf[128];
    for (int i = 0; i < menu_app_get_main_count(); i++) {
        menu_app_get_main_item_text(i, buf, sizeof buf);
        if (strncmp(buf, prefix, strlen(prefix)) == 0) { return i; }
    }
    return -1;
}

/* Главное меню -> пункт с заголовком prefix -> OK. */
static purrgo_state_t open_item(const char *prefix)
{
    purrgo_state_t st = APP_STATE_MENU_MAIN;
    purrgo_menu_controller_on_enter(APP_STATE_MENU_MAIN);
    const int idx = find_main_item(prefix);
    for (int i = 0; i < idx; i++) { st = press(st, PURRGO_ACTION_DOWN); }
    return press(st, PURRGO_ACTION_OK);
}

static void reset_world(void)
{
    fake_fs_reset();
    purrgo_config_init();
    purrgo_menu_controller_init();
    purrgo_map_controller_init();
    purrgo_route_clear();
    purrgo_logger_reset();
}

/* ---------------------------------------------------------------- имена --- */

static void test_names(void)
{
    char n[32], label[64];
    uint32_t e = 0;

    track_name(n, sizeof n, 26, 11, 16, 8, 12, 5);
    CHECK(purrgo_track_name_decode(n, &e));
    uint32_t want = 0;
    CHECK(purrgo_time_datetime_to_epoch(26, 11, 16, 8, 12, 5, &want) && e == want);

    purrgo_track_name_label(n, 0, label, sizeof label);
    CHECK(strcmp(label, "2026-11-16 08:12") == 0);
    purrgo_track_name_label(n, 180, label, sizeof label);          /* UTC+3 */
    CHECK(strcmp(label, "2026-11-16 11:12") == 0);
    purrgo_track_name_label(n, -330, label, sizeof label);         /* UTC-5:30 */
    CHECK(strcmp(label, "2026-11-16 02:42") == 0);

    track_name(n, sizeof n, 26, 11, 16, 22, 30, 0);                /* переход через полночь */
    purrgo_track_name_label(n, 180, label, sizeof label);
    CHECK(strcmp(label, "2026-11-17 01:30") == 0);

    /* Регистр hex и расширения не важен. */
    track_name(n, sizeof n, 26, 1, 2, 3, 4, 5);
    for (char *p = n; *p; p++) { if (*p >= 'A' && *p <= 'F') { *p = (char)(*p - 'A' + 'a'); } }
    memcpy(n + 9, "GPX", 3);
    CHECK(purrgo_track_name_decode(n, NULL));

    /* Не раскодируется: чужое имя, не hex, не 8 цифр, прежний формат (малое значение). */
    const char *bad[] = { "route.gpx", "ZZZZZZZZ.gpx", "2F8A91C.gpx", "2F8A91C3A.gpx",
                          "2F8A91C3.txt", "00000123.gpx", "1FFFFFFF.gpx", "" };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        CHECK(!purrgo_track_name_decode(bad[i], &e));
        purrgo_track_name_label(bad[i], 0, label, sizeof label);
        CHECK(strcmp(label, bad[i]) == 0);                         /* как есть */
    }
    purrgo_track_name_label("longname.gpx", 0, label, 5);          /* обрезка по size */
    CHECK(strcmp(label, "long") == 0);

    CHECK(purrgo_track_name_cmp("2f8a91c3.gpx", "2F8A91C3.GPX") == 0);
    CHECK(purrgo_track_name_cmp("2F8A91C2.gpx", "2F8A91C3.gpx") < 0);
    CHECK(purrgo_track_name_cmp("2F8A91C4.gpx", "2f8a91c3.gpx") > 0);
}

/* ---------------------------------------------------- границы, каталоги --- */

static void test_bounds_and_src(void)
{
    int32_t a, b, c, d;

    reset_world();
    CHECK(!purrgo_route_get_bounds(&a, &b, &c, &d));               /* ничего не загружено */

    put_line("routes", "same.gpx", "53.1", "28.2", "53.2", "28.4");
    put_line("TRACKS", "same.gpx", "10.0", "20.0", "10.5", "20.25");

    CHECK(purrgo_route_load_src("same.gpx", PURRGO_ROUTE_SRC_TRACKS));
    CHECK(purrgo_route_get_bounds(&a, &b, &c, &d));
    CHECK(a == 100000000 && b == 200000000 && c == 105000000 && d == 202500000);

    CHECK(purrgo_route_load("same.gpx"));                          /* по умолчанию ROUTES */
    CHECK(purrgo_route_get_bounds(&a, &b, &c, &d));
    CHECK(a == 531000000 && b == 282000000 && c == 532000000 && d == 284000000);
    CHECK(!purrgo_route_get_bounds(NULL, &b, &c, &d));

    /* Кэш по паре (имя, каталог): тот же каталог не читается заново, другой - читается. */
    reset_world();
    put_line("routes", "same.gpx", "53.1", "28.2", "53.2", "28.4");
    put_line("TRACKS", "same.gpx", "10.0", "20.0", "10.5", "20.25");
    CHECK(purrgo_route_sync_src("same.gpx", PURRGO_ROUTE_SRC_ROUTES));
    const int opens = fake_fs_open_calls();
    CHECK(purrgo_route_sync_src("same.gpx", PURRGO_ROUTE_SRC_ROUTES));
    CHECK(fake_fs_open_calls() == opens);
    CHECK(purrgo_route_sync_src("same.gpx", PURRGO_ROUTE_SRC_TRACKS));
    CHECK(fake_fs_open_calls() == opens + 1);
    CHECK(purrgo_route_get_bounds(&a, &b, &c, &d) && a == 100000000);
    CHECK(fake_fs_open_handles() == 0);

    /* Трека нет в каталоге: загрузка не удалась, границ нет. */
    CHECK(!purrgo_route_sync_src("absent.gpx", PURRGO_ROUTE_SRC_TRACKS));
    CHECK(!purrgo_route_get_bounds(&a, &b, &c, &d));
}

/* -------------------------------------------------------------- подгонка --- */

/* Прямоугольник целиком внутри окна карты при текущих центре и масштабе. */
static bool box_inside_view(int32_t min_lat, int32_t min_lon, int32_t max_lat, int32_t max_lon)
{
    purrgo_viewport_t vp = {
        .width = PURRGO_HW_DISPLAY_WIDTH_PX,
        .height = PURRGO_MAP_VIEWPORT_HEIGHT_PX,
        .offset_x = 0,
        .offset_y = PURRGO_MAP_STATUS_BAR_H_PX
    };
    purrgo_bbox_t cam;
    purrgo_geo_bbox_from_center(map_app_get_map_center_lat(), map_app_get_map_center_lon(),
                                map_app_get_map_scale_width_m(), &vp, &cam);
    return min_lon > cam.min_x && max_lon < cam.max_x &&
           min_lat > cam.min_y && max_lat < cam.max_y;
}

/* То же с полями: прямоугольник, увеличенный на px пикселей со всех сторон, внутри окна. */
static bool box_inside_view_margin(int32_t min_lat, int32_t min_lon, int32_t max_lat, int32_t max_lon, int px)
{
    purrgo_viewport_t vp = {
        .width = PURRGO_HW_DISPLAY_WIDTH_PX,
        .height = PURRGO_MAP_VIEWPORT_HEIGHT_PX,
        .offset_x = 0,
        .offset_y = PURRGO_MAP_STATUS_BAR_H_PX
    };
    purrgo_bbox_t cam;
    purrgo_geo_bbox_from_center(map_app_get_map_center_lat(), map_app_get_map_center_lon(),
                                map_app_get_map_scale_width_m(), &vp, &cam);
    const int64_t dx = ((int64_t)cam.max_x - cam.min_x) * px / PURRGO_HW_DISPLAY_WIDTH_PX;
    const int64_t dy = ((int64_t)cam.max_y - cam.min_y) * px / PURRGO_MAP_VIEWPORT_HEIGHT_PX;
    return (int64_t)min_lon - dx > cam.min_x && (int64_t)max_lon + dx < cam.max_x &&
           (int64_t)min_lat - dy > cam.min_y && (int64_t)max_lat + dy < cam.max_y;
}

static void test_fit(void)
{
    reset_world();
    CHECK(!map_app_is_manual_pan_active());

    /* ~1 км x ~1 км на 53 град.: нужна ширина ~1,1 км -> 2 КМ. */
    purrgo_map_controller_fit_bounds(530000000, 280000000, 530090000, 280150000);
    CHECK(map_app_get_map_center_lat() == 530045000);
    CHECK(map_app_get_map_center_lon() == 280075000);
    CHECK(map_app_get_map_scale_width_m() == 2000);
    CHECK(map_app_is_manual_pan_active());                         /* автоследование выключено */
    CHECK(box_inside_view(530000000, 280000000, 530090000, 280150000));

    /* Высокий узкий трек (5 км по широте, ~100 м по долготе): определяет высота окна. */
    purrgo_map_controller_fit_bounds(530000000, 280000000, 530450000, 280015000);
    CHECK(map_app_get_map_scale_width_m() == 5000);
    CHECK(box_inside_view(530000000, 280000000, 530450000, 280015000));

    /* Широкий низкий (по долготе ~17 км на 53 град.): определяет ширина окна. */
    purrgo_map_controller_fit_bounds(530000000, 280000000, 530001000, 282500000);
    CHECK(map_app_get_map_scale_width_m() == 20000);

    /* Одна точка (нулевой размер): самый мелкий масштаб. */
    purrgo_map_controller_fit_bounds(530000000, 280000000, 530000000, 280000000);
    CHECK(map_app_get_map_scale_width_m() == 10);
    CHECK(map_app_get_map_center_lat() == 530000000 && map_app_get_map_center_lon() == 280000000);

    /* Весь мир: самый крупный масштаб, без переполнения. */
    purrgo_map_controller_fit_bounds(-850000000, -1790000000, 850000000, 1790000000);
    CHECK(map_app_get_map_zoom_level() == PURRGO_MAP_SCALE_COUNT - 1);

    /*
     * Набор прямоугольников разного размера и широты: выбранный масштаб вмещает
     * прямоугольник с полями, а один масштаб мельче (если он есть) - нет.
     */
    static const int32_t lat0[] = { 530000000, -330000000, 640000000, 10000000 };
    static const int32_t lon0[] = { 280000000, 1510000000, -210000000, -700000000 };
    static const int32_t dlat[] = { 700, 20000, 450000, 12345678, 3000000, 90000 };
    static const int32_t dlon[] = { 900, 30000, 150000, 7654321, 20000000, 400 };
    int checked = 0;
    for (size_t i = 0; i < 4; i++) {
        for (size_t j = 0; j < 6; j++) {
            const int32_t mn_lat = lat0[i], mn_lon = lon0[i];
            const int32_t mx_lat = lat0[i] + dlat[j], mx_lon = lon0[i] + dlon[j];
            purrgo_map_controller_fit_bounds(mn_lat, mn_lon, mx_lat, mx_lon);
            if (map_app_get_map_zoom_level() == PURRGO_MAP_SCALE_COUNT - 1) { continue; }
            checked++;
            CHECK(box_inside_view(mn_lat, mn_lon, mx_lat, mx_lon));
            if (map_app_get_map_zoom_level() > 0) {
                (void)purrgo_map_controller_handle_button(PURRGO_ACTION_PLUS);   /* на шаг мельче */
                CHECK(!box_inside_view_margin(mn_lat, mn_lon, mx_lat, mx_lon, 10));
            }
        }
    }
    CHECK(checked >= 18);

    /* Перевёрнутый прямоугольник игнорируется. */
    purrgo_map_controller_fit_bounds(530000000, 280000000, 530000000, 280000000);
    const int32_t keep_lat = map_app_get_map_center_lat();
    purrgo_map_controller_fit_bounds(540000000, 280000000, 530000000, 290000000);
    CHECK(map_app_get_map_center_lat() == keep_lat);
}

/* ------------------------------------------------------------ меню треков --- */

#define N_TRACKS 25

static void test_track_menu(void)
{
    reset_world();

    /* 25 треков в "перемешанном" порядке создания: порядок записей не равен порядку времени. */
    char names[N_TRACKS][16];
    for (int k = 0; k < N_TRACKS; k++) {
        const int i = (k * 7) % N_TRACKS;                          /* перестановка 0..24 */
        track_name(names[i], sizeof names[i], 26, 10, (unsigned)(1 + i), 6, 0, 0);
        char lat[16], lat2[16];
        snprintf(lat, sizeof lat, "%d.100", 50 + i);
        snprintf(lat2, sizeof lat2, "%d.101", 50 + i);
        put_line("TRACKS", names[i], lat, "28.200", lat2, "28.201");
    }
    put_in("TRACKS", "notes.txt", "x");
    put_in("TRACKS", "._hidden.gpx", "x");
    put_in("TRACKS", "00000123.gpx", "<trkpt lat=\"1\" lon=\"1\"/><trkpt lat=\"2\" lon=\"2\"/>");   /* прежний формат */

    purrgo_state_t st = open_item("OPEN TRACK");
    CHECK(st == APP_STATE_MENU_ROUTE_SELECT);
    CHECK(menu_app_dir_is_tracks());
    CHECK(fake_fs_open_dirs() == 0);                               /* каталог между кнопками не держится */

    purrgo_fs_dirent_t *list;
    CHECK(menu_app_get_dir_list(&list) == 10);
    CHECK(strcmp(cur_name(), names[N_TRACKS - 1]) == 0);           /* самый новый сверху, "(none)" нет */

    /* Вниз: строго по убыванию, на границах страниц без пропусков и дублей;
     * прежний формат (меньше всех) - последним. */
    for (int k = 1; k < N_TRACKS; k++) {
        st = press(st, PURRGO_ACTION_DOWN);
        CHECK(strcmp(cur_name(), names[N_TRACKS - 1 - k]) == 0);
    }
    st = press(st, PURRGO_ACTION_DOWN);
    CHECK(strcmp(cur_name(), "00000123.gpx") == 0);
    CHECK(menu_app_get_dir_list(&list) == 6);                      /* 26 записей: страницы 10 + 10 + 6 */
    st = press(st, PURRGO_ACTION_DOWN);                            /* дальше конца не уходит */
    CHECK(strcmp(cur_name(), "00000123.gpx") == 0);
    CHECK(fake_fs_open_dirs() == 0);

    /* Вверх обратно: те же записи. */
    st = press(st, PURRGO_ACTION_UP);
    CHECK(strcmp(cur_name(), names[0]) == 0);
    for (int k = 1; k < N_TRACKS; k++) {
        st = press(st, PURRGO_ACTION_UP);
        CHECK(strcmp(cur_name(), names[k]) == 0);
    }
    CHECK(strcmp(cur_name(), names[N_TRACKS - 1]) == 0);
    CHECK(menu_app_get_dir_list(&list) == 10);
    st = press(st, PURRGO_ACTION_UP);                              /* выше начала не уходит */
    CHECK(strcmp(cur_name(), names[N_TRACKS - 1]) == 0);

    /* Выбор трека со второй страницы: имя и источник в конфиге, слой Route включён. */
    CHECK(!purrgo_layer_on(app_config.layers, PURRGO_LAYER_ROUTE));
    for (int k = 0; k < 12; k++) { st = press(st, PURRGO_ACTION_DOWN); }
    const char *picked = names[N_TRACKS - 1 - 12];
    CHECK(strcmp(cur_name(), picked) == 0);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(strcmp(app_config.route_file, picked) == 0);
    CHECK(app_config.route_from_tracks);
    CHECK(purrgo_layer_on(app_config.layers, PURRGO_LAYER_ROUTE));
    CHECK(fake_fs_open_dirs() == 0 && fake_fs_open_handles() == 0);

    /* Карта переведена на трек: центр посередине, масштаб вмещает (~111 м x ~65 м -> 100 М). */
    CHECK(map_app_is_manual_pan_active());
    {
        const int i = N_TRACKS - 1 - 12;
        const int32_t lat_mid = (int32_t)(((int64_t)(50 + i) * 10000000 + 1000000 + (int64_t)(50 + i) * 10000000 + 1010000) / 2);
        CHECK(map_app_get_map_center_lat() == lat_mid);
        CHECK(map_app_get_map_center_lon() == (282000000 + 282010000) / 2);
        CHECK(map_app_get_map_scale_width_m() == 100);
        CHECK(box_inside_view((int32_t)((50 + i) * 10000000 + 1000000), 282000000,
                              (int32_t)((50 + i) * 10000000 + 1010000), 282010000));
    }

    /* Конфиг: ROUTE_SRC=1 записан и читается обратно; подпись пункта ROUTE - по времени. */
    {
        uint32_t n;
        const uint8_t *d = fake_fs_data(CFG, &n);
        CHECK(d != NULL && strstr((const char *)d, "ROUTE_SRC=1\n") != NULL);
        CHECK(d != NULL && strstr((const char *)d, "ROUTE_FILE=") != NULL);
    }
    {
        char buf[128];
        purrgo_menu_controller_on_enter(APP_STATE_MENU_MAIN);
        menu_app_get_main_item_text(find_main_item("ROUTE"), buf, sizeof buf);
        CHECK(strncmp(buf, "ROUTE: TRK 2026-10-", 19) == 0);
    }
    app_config.route_from_tracks = false;
    CHECK(purrgo_config_load());
    CHECK(app_config.route_from_tracks && strcmp(app_config.route_file, picked) == 0);

    /* Ключ ROUTE_SRC не 0/1 или отсутствует - ROUTES. */
    fake_fs_put(CFG, "ROUTE_FILE=a.gpx\nROUTE_SRC=7\n");
    CHECK(purrgo_config_load() && !app_config.route_from_tracks);
    fake_fs_put(CFG, "ROUTE_FILE=a.gpx\n");
    CHECK(purrgo_config_load() && !app_config.route_from_tracks);

    /* Выход из списка без выбора ничего не меняет. */
    fake_fs_put(CFG, "ROUTE_FILE=x.gpx\nROUTE_SRC=1\n");
    (void)purrgo_config_load();
    st = open_item("OPEN TRACK");
    st = press(st, PURRGO_ACTION_DOWN);
    st = press(st, PURRGO_ACTION_MENU);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(strcmp(app_config.route_file, "x.gpx") == 0 && app_config.route_from_tracks);
    CHECK(fake_fs_open_dirs() == 0);
}

static void test_track_menu_empty(void)
{
    reset_world();
    purrgo_state_t st = open_item("OPEN TRACK");
    purrgo_fs_dirent_t *list;
    CHECK(st == APP_STATE_MENU_ROUTE_SELECT);
    CHECK(menu_app_get_dir_list(&list) == 0);                      /* каталога TRACKS нет */
    st = press(st, PURRGO_ACTION_DOWN);
    st = press(st, PURRGO_ACTION_UP);
    st = press(st, PURRGO_ACTION_OK);                              /* нечего выбирать */
    CHECK(st == APP_STATE_MENU_ROUTE_SELECT);
    st = press(st, PURRGO_ACTION_MENU);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(fake_fs_open_dirs() == 0);
}

/* Файл, который пишется сейчас, помечен и не выбирается. */
static void test_recording_track_blocked(void)
{
    reset_world();

    char old_name[16];
    track_name(old_name, sizeof old_name, 26, 10, 1, 6, 0, 0);
    put_line("TRACKS", old_name, "53.1", "28.2", "53.2", "28.3");

    purrgo_gnss_solution_t f;
    memset(&f, 0, sizeof f);
    f.valid = true; f.year = 26; f.month = 11; f.day = 16;
    f.hours = 8; f.minutes = 12; f.seconds = 0;
    f.lat_1e7 = 537000000; f.lon_1e7 = 284000000; f.alt_m = 100; f.speed_knots = 500;
    CHECK(purrgo_logger_start(&f));
    const char *active = purrgo_logger_get_active_filename();
    CHECK(active != NULL);
    const char *active_name = strrchr(active, '/');
    active_name = active_name ? active_name + 1 : active;
    CHECK(purrgo_track_name_is_recording(active_name));
    CHECK(!purrgo_track_name_is_recording(old_name));

    purrgo_state_t st = open_item("OPEN TRACK");
    CHECK(st == APP_STATE_MENU_ROUTE_SELECT);
    purrgo_fs_dirent_t *list;
    CHECK(menu_app_get_dir_list(&list) == 2);
    CHECK(strcmp(cur_name(), active_name) == 0);                   /* новее: наверху */

    st = press(st, PURRGO_ACTION_OK);                              /* пишущийся файл не открывается */
    CHECK(st == APP_STATE_MENU_ROUTE_SELECT);
    CHECK(app_config.route_file[0] == '\0' && !app_config.route_from_tracks);

    st = press(st, PURRGO_ACTION_DOWN);                            /* а старый - да */
    CHECK(strcmp(cur_name(), old_name) == 0);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(strcmp(app_config.route_file, old_name) == 0 && app_config.route_from_tracks);

    purrgo_logger_stop();
    CHECK(!purrgo_track_name_is_recording(active_name));
    purrgo_logger_reset();
}

/* --------------------------------------------- маршруты из ROUTES тоже --- */

static void test_route_menu_fits(void)
{
    reset_world();
    put_line("routes", "hike.gpx", "53.100", "28.200", "53.110", "28.220");
    put_line("routes", "tiny.gpx", "53.1000", "28.2000", "53.1004", "28.2004");

    purrgo_state_t st = open_item("ROUTE");
    CHECK(st == APP_STATE_MENU_ROUTE_SELECT);
    CHECK(!menu_app_dir_is_tracks());
    CHECK(cur_name()[0] == '\0');                                  /* "(none)" на месте */
    st = press(st, PURRGO_ACTION_DOWN);
    CHECK(strcmp(cur_name(), "hike.gpx") == 0);
    CHECK(!map_app_is_manual_pan_active());
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(strcmp(app_config.route_file, "hike.gpx") == 0 && !app_config.route_from_tracks);

    /* ~1,1 км по широте, ~1,3 км по долготе -> ширина окна 2 км. */
    CHECK(map_app_is_manual_pan_active());
    CHECK(map_app_get_map_center_lat() == 531050000);
    CHECK(map_app_get_map_center_lon() == 282100000);
    CHECK(map_app_get_map_scale_width_m() == 2000);
    CHECK(box_inside_view(531000000, 282000000, 531100000, 282200000));

    /* Другой маршрут: центр и масштаб пересчитываются. */
    st = open_item("ROUTE");
    st = press(st, PURRGO_ACTION_DOWN);
    st = press(st, PURRGO_ACTION_DOWN);
    CHECK(strcmp(cur_name(), "tiny.gpx") == 0);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(map_app_get_map_scale_width_m() == 50);
    CHECK(map_app_get_map_center_lat() == 531002000);

    /* Повторный выбор того же файла снова переводит карту на него (после сдвига кнопками). */
    (void)purrgo_map_controller_handle_button(PURRGO_ACTION_UP);
    CHECK(map_app_get_map_center_lat() != 531002000);
    st = open_item("ROUTE");
    st = press(st, PURRGO_ACTION_DOWN);
    st = press(st, PURRGO_ACTION_DOWN);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(map_app_get_map_center_lat() == 531002000);

    /* "(none)" снимает маршрут и не трогает камеру. */
    const int32_t lat = map_app_get_map_center_lat();
    st = open_item("ROUTE");
    CHECK(cur_name()[0] == '\0');
    st = press(st, PURRGO_ACTION_OK);
    CHECK(app_config.route_file[0] == '\0' && !app_config.route_from_tracks);
    CHECK(map_app_get_map_center_lat() == lat);

    /* Файл, который не читается (нет линии): выбор записан, камера на месте. */
    put_in("routes", "empty.gpx", "<gpx></gpx>");
    st = open_item("ROUTE");
    for (int k = 0; k < 3; k++) { st = press(st, PURRGO_ACTION_DOWN); }
    CHECK(strcmp(cur_name(), "empty.gpx") == 0);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(strcmp(app_config.route_file, "empty.gpx") == 0);
    CHECK(map_app_get_map_center_lat() == lat);

    /* ROUTES -> TRACKS -> ROUTES: источник не "залипает". */
    char tn[16];
    track_name(tn, sizeof tn, 26, 10, 5, 6, 0, 0);
    put_line("TRACKS", tn, "10.0", "20.0", "10.1", "20.1");
    st = open_item("OPEN TRACK");
    CHECK(menu_app_dir_is_tracks());
    st = press(st, PURRGO_ACTION_OK);
    CHECK(app_config.route_from_tracks && strcmp(app_config.route_file, tn) == 0);
    st = open_item("ROUTE");
    CHECK(!menu_app_dir_is_tracks());
    CHECK(cur_name()[0] == '\0');
    st = press(st, PURRGO_ACTION_DOWN);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(!app_config.route_from_tracks);
    CHECK(fake_fs_open_dirs() == 0 && fake_fs_open_handles() == 0);
}

int main(void)
{
    test_names();
    test_bounds_and_src();
    test_fit();
    test_track_menu();
    test_track_menu_empty();
    test_recording_track_blocked();
    test_route_menu_fits();
    return test_finish("test_open_track");
}
