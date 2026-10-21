/*
 * Host-тесты purrgo_time и track_logger на границах (патчи 0009, 0014-0016):
 * високосные годы, конец диапазона 2000..2099, переход через полночь и Новый год
 * с часовым поясом, год < 26 (до эпохи имён треков), прореживание точек,
 * кольцевой RAM-буфер трека, имена файлов треков. Известные дефекты раздела 2 помечены XFAIL.
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/purrgo_time.h"
#include "purrgo/track_logger.h"
#include "purrgo/config.h"
#include "fake_fs.h"
#include "test_common.h"

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

/* ----------------------------------------------------------- purrgo_time --- */

static void test_time_calendar(void)
{
    CHECK(purrgo_time_is_leap_year(0));      /* 2000 */
    CHECK(purrgo_time_is_leap_year(4));
    CHECK(purrgo_time_is_leap_year(24));
    CHECK(purrgo_time_is_leap_year(96));
    CHECK(!purrgo_time_is_leap_year(1));
    CHECK(!purrgo_time_is_leap_year(26));
    CHECK(!purrgo_time_is_leap_year(99));

    CHECK(purrgo_time_days_in_month(2, 28) == 29);
    CHECK(purrgo_time_days_in_month(2, 26) == 28);
    CHECK(purrgo_time_days_in_month(4, 26) == 30);
    CHECK(purrgo_time_days_in_month(6, 26) == 30);
    CHECK(purrgo_time_days_in_month(9, 26) == 30);
    CHECK(purrgo_time_days_in_month(11, 26) == 30);
    CHECK(purrgo_time_days_in_month(1, 26) == 31);
    CHECK(purrgo_time_days_in_month(12, 26) == 31);
}

static void test_time_epoch_edges(void)
{
    uint32_t e = 123;
    CHECK(purrgo_time_datetime_to_epoch(0, 1, 1, 0, 0, 0, &e) && e == 0);
    CHECK(purrgo_time_datetime_to_epoch(26, 1, 1, 0, 0, 0, &e) && e == 820540800UL);
    CHECK(purrgo_time_datetime_to_epoch(28, 2, 29, 12, 0, 0, &e) && e == 888753600UL);
    CHECK(purrgo_time_datetime_to_epoch(99, 12, 31, 23, 59, 59, &e) && e == 3155759999UL);

    /* Валидация: каждое поле на границе допустимого и сразу за ней. */
    CHECK(!purrgo_time_datetime_to_epoch(100, 1, 1, 0, 0, 0, &e));
    CHECK(!purrgo_time_datetime_to_epoch(26, 0, 1, 0, 0, 0, &e));
    CHECK(!purrgo_time_datetime_to_epoch(26, 13, 1, 0, 0, 0, &e));
    CHECK(!purrgo_time_datetime_to_epoch(26, 1, 0, 0, 0, 0, &e));
    CHECK(!purrgo_time_datetime_to_epoch(26, 1, 32, 0, 0, 0, &e));
    CHECK(!purrgo_time_datetime_to_epoch(26, 4, 31, 0, 0, 0, &e));
    CHECK(!purrgo_time_datetime_to_epoch(26, 2, 29, 0, 0, 0, &e));   /* 2026 не високосный */
    CHECK(purrgo_time_datetime_to_epoch(28, 2, 29, 0, 0, 0, &e));
    CHECK(!purrgo_time_datetime_to_epoch(28, 2, 30, 0, 0, 0, &e));
    CHECK(!purrgo_time_datetime_to_epoch(26, 1, 1, 24, 0, 0, &e));
    CHECK(!purrgo_time_datetime_to_epoch(26, 1, 1, 0, 60, 0, &e));
    CHECK(!purrgo_time_datetime_to_epoch(26, 1, 1, 0, 0, 60, &e));
    CHECK(purrgo_time_datetime_to_epoch(26, 1, 1, 0, 0, 0, NULL));   /* epoch_out может быть NULL */

    uint8_t y, mo, d, h, mi, s;
    CHECK(!purrgo_time_epoch_to_datetime(3155760000UL, &y, &mo, &d, &h, &mi, &s));
    CHECK(!purrgo_time_epoch_to_datetime(0xFFFFFFFFUL, &y, &mo, &d, &h, &mi, &s));
    CHECK(purrgo_time_epoch_to_datetime(3155759999UL, &y, &mo, &d, &h, &mi, &s));
    CHECK(y == 99 && mo == 12 && d == 31 && h == 23 && mi == 59 && s == 59);
    CHECK(purrgo_time_epoch_to_datetime(0, &y, &mo, &d, &h, &mi, &s));
    CHECK(y == 0 && mo == 1 && d == 1 && h == 0 && mi == 0 && s == 0);
}

/* Туда-обратно: «шаг» подобран так, чтобы попадать в разные часы/дни/месяцы/годы. */
static void test_time_roundtrip(void)
{
    uint32_t bad = 0, n = 0;
    for (uint64_t e = 0; e <= 3155759999ULL; e += 86400ULL * 7ULL + 3601ULL) {
        uint8_t y, mo, d, h, mi, s;
        uint32_t back = 0;
        if (!purrgo_time_epoch_to_datetime((uint32_t)e, &y, &mo, &d, &h, &mi, &s) ||
            !purrgo_time_datetime_to_epoch(y, mo, d, h, mi, s, &back) || back != (uint32_t)e) {
            bad++;
        }
        n++;
    }
    CHECK(n > 4000);
    CHECK(bad == 0);

    /* Каждая секунда вокруг полуночи, конца месяца, високосного дня и конца диапазона. */
    static const uint32_t anchors[] = { 0, 86399UL, 888753600UL - 43200UL, 820540800UL - 1UL, 3155759999UL - 5UL };
    bad = 0;
    for (size_t a = 0; a < sizeof anchors / sizeof anchors[0]; a++) {
        for (uint32_t e = anchors[a]; e < anchors[a] + 100000UL && e <= 3155759999UL; e += 997) {
            uint8_t y, mo, d, h, mi, s; uint32_t back = 0;
            if (!purrgo_time_epoch_to_datetime(e, &y, &mo, &d, &h, &mi, &s) ||
                !purrgo_time_datetime_to_epoch(y, mo, d, h, mi, s, &back) || back != e) { bad++; }
        }
    }
    CHECK(bad == 0);
}

static purrgo_gnss_solution_t fixt(uint8_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi, uint8_t s)
{
    purrgo_gnss_solution_t f;
    memset(&f, 0, sizeof f);
    f.valid = true; f.year = y; f.month = mo; f.day = d; f.hours = h; f.minutes = mi; f.seconds = s;
    f.lat_1e7 = 537000000; f.lon_1e7 = 284000000; f.alt_m = 100;
    f.speed_knots = 500;                /* 5 уз: движемся (порог стоянки - 0,5 уз) */
    return f;
}

static void test_time_timezone(void)
{
    purrgo_gnss_solution_t in, out;

    in = fixt(26, 10, 19, 23, 30, 0);                           /* через полночь вперёд */
    CHECK(purrgo_time_apply_timezone(&in, &out, 180));
    CHECK(out.year == 26 && out.month == 10 && out.day == 20 && out.hours == 2 && out.minutes == 30);

    in = fixt(26, 10, 19, 0, 10, 0);                            /* через полночь назад */
    CHECK(purrgo_time_apply_timezone(&in, &out, -60));
    CHECK(out.day == 18 && out.hours == 23 && out.minutes == 10);

    in = fixt(26, 12, 31, 23, 30, 0);                           /* Новый год */
    CHECK(purrgo_time_apply_timezone(&in, &out, 60));
    CHECK(out.year == 27 && out.month == 1 && out.day == 1 && out.hours == 0 && out.minutes == 30);

    in = fixt(28, 2, 28, 23, 0, 0);                             /* 29 февраля високосного года */
    CHECK(purrgo_time_apply_timezone(&in, &out, 120));
    CHECK(out.month == 2 && out.day == 29 && out.hours == 1);
    in = fixt(26, 2, 28, 23, 0, 0);                             /* ... и 1 марта обычного */
    CHECK(purrgo_time_apply_timezone(&in, &out, 120));
    CHECK(out.month == 3 && out.day == 1);

    in = fixt(26, 6, 1, 12, 0, 0);                              /* крайние пояса UTC-12 / UTC+14 */
    CHECK(purrgo_time_apply_timezone(&in, &out, -12 * 60) && out.hours == 0);
    CHECK(purrgo_time_apply_timezone(&in, &out, 14 * 60) && out.day == 2 && out.hours == 2);

    in = fixt(0, 1, 1, 0, 0, 0);                                /* за нижней границей диапазона */
    CHECK(!purrgo_time_apply_timezone(&in, &out, -60));
    in = fixt(99, 12, 31, 23, 59, 59);                          /* за верхней границей */
    CHECK(!purrgo_time_apply_timezone(&in, &out, 1));
    CHECK(purrgo_time_apply_timezone(&in, &out, 0));

    in = fixt(26, 13, 1, 0, 0, 0);                              /* некорректная дата */
    CHECK(!purrgo_time_apply_timezone(&in, &out, 0));
    in = fixt(26, 6, 1, 0, 0, 0); in.valid = false;             /* нет фикса */
    CHECK(!purrgo_time_apply_timezone(&in, &out, 0));
}

/* --------------------------------------------------------- track_logger --- */

static int count_str(const char *hay, const char *needle)
{
    int c = 0;
    for (const char *p = hay; (p = strstr(p, needle)); p += strlen(needle)) { c++; }
    return c;
}

/* Ожидаемое имя трека: UTC-секунды от 2000 года, 8 hex-цифр. */
static void tname(char out[32], uint8_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi, uint8_t sec)
{
    uint32_t e = 0;
    CHECK(purrgo_time_datetime_to_epoch(y, mo, d, h, mi, sec, &e));
    snprintf(out, 32, "TRACKS/%08X.gpx", (unsigned int)e);
}

static void logger_reset(int tz_minutes)
{
    purrgo_logger_stop();
    fake_fs_reset();
    app_config.tz_offset_minutes = (int16_t)tz_minutes;
    purrgo_logger_set_mode(LOGGER_MODE_STANDARD);
}

static char *file_text(const char *name)
{
    uint32_t n;
    const uint8_t *d = fake_fs_data(name, &n);
    if (!d) { return NULL; }
    char *t = malloc((size_t)n + 1u);
    memcpy(t, d, n);
    t[n] = 0;
    return t;
}

static void test_logger_filename_and_gpx(void)
{
    logger_reset(0);
    char nm[32];
    tname(nm, 26, 10, 19, 12, 0, 0);
    purrgo_gnss_solution_t f = fixt(26, 10, 19, 12, 0, 0);
    CHECK(purrgo_logger_start(&f));
    CHECK(strcmp(purrgo_logger_get_active_filename(), nm) == 0);
    CHECK(purrgo_logger_add_point(&f));                           /* первая точка пишется всегда */
    purrgo_logger_stop();

    char *t = file_text(nm);
    CHECK(t != NULL);
    if (t) {
        CHECK(strncmp(t, "<?xml", 5) == 0);
        CHECK(count_str(t, "<trkpt") == 1 && count_str(t, "</trkpt>") == 1);
        CHECK(strstr(t, "lat=\"53.7000000\"") != NULL && strstr(t, "lon=\"28.4000000\"") != NULL);
        CHECK(strstr(t, "<ele>100</ele>") != NULL);
        CHECK(strstr(t, "<time>2026-10-19T12:00:00Z</time>") != NULL);
        CHECK(strstr(t, "</gpx>") != NULL);
        free(t);
    }
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);
    CHECK(purrgo_logger_get_active_filename() == NULL);
    CHECK(fake_fs_open_handles() == 0);

    /* Южное и западное полушария в GPX. */
    logger_reset(0);
    f = fixt(26, 10, 19, 12, 0, 0);
    f.lat_1e7 = -338688000; f.lon_1e7 = -1512050000;
    CHECK(purrgo_logger_start(&f) && purrgo_logger_add_point(&f));
    purrgo_logger_stop();
    t = file_text(nm);
    CHECK(t && strstr(t, "lat=\"-33.8688000\"") && strstr(t, "lon=\"-151.2050000\""));
    free(t);
}

static void test_logger_invalid_inputs(void)
{
    logger_reset(0);
    purrgo_gnss_solution_t f = fixt(26, 13, 40, 25, 61, 61);       /* мусорная дата */
    CHECK(!purrgo_logger_start(&f));
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);

    f = fixt(26, 10, 19, 12, 0, 0); f.valid = false;                /* нет фикса */
    CHECK(!purrgo_logger_start(&f));
    CHECK(!purrgo_logger_start(NULL));
    CHECK(!purrgo_logger_add_point(NULL));

    f = fixt(26, 10, 19, 12, 0, 0);
    CHECK(!purrgo_logger_add_point(&f));                            /* логгер не запущен */

    CHECK(purrgo_logger_start(&f));
    purrgo_gnss_solution_t bad = fixt(26, 2, 30, 12, 0, 1);         /* 30 февраля */
    CHECK(!purrgo_logger_add_point(&bad));
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    CHECK(!purrgo_logger_start(&f));                                /* повторный старт игнорируется */
    purrgo_logger_stop();
    purrgo_logger_stop();                                           /* повторный stop безопасен */

    purrgo_logger_set_mode(LOGGER_MODE_OFF);
    CHECK(!purrgo_logger_start(&f));
    purrgo_logger_set_mode(LOGGER_MODE_STANDARD);
    CHECK(purrgo_logger_start(&f));
    purrgo_logger_set_mode(LOGGER_MODE_OFF);                        /* OFF во время записи останавливает её */
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);
    CHECK(fake_fs_open_handles() == 0);
}

static void test_logger_decimation(void)
{
    logger_reset(0);
    purrgo_gnss_solution_t f = fixt(26, 10, 19, 12, 0, 0);
    CHECK(purrgo_logger_start(&f));
    CHECK(purrgo_logger_add_point(&f));                             /* #1 */

    f = fixt(26, 10, 19, 12, 0, 1); f.lat_1e7 += 300;               /* ~3,3 м */
    CHECK(!purrgo_logger_add_point(&f));
    f = fixt(26, 10, 19, 12, 0, 2); f.lat_1e7 += 600;               /* ~6,7 м */
    CHECK(purrgo_logger_add_point(&f));                             /* #2 */
    f = fixt(26, 10, 19, 12, 0, 3); f.lat_1e7 += 600 + 100;         /* ~0,8 м от #2 */
    CHECK(!purrgo_logger_add_point(&f));

    /* Стоим на месте: точка по времени, ровно через 300 с (UTC-фильтр). */
    f = fixt(26, 10, 19, 12, 4, 59); f.lat_1e7 += 600;
    CHECK(!purrgo_logger_add_point(&f));
    f = fixt(26, 10, 19, 12, 5, 2); f.lat_1e7 += 600;
    CHECK(purrgo_logger_add_point(&f));                             /* #3 */
    CHECK(purrgo_logger_get_track_point_count() == 3);
    purrgo_logger_stop();

    /* EXPEDITION: 100 м или 900 с. */
    logger_reset(0);
    purrgo_logger_set_mode(LOGGER_MODE_EXPEDITION);
    f = fixt(26, 10, 19, 12, 0, 0);
    CHECK(purrgo_logger_start(&f) && purrgo_logger_add_point(&f));
    f = fixt(26, 10, 19, 12, 0, 10); f.lat_1e7 += 5000;             /* ~55 м */
    CHECK(!purrgo_logger_add_point(&f));
    f = fixt(26, 10, 19, 12, 0, 20); f.lat_1e7 += 10000;            /* ~111 м */
    CHECK(purrgo_logger_add_point(&f));
    f = fixt(26, 10, 19, 12, 14, 0); f.lat_1e7 += 10000;            /* 13:40 после последней */
    CHECK(!purrgo_logger_add_point(&f));
    f = fixt(26, 10, 19, 12, 15, 21); f.lat_1e7 += 10000;           /* > 900 с */
    CHECK(purrgo_logger_add_point(&f));
    purrgo_logger_stop();
    purrgo_logger_set_mode(LOGGER_MODE_STANDARD);
}

static void test_logger_midnight(void)
{
    /* Часовой пояс +3: локальная полночь = 21:00 UTC. */
    logger_reset(180);
    purrgo_gnss_solution_t f = fixt(26, 10, 19, 20, 59, 50);          /* локально 23:59:50 */
    CHECK(purrgo_logger_start(&f) && purrgo_logger_add_point(&f));
    char n_old[32], n_new[32];
    tname(n_old, 26, 10, 19, 20, 59, 50);
    tname(n_new, 26, 10, 19, 21, 0, 5);
    CHECK(strcmp(purrgo_logger_get_active_filename(), n_old) == 0);

    purrgo_gnss_solution_t g = fixt(26, 10, 19, 20, 59, 58); g.lat_1e7 += 1000;
    CHECK(purrgo_logger_add_point(&g));
    size_t before = purrgo_logger_get_track_point_count();
    CHECK(before == 2);

    purrgo_gnss_solution_t h = fixt(26, 10, 19, 21, 0, 5); h.lat_1e7 += 2000;   /* локально 00:00:05 */
    CHECK(purrgo_logger_add_point(&h));                                         /* первая точка нового дня */
    CHECK(strcmp(purrgo_logger_get_active_filename(), n_new) == 0);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);

    char *old = file_text(n_old);
    CHECK(old && strstr(old, "</gpx>") && count_str(old, "<trkpt") == 2);       /* старый файл закрыт корректно */
    free(old);
    CHECK(fake_fs_open_handles() == 1);

    /* При смене суток RAM-буфер не очищается: нарисованный трек остаётся на экране. */
    CHECK(purrgo_logger_get_track_point_count() == before + 1);

    purrgo_logger_stop();
    char *neu = file_text(n_new);
    CHECK(neu && strstr(neu, "</gpx>") && count_str(neu, "<trkpt") == 1);
    free(neu);
    CHECK(fake_fs_open_handles() == 0);
}

static void test_logger_year_before_2026(void)
{
    logger_reset(0);
    purrgo_gnss_solution_t f = fixt(25, 6, 1, 12, 0, 0);
    char exp25[32], exp26[32], exp26_midnight[32];
    tname(exp25, 25, 6, 1, 12, 0, 0);
    CHECK(purrgo_logger_start(&f));                                   /* не падает, пишет файл */
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    const char *n25 = purrgo_logger_get_active_filename();
    CHECK(n25 != NULL && strcmp(n25, exp25) == 0);
    char name25[64];
    snprintf(name25, sizeof name25, "%s", n25 ? n25 : "");
    purrgo_logger_stop();

    f = fixt(26, 1, 1, 12, 0, 0);
    tname(exp26, 26, 1, 1, 12, 0, 0);
    CHECK(purrgo_logger_start(&f));
    char name26[64];
    snprintf(name26, sizeof name26, "%s", purrgo_logger_get_active_filename());
    purrgo_logger_stop();
    CHECK(strcmp(name26, exp26) == 0);

    /* Имя не переполняется: старые треки сортируются раньше новых. */
    CHECK(strcmp(name25, name26) < 0);

    /* Имя не зависит от часового пояса (раньше - местное время). */
    logger_reset(180);
    f = fixt(26, 1, 1, 12, 0, 0);
    CHECK(purrgo_logger_start(&f));
    CHECK(strcmp(purrgo_logger_get_active_filename(), exp26) == 0);
    purrgo_logger_stop();

    /* Эпоха 01.01.2026 00:00 UTC = 820540800 (0x30..): прежняя константа была на сутки меньше. */
    logger_reset(0);
    f = fixt(26, 1, 1, 0, 0, 0);
    tname(exp26_midnight, 26, 1, 1, 0, 0, 0);
    CHECK(purrgo_logger_start(&f));
    CHECK(strcmp(purrgo_logger_get_active_filename(), exp26_midnight) == 0);
    purrgo_logger_stop();
}

static void test_logger_same_name_overwrite(void)
{
    logger_reset(0);
    char nm[32], nm2[32], nm3[32];
    tname(nm, 26, 10, 19, 12, 0, 0);
    tname(nm2, 26, 10, 19, 12, 0, 1);
    tname(nm3, 26, 10, 19, 12, 0, 2);
    purrgo_gnss_solution_t f = fixt(26, 10, 19, 12, 0, 0);
    CHECK(purrgo_logger_start(&f) && purrgo_logger_add_point(&f));
    purrgo_logger_stop();
    char *first = file_text(nm);
    CHECK(first && count_str(first, "<trkpt") == 1);

    /* Повторный старт в ту же секунду (перезапуск после сбоя): существующий файл не затирается. */
    CHECK(purrgo_logger_start(&f));
    CHECK(strcmp(purrgo_logger_get_active_filename(), nm2) == 0);
    purrgo_logger_stop();
    char *again = file_text(nm);
    CHECK(again && count_str(again, "<trkpt") == 1);
    char *second = file_text(nm2);
    CHECK(second != NULL);
    free(second);

    /* И ещё раз: берётся следующий свободный номер. */
    CHECK(purrgo_logger_start(&f));
    CHECK(strcmp(purrgo_logger_get_active_filename(), nm3) == 0);
    purrgo_logger_stop();

    /* Все подряд идущие имена заняты: старт отклоняется, существующие файлы целы. */
    logger_reset(0);
    for (uint32_t k = 0; k < 8; k++) {
        char n[32];
        tname(n, 26, 10, 19, 12, 0, (uint8_t)k);
        fake_fs_put(n, "x");
    }
    CHECK(!purrgo_logger_start(&f));
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_ERROR);
    CHECK(fake_fs_open_handles() == 0);
    free(first);
    free(again);
}

static void test_logger_ram_ring(void)
{
    logger_reset(0);
    purrgo_gnss_solution_t f = fixt(26, 10, 19, 12, 0, 0);
    CHECK(purrgo_logger_start(&f));

    const int total = (int)TRACK_RAM_MAX_POINTS + 76;                 /* с запасом за границу кольца */
    int recorded = 0;
    for (int i = 0; i < total; i++) {
        f = fixt(26, 10, 19, 12, 0, (uint8_t)(i % 60));
        f.lat_1e7 = 537000000 + i * 1000;                             /* ~11 м шаг */
        if (purrgo_logger_add_point(&f)) { recorded++; }
    }
    CHECK(recorded == total);
    CHECK(purrgo_logger_get_track_point_count() == TRACK_RAM_MAX_POINTS);

    track_point_t p;
    CHECK(purrgo_logger_get_track_point(0, &p) && p.lat_1e7 == 537000000 + (total - (int)TRACK_RAM_MAX_POINTS) * 1000);
    CHECK(purrgo_logger_get_track_point(TRACK_RAM_MAX_POINTS - 1, &p) && p.lat_1e7 == 537000000 + (total - 1) * 1000);
    CHECK(!purrgo_logger_get_track_point(TRACK_RAM_MAX_POINTS, &p));
    CHECK(!purrgo_logger_get_track_point(0, NULL));

    track_point_t prev, last;
    CHECK(purrgo_logger_get_last_two_points(&prev, &last));
    CHECK(last.lat_1e7 == 537000000 + (total - 1) * 1000 && prev.lat_1e7 == last.lat_1e7 - 1000);
    CHECK(!purrgo_logger_get_last_two_points(NULL, &last));

    purrgo_logger_stop();
    CHECK(purrgo_logger_get_track_point_count() == 0);
    CHECK(!purrgo_logger_get_last_two_points(&prev, &last));
}

/* f_sync - не на каждый сброс буфера, а раз в 50 точек или 60 с. */
static void test_logger_sync_cadence(void)
{
    logger_reset(0);
    purrgo_gnss_solution_t f = fixt(26, 10, 19, 12, 0, 0);
    CHECK(purrgo_logger_start(&f));
    int i;
    for (i = 0; i < 49; i++) {                       /* 49 точек за 49 с: буфер сбрасывался, sync - нет */
        f = fixt(26, 10, 19, 12, 0, (uint8_t)i); f.lat_1e7 += i * 1000;
        CHECK(purrgo_logger_add_point(&f));
    }
    CHECK(fake_fs_sync_calls() == 0);
    f = fixt(26, 10, 19, 12, 0, 49); f.lat_1e7 += 49 * 1000;
    CHECK(purrgo_logger_add_point(&f));              /* 50-я точка */
    CHECK(fake_fs_sync_calls() == 1);

    f = fixt(26, 10, 19, 12, 1, 20); f.lat_1e7 += 60 * 1000;     /* 31 с после sync */
    CHECK(purrgo_logger_add_point(&f));
    CHECK(fake_fs_sync_calls() == 1);
    f = fixt(26, 10, 19, 12, 2, 0); f.lat_1e7 += 61 * 1000;      /* 71 с после sync */
    CHECK(purrgo_logger_add_point(&f));
    CHECK(fake_fs_sync_calls() == 2);

    char name[64];
    snprintf(name, sizeof name, "%s", purrgo_logger_get_active_filename());
    purrgo_logger_stop();
    char *t = file_text(name);                       /* всё записано и закрыто футером */
    CHECK(t && strstr(t, "</gpx>") && count_str(t, "<trkpt") == 52);
    free(t);
}

/* Фильтры качества фикса, шума на стоянке, поворотов и выбросов. */
static void test_logger_filters(void)
{
    purrgo_gnss_solution_t f;

    /* Качество: 2D-фикс, HDOP > 5, меньше 4 спутников - не пишем; 0 = неизвестно - пишем. */
    logger_reset(0);
    f = fixt(26, 10, 19, 12, 0, 0);
    CHECK(purrgo_logger_start(&f));
    f.fix_type = 2;                       CHECK(!purrgo_logger_add_point(&f));
    f.fix_type = 1;                       CHECK(!purrgo_logger_add_point(&f));
    f.fix_type = 3; f.hdop_100 = 501;     CHECK(!purrgo_logger_add_point(&f));
    f.hdop_100 = 500; f.satellites_tracked = 3;   CHECK(!purrgo_logger_add_point(&f));
    CHECK(purrgo_logger_get_track_point_count() == 0);
    f.satellites_tracked = 4;             CHECK(purrgo_logger_add_point(&f));      /* граница: пишем */
    CHECK(purrgo_logger_get_track_point_count() == 1);
    purrgo_logger_stop();

    logger_reset(0);
    f = fixt(26, 10, 19, 12, 0, 0);       /* fix_type = hdop = sats = 0: приёмник их не присылает */
    CHECK(purrgo_logger_start(&f) && purrgo_logger_add_point(&f));
    purrgo_logger_stop();

    /* Стоянка: дрейф на 6-7 м при скорости < 0,5 уз не пишется, точка по таймауту - пишется. */
    logger_reset(0);
    f = fixt(26, 10, 19, 12, 0, 0);
    CHECK(purrgo_logger_start(&f) && purrgo_logger_add_point(&f));
    f = fixt(26, 10, 19, 12, 0, 10); f.lat_1e7 += 700; f.speed_knots = 49;
    CHECK(!purrgo_logger_add_point(&f));
    f.speed_knots = 50;
    CHECK(purrgo_logger_add_point(&f));                             /* граница: движемся */
    f = fixt(26, 10, 19, 12, 5, 10); f.lat_1e7 += 700; f.speed_knots = 0;
    CHECK(purrgo_logger_add_point(&f));                             /* 300 с: по таймауту */
    purrgo_logger_stop();

    /* Выброс: скачок быстрее 500 м/с отбрасывается и не сдвигает точку отсчёта. */
    logger_reset(0);
    f = fixt(26, 10, 19, 12, 0, 0);
    CHECK(purrgo_logger_start(&f) && purrgo_logger_add_point(&f));
    f = fixt(26, 10, 19, 12, 0, 1); f.lat_1e7 += 50000;             /* ~5,5 км за 1 с */
    CHECK(!purrgo_logger_add_point(&f));
    f = fixt(26, 10, 19, 12, 0, 1); f.lat_1e7 += 1000;              /* ~11 м, нормальное движение */
    CHECK(purrgo_logger_add_point(&f));
    f = fixt(26, 10, 19, 12, 0, 1); f.lat_1e7 += 1000 + 4000;       /* ~44 м за тот же момент: dt = 0 -> 1 с */
    CHECK(purrgo_logger_add_point(&f));
    CHECK(purrgo_logger_get_track_point_count() == 3);
    f = fixt(26, 10, 19, 12, 10, 1); f.lat_1e7 += 5000 + 300000;    /* ~33 км за 600 с = 55 м/с: допустимо */
    CHECK(purrgo_logger_add_point(&f));
    purrgo_logger_stop();

    /* EXPEDITION: поворот >= 20 градусов при смещении >= 10 м пишется раньше, чем 100 м. */
    logger_reset(0);
    purrgo_logger_set_mode(LOGGER_MODE_EXPEDITION);
    f = fixt(26, 10, 19, 12, 0, 0); f.course_valid = true; f.course_deg_100 = 35000;   /* 350 */
    CHECK(purrgo_logger_start(&f) && purrgo_logger_add_point(&f));
    f = fixt(26, 10, 19, 12, 0, 5); f.lat_1e7 += 500;               /* ~5,5 м, курс 350 -> 5 (15 град) */
    f.course_valid = true; f.course_deg_100 = 500;
    CHECK(!purrgo_logger_add_point(&f));
    f = fixt(26, 10, 19, 12, 0, 10); f.lat_1e7 += 1500;             /* ~16,7 м, 350 -> 15 (25 град через 0) */
    f.course_valid = true; f.course_deg_100 = 1500;
    CHECK(purrgo_logger_add_point(&f));
    f = fixt(26, 10, 19, 12, 0, 12); f.lat_1e7 += 1500 + 400;       /* ~4,4 м, курс 15 -> 90: мало смещения */
    f.course_valid = true; f.course_deg_100 = 9000;
    CHECK(!purrgo_logger_add_point(&f));
    f = fixt(26, 10, 19, 12, 0, 14); f.lat_1e7 += 1500 + 1500;      /* ~16,7 м, курс неизвестен */
    f.course_valid = false;
    CHECK(!purrgo_logger_add_point(&f));
    purrgo_logger_stop();
    purrgo_logger_set_mode(LOGGER_MODE_STANDARD);

    /* STANDARD: критерий поворота не действует. */
    logger_reset(0);
    f = fixt(26, 10, 19, 12, 0, 0); f.course_valid = true; f.course_deg_100 = 0;
    CHECK(purrgo_logger_start(&f) && purrgo_logger_add_point(&f));
    f = fixt(26, 10, 19, 12, 0, 1); f.lat_1e7 += 300;               /* ~3,3 м (< 5 м) */
    f.course_valid = true; f.course_deg_100 = 9000;
    CHECK(!purrgo_logger_add_point(&f));
    purrgo_logger_stop();
}

int main(void)
{
    test_time_calendar();
    test_time_epoch_edges();
    test_time_roundtrip();
    test_time_timezone();
    test_logger_filename_and_gpx();
    test_logger_invalid_inputs();
    test_logger_decimation();
    test_logger_midnight();
    test_logger_year_before_2026();
    test_logger_same_name_overwrite();
    test_logger_ram_ring();
    test_logger_sync_cadence();
    test_logger_filters();
    return test_finish("test_time_logger");
}
