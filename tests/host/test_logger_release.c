/*
 * Трек-логгер в режиме трекера (purrgo_logger_set_release_between_writes): файл открыт только на время записи на SD.
 *   - после первой же точки файл закрыт, платформе сказано, что карта не нужна (purrgo_fs_idle_hint) и в этот момент
 *     открытых файлов нет (иначе парковка карты ничего бы не сделала);
 *   - между записями SD не трогается: точки копятся в буфере, открытий файла нет;
 *   - когда буфер GPX заполнился или пришёл срок синхронизации, файл открывается на дозапись, пишется, закрывается;
 *   - в итоге GPX целый: заголовок один, все точки на месте по порядку, футер один;
 *   - выключение режима возвращает открытый файл, остановка трека пишет футер и из закрытого файла;
 *   - сбой повторного открытия переводит логгер в ERROR (дальше работает обычное восстановление);
 *   - без режима трекера ничего не меняется: файл открыт, подсказок нет.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "purrgo/config.h"
#include "purrgo/purrgo_time.h"
#include "purrgo/track_logger.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

#define LAT0 537135000
#define LON0 284199000

/* Точка через n секунд от начала; смещение по широте больше шага EXPEDITION (10 м) и растёт монотонно: каждая пишется. */
static purrgo_gnss_solution_t pt(uint32_t n)
{
    purrgo_gnss_solution_t f;
    memset(&f, 0, sizeof f);
    f.valid = true; f.time_valid = true; f.fix_type = 3; f.fix_quality = 1;
    f.satellites_tracked = 9; f.hdop_100 = 100;
    f.year = 26; f.month = 11; f.day = 25;
    f.hours = (uint8_t)(10U + n / 3600U); f.minutes = (uint8_t)((n / 60U) % 60U); f.seconds = (uint8_t)(n % 60U);
    f.lat_1e7 = LAT0 + (int32_t)(n * 200U);       /* ~2,2 м на секунду: шаг 15 с = 33 м */
    f.lon_1e7 = LON0 + (int32_t)((n % 2U) * 3000U);   /* зигзаг: поворот на каждой точке, отрезки не сливаются */
    f.alt_m = 100;
    f.speed_knots = 500;
    return f;
}

static int count_str(const char *hay, const char *needle)
{
    int c = 0;
    for (const char *p = hay; (p = strstr(p, needle)) != NULL; p += strlen(needle)) { c++; }
    return c;
}

static void start_session(void)
{
    fake_fs_reset();
    fake_fs_idle_hint_reset();
    fake_power_reset();
    purrgo_logger_reset();
    memset(&app_config, 0, sizeof app_config);
    purrgo_logger_set_mode(LOGGER_MODE_EXPEDITION);
    const purrgo_gnss_solution_t first = pt(0);
    CHECK(purrgo_logger_start(&first));
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
}

static void test_release_after_each_point(void)
{
    start_session();
    purrgo_logger_set_release_between_writes(true);
    /* Режим включён при открытом файле: он закрывается сразу, карта свободна. */
    CHECK(fake_fs_open_handles() == 0);
    CHECK(fake_fs_idle_hint_calls() == 1);
    CHECK(fake_fs_open_at_last_idle_hint() == 0);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);

    const int opens_before = fake_fs_open_calls();
    for (uint32_t n = 15; n <= 15U * 4U; n += 15U) {
        const purrgo_gnss_solution_t f = pt(n);
        CHECK(purrgo_logger_add_point(&f));
        CHECK(fake_fs_open_handles() == 0);           /* между точками файл закрыт */
    }
    CHECK(purrgo_logger_get_track_point_count() == 4);
    /* Буфер (1 КБ) не заполнился, срок синхронизации (60 с) только наступил: на карту ходили не на каждую точку. */
    CHECK(fake_fs_open_calls() - opens_before <= 1);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
}

static void test_long_session_gpx_intact(void)
{
    start_session();
    purrgo_logger_set_release_between_writes(true);

    const int points = 120;                           /* 30 минут при шаге 15 с: десятки сбросов буфера */
    for (int i = 1; i <= points; i++) {
        const purrgo_gnss_solution_t f = pt((uint32_t)i * 15U);
        CHECK(purrgo_logger_add_point(&f));
        CHECK(fake_fs_open_handles() == 0);
    }
    /* Обращений к карте заметно меньше, чем точек (буфер 1 КБ и синхронизация раз в минуту). */
    CHECK(fake_fs_open_calls() < points / 2);
    CHECK(fake_fs_idle_hint_calls() >= 2);
    CHECK(fake_fs_open_at_last_idle_hint() == 0);

    char name[64];
    snprintf(name, sizeof name, "%s", purrgo_logger_get_active_filename());
    purrgo_logger_stop();                             /* футер дописывается в закрытый файл */
    uint32_t sz = 0;
    const uint8_t *d = fake_fs_data(name, &sz);
    CHECK(d != NULL && sz > 0);
    if (d != NULL) {
        char *t = malloc((size_t)sz + 1U);
        memcpy(t, d, sz);
        t[sz] = 0;
        CHECK(count_str(t, "<gpx version") == 1);
        CHECK(count_str(t, "</gpx>") == 1);
        CHECK(count_str(t, "<trkpt ") == points);
        /* Порядок: широта растёт от точки к точке. */
        int prev = -1, ordered = 1;
        for (const char *p = t; (p = strstr(p, "<trkpt lat=\"")) != NULL; p += 12) {
            const int deg = atoi(p + 12);
            if (deg < prev) { ordered = 0; }
            prev = deg;
        }
        CHECK(ordered);
        free(t);
    }
    CHECK(fake_fs_open_handles() == 0);
}

static void test_disable_reopens_and_normal_mode(void)
{
    start_session();
    purrgo_logger_set_release_between_writes(true);
    const purrgo_gnss_solution_t a = pt(15);
    CHECK(purrgo_logger_add_point(&a));
    CHECK(fake_fs_open_handles() == 0);

    purrgo_logger_set_release_between_writes(false);   /* выход из трекера: файл снова открыт */
    CHECK(fake_fs_open_handles() == 1);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);

    fake_fs_idle_hint_reset();
    const purrgo_gnss_solution_t b = pt(30);
    CHECK(purrgo_logger_add_point(&b));
    CHECK(fake_fs_open_handles() == 1);                /* обычный режим: файл открыт всё время записи */
    CHECK(fake_fs_idle_hint_calls() == 0);
    purrgo_logger_stop();
    CHECK(fake_fs_open_handles() == 0);
}

static void test_reopen_failure_goes_to_error(void)
{
    start_session();
    purrgo_logger_set_release_between_writes(true);
    CHECK(fake_fs_open_handles() == 0);

    /* Карта не отвечает при дозаписи: ближайший сброс буфера (синхронизация через минуту) уходит в ERROR. */
    g_fault.fail_open_append = 1;
    int errored = 0;
    for (uint32_t n = 15; n <= 15U * 8U; n += 15U) {
        const purrgo_gnss_solution_t f = pt(n);
        (void)purrgo_logger_add_point(&f);
        if (purrgo_logger_get_state() == LOGGER_STATE_ERROR) { errored = 1; break; }
    }
    CHECK(errored);
    CHECK(fake_fs_open_handles() == 0);

    /* Карта вернулась: перезапуск пробует дозаписать тот же файл (восстановление из app_fsm). */
    g_fault.fail_open_append = 0;
    const purrgo_gnss_solution_t f = pt(200);
    CHECK(purrgo_logger_start(&f));
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    purrgo_logger_stop();
}

int main(void)
{
    test_release_after_each_point();
    test_long_session_gpx_intact();
    test_disable_reopens_and_normal_mode();
    test_reopen_failure_goes_to_error();
    return test_finish("test_logger_release");
}
