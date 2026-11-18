/*
 * Host-тесты автопочинки недописанных GPX (gpx_repair.c + загрузка трека в route.c):
 *   - сканер: обрыв без футера, посреди точки, посреди футера, целый файл, чужие файлы;
 *   - один и тот же результат при подаче по 1 байту и целиком;
 *   - открытие трека чинит файл на "SD", повторное открытие его не меняет;
 *   - файл, который ведёт логгер, не трогается; ROUTES не трогается;
 *   - сбои (не открылась запись, не обрезалось) не ломают загрузку и не повторяются;
 *   - после починки GPX читается логгером/просмотром без потери точек.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "purrgo/config.h"
#include "purrgo/gpx_repair.h"
#include "purrgo/route.h"
#include "purrgo/track_logger.h"
#include "fake_fs.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

#define HEADER "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" \
               "<gpx version=\"1.1\" creator=\"PurrGo\" xmlns=\"http://www.topografix.com/GPX/1/1\">\n" \
               "<trk><trkseg>\n"
#define PT1 "<trkpt lat=\"53.1000000\" lon=\"28.2000000\"><ele>100</ele><time>2026-11-16T08:12:00Z</time>" \
            "<extensions><gpxtpx:TrackPointExtension><gpxtpx:speed>1.5</gpxtpx:speed></gpxtpx:TrackPointExtension>" \
            "</extensions></trkpt>\n"
#define PT2 "<trkpt lat=\"53.1001000\" lon=\"28.2001000\"><ele>101</ele><time>2026-11-16T08:12:05Z</time></trkpt>\n"
#define PT3 "<trkpt lat=\"53.1002000\" lon=\"28.2002000\"><ele>102</ele><time>2026-11-16T08:12:10Z</time></trkpt>\n"
#define FOOTER "</trkseg></trk>\n</gpx>\n"
#define FIXED_FOOTER "\n</trkseg></trk>\n</gpx>\n"

static const char *PATH = "TRACKS/2F8A91C3.gpx";
static const char *NAME = "2F8A91C3.gpx";

static void reset(void)
{
    fake_fs_reset();
    purrgo_config_init();
    purrgo_route_clear();
    purrgo_logger_reset();
}

static void scan_all(purrgo_gpx_scan_t *s, const char *text, size_t chunk)
{
    const size_t n = strlen(text);
    purrgo_gpx_scan_init(s);
    for (size_t i = 0; i < n; i += chunk) {
        purrgo_gpx_scan_feed(s, text + i, (n - i < chunk) ? n - i : chunk);
    }
}

static int file_eq(const char *name, const char *expected)
{
    uint32_t size = 0;
    const uint8_t *d = fake_fs_data(name, &size);
    return d != NULL && size == strlen(expected) && memcmp(d, expected, size) == 0;
}

/* ----------------------------------------------------------------- сканер --- */

static void test_scanner(void)
{
    purrgo_gpx_scan_t s;
    char fix[160];

    /* Целый файл - не сломан. */
    scan_all(&s, HEADER PT1 PT2 FOOTER, 512);
    CHECK(!purrgo_gpx_scan_is_broken(&s));

    /* Нет футера целиком: режем после последней точки (перевод строки после неё отбрасывается). */
    const char *no_footer = HEADER PT1 PT2;
    scan_all(&s, no_footer, 512);
    CHECK(purrgo_gpx_scan_is_broken(&s));
    CHECK(purrgo_gpx_scan_cut_offset(&s) == strlen(no_footer) - 1);
    CHECK(purrgo_gpx_scan_fix_text(&s, fix, sizeof fix) > 0);
    CHECK(strcmp(fix, FIXED_FOOTER) == 0);

    /* Обрыв посреди точки: теряется только неполная точка. */
    const char *mid = HEADER PT1 "<trkpt lat=\"53.1001000\" lon=\"28.20";
    scan_all(&s, mid, 512);
    CHECK(purrgo_gpx_scan_is_broken(&s));
    CHECK(purrgo_gpx_scan_cut_offset(&s) == strlen(HEADER PT1) - 1);

    /* Обрыв после открывающего <trkpt ...> и внутри его вложенных тегов. */
    scan_all(&s, HEADER PT1 "<trkpt lat=\"1\" lon=\"2\"><ele>12</ele><time>2026-", 512);
    CHECK(purrgo_gpx_scan_is_broken(&s));
    CHECK(purrgo_gpx_scan_cut_offset(&s) == strlen(HEADER PT1) - 1);
    scan_all(&s, HEADER PT1 "<trkpt lat=\"1\" lon=\"2\"><extensions><gpxtpx:TrackPointExtension>", 512);
    CHECK(purrgo_gpx_scan_cut_offset(&s) == strlen(HEADER PT1) - 1);

    /* Обрыв посреди футера. */
    scan_all(&s, HEADER PT1 "</trkseg></tr", 512);
    CHECK(purrgo_gpx_scan_is_broken(&s));
    CHECK(purrgo_gpx_scan_cut_offset(&s) == strlen(HEADER PT1 "</trkseg>"));
    CHECK(purrgo_gpx_scan_fix_text(&s, fix, sizeof fix) > 0);
    CHECK(strcmp(fix, "\n</trk>\n</gpx>\n") == 0);
    scan_all(&s, HEADER PT1 FOOTER "x", 512);       /* </gpx> уже был */
    CHECK(!purrgo_gpx_scan_is_broken(&s));
    scan_all(&s, HEADER PT1 "</trkseg></trk>\n</gp", 512);
    CHECK(purrgo_gpx_scan_is_broken(&s));
    CHECK(purrgo_gpx_scan_fix_text(&s, fix, sizeof fix) > 0);
    CHECK(strcmp(fix, "\n</gpx>\n") == 0);

    /* Только заголовок: получится пустой, но корректный трек. */
    scan_all(&s, HEADER, 512);
    CHECK(purrgo_gpx_scan_is_broken(&s));
    CHECK(purrgo_gpx_scan_fix_text(&s, fix, sizeof fix) > 0);
    CHECK(strcmp(fix, FIXED_FOOTER) == 0);

    /* Оборван сам открывающий <gpx ...>: нечего чинить. */
    scan_all(&s, "<?xml version=\"1.0\"?>\n<gpx version=\"1.1\" creat", 512);
    CHECK(!purrgo_gpx_scan_is_broken(&s));

    /* Пустой файл, не GPX, нули после сбоя питания, самозакрытый корень. */
    scan_all(&s, "", 512);
    CHECK(!purrgo_gpx_scan_is_broken(&s));
    scan_all(&s, "<html><body><p>hi</p>", 512);
    CHECK(!purrgo_gpx_scan_is_broken(&s));
    scan_all(&s, "<gpx/>", 512);
    CHECK(!purrgo_gpx_scan_is_broken(&s));

    /* Маршрут (rte/rtept) и путевые точки (wpt) режутся по своим границам. */
    const char *rte = "<gpx><rte><rtept lat=\"1\" lon=\"2\"/><rtept lat=\"3\" lon=\"4\"><name>A</name></rtept><rtept lat=\"5\"";
    scan_all(&s, rte, 512);
    CHECK(purrgo_gpx_scan_is_broken(&s));
    CHECK(purrgo_gpx_scan_cut_offset(&s) ==
          strlen("<gpx><rte><rtept lat=\"1\" lon=\"2\"/><rtept lat=\"3\" lon=\"4\"><name>A</name></rtept>"));
    CHECK(purrgo_gpx_scan_fix_text(&s, fix, sizeof fix) > 0);
    CHECK(strcmp(fix, "\n</rte>\n</gpx>\n") == 0);
    scan_all(&s, "<gpx><wpt lat=\"1\" lon=\"2\"><name>A</name></wpt><wpt lat=\"3\"><na", 512);
    CHECK(purrgo_gpx_scan_fix_text(&s, fix, sizeof fix) > 0);
    CHECK(strcmp(fix, "\n</gpx>\n") == 0);

    /* Результат не зависит от размера фрагментов (в том числе по 1 байту: граница внутри тега). */
    const char *text = HEADER PT1 PT2 "<trkpt lat=\"53.1002000\" lon=\"28.20";
    purrgo_gpx_scan_t a, b;
    scan_all(&a, text, 1);
    scan_all(&b, text, 7);
    CHECK(a.cut == b.cut && a.cut_depth == b.cut_depth && a.pos == b.pos);
    CHECK(purrgo_gpx_scan_is_broken(&a) && purrgo_gpx_scan_is_broken(&b));

    /* Слишком длинное имя незакрытого элемента: чинить не берёмся. */
    scan_all(&s, "<gpx><averyveryverylongelementname><trk>", 512);
    CHECK(!purrgo_gpx_scan_is_broken(&s));
}

/* --------------------------------------------------- открытие трека на SD --- */

static void test_load_repairs_file(void)
{
    reset();
    fake_fs_put(PATH, HEADER PT1 PT2 PT3);                 /* оборван без футера */
    CHECK(purrgo_route_load_src(NAME, PURRGO_ROUTE_SRC_TRACKS));
    CHECK(file_eq(PATH, HEADER PT1 PT2 "<trkpt lat=\"53.1002000\" lon=\"28.2002000\"><ele>102</ele>"
                        "<time>2026-11-16T08:12:10Z</time></trkpt>" FIXED_FOOTER));
    CHECK(fake_fs_open_handles() == 0);

    /* Повторное открытие: файл уже целый, ничего не пишется. */
    const int opens = fake_fs_open_calls();
    CHECK(purrgo_route_load_src(NAME, PURRGO_ROUTE_SRC_TRACKS));
    CHECK(fake_fs_open_calls() == opens + 1);              /* только чтение */
    uint32_t size = 0;
    (void)fake_fs_data(PATH, &size);
    CHECK(size > 0);
    CHECK(purrgo_route_entry_count() == 3);                /* 3 точки, без разрывов в конце */
}

static void test_load_cuts_partial_point(void)
{
    reset();
    fake_fs_put(PATH, HEADER PT1 PT2 "<trkpt lat=\"53.1002000\" lon=\"28.20");
    CHECK(purrgo_route_load_src(NAME, PURRGO_ROUTE_SRC_TRACKS));
    /* Оборванной точки в файле больше нет, перед футером - целая PT2 без лишнего мусора. */
    uint32_t size = 0;
    const uint8_t *d = fake_fs_data(PATH, &size);
    CHECK(d != NULL);
    CHECK(file_eq(PATH, HEADER PT1
                        "<trkpt lat=\"53.1001000\" lon=\"28.2001000\"><ele>101</ele><time>2026-11-16T08:12:05Z</time></trkpt>"
                        FIXED_FOOTER));
    purrgo_gpx_scan_t s;
    scan_all(&s, (const char *)d, 512);
    CHECK(!purrgo_gpx_scan_is_broken(&s));
}

static void test_load_cut_in_footer_and_nuls(void)
{
    reset();
    fake_fs_put(PATH, HEADER PT1 PT2 "</trkseg></tr");
    CHECK(purrgo_route_load_src(NAME, PURRGO_ROUTE_SRC_TRACKS));
    CHECK(file_eq(PATH, HEADER PT1 PT2 "</trkseg>\n</trk>\n</gpx>\n"));

    /* Нули после последней точки (SD не дописала сектор). */
    reset();
    static uint8_t buf[600];
    const char *good = HEADER PT1 PT2;
    const size_t n = strlen(good);
    memcpy(buf, good, n);
    memset(buf + n, 0, 40);
    fake_fs_put_bin(PATH, buf, (uint32_t)(n + 40));
    CHECK(purrgo_route_load_src(NAME, PURRGO_ROUTE_SRC_TRACKS));
    uint32_t size = 0;
    const uint8_t *d = fake_fs_data(PATH, &size);
    CHECK(d != NULL && memchr(d, 0, size) == NULL);
    purrgo_gpx_scan_t s;
    scan_all(&s, (const char *)d, 512);
    CHECK(!purrgo_gpx_scan_is_broken(&s));
}

static void test_healthy_and_foreign_untouched(void)
{
    reset();
    fake_fs_put(PATH, HEADER PT1 PT2 FOOTER);
    CHECK(purrgo_route_load_src(NAME, PURRGO_ROUTE_SRC_TRACKS));
    CHECK(file_eq(PATH, HEADER PT1 PT2 FOOTER));

    fake_fs_put("TRACKS/A.gpx", "<html><body>trkpt");
    purrgo_route_load_src("A.gpx", PURRGO_ROUTE_SRC_TRACKS);
    CHECK(file_eq("TRACKS/A.gpx", "<html><body>trkpt"));

    /* Файл маршрута (ROUTES) без футера не меняется: это чужие файлы. */
    fake_fs_put("routes/R.gpx", HEADER PT1 PT2);
    CHECK(purrgo_route_load_src("R.gpx", PURRGO_ROUTE_SRC_ROUTES));
    CHECK(file_eq("routes/R.gpx", HEADER PT1 PT2));
}

static void test_active_file_untouched(void)
{
    reset();
    purrgo_gnss_solution_t f;
    memset(&f, 0, sizeof f);
    f.valid = true; f.year = 26; f.month = 11; f.day = 16;
    f.hours = 8; f.minutes = 12; f.seconds = 0;
    f.lat_1e7 = 537000000; f.lon_1e7 = 284000000;
    CHECK(purrgo_logger_start(&f));

    const char *active = purrgo_logger_get_active_filename();
    CHECK(active != NULL);
    const char *name = strrchr(active, '/');
    name = name ? name + 1 : active;

    uint32_t before = 0;
    (void)fake_fs_data(active, &before);
    purrgo_route_load_src(name, PURRGO_ROUTE_SRC_TRACKS);   /* в меню так не открыть, но защита должна быть */
    uint32_t after = 0;
    (void)fake_fs_data(active, &after);
    CHECK(before == after);
    purrgo_logger_reset();
}

static void test_failures(void)
{
    /* Запись не открылась: маршрут всё равно загружен, повторных попыток нет. */
    reset();
    fake_fs_put(PATH, HEADER PT1 PT2);
    g_fault.fail_open_append = 1;
    CHECK(purrgo_route_load_src(NAME, PURRGO_ROUTE_SRC_TRACKS));
    CHECK(file_eq(PATH, HEADER PT1 PT2));
    const int opens = fake_fs_open_calls();
    CHECK(purrgo_route_load_src(NAME, PURRGO_ROUTE_SRC_TRACKS));
    CHECK(fake_fs_open_calls() == opens + 1);              /* только чтение, без новой записи */
    CHECK(fake_fs_open_handles() == 0);

    /* Не удалось обрезать: файл не изменён, слот освобождён. */
    reset();
    fake_fs_put(PATH, HEADER PT1 PT2);
    g_fault.fail_truncate = 1;
    CHECK(purrgo_route_load_src(NAME, PURRGO_ROUTE_SRC_TRACKS));
    CHECK(file_eq(PATH, HEADER PT1 PT2));
    CHECK(fake_fs_open_handles() == 0);

    /* Сбой sync / close - файл всё равно не остаётся открытым. */
    reset();
    fake_fs_put(PATH, HEADER PT1 PT2);
    g_fault.fail_sync = 1;
    CHECK(purrgo_route_load_src(NAME, PURRGO_ROUTE_SRC_TRACKS));
    CHECK(fake_fs_open_handles() == 0);
}

int main(void)
{
    test_scanner();
    test_load_repairs_file();
    test_load_cuts_partial_point();
    test_load_cut_in_footer_and_nuls();
    test_healthy_and_foreign_untouched();
    test_active_file_untouched();
    test_failures();
    return test_finish("test_gpx_repair");
}
