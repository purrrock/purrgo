/*
 * Трек-логгер и достоверность времени (app_fsm.c, track_logger_service()):
 *   - решение с позицией, но без time_valid (время неизвестно или отвергнуто проверкой
 *     purrgo_utc_hint_filter()), трек не стартует и файл не создаётся: имя файла и метки точек
 *     взяты из даты решения, а там нули или чужая дата;
 *   - когда время подтвердилось, трек стартует, имя файла - UTC-секунды этого времени;
 *   - во время записи решение без time_valid точку не добавляет (счётчик точек не растёт),
 *     а запись продолжается, когда время вернулось.
 */
#include <stdio.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include "purrgo/purrgo_time.h"
#include "purrgo/track_logger.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

static purrgo_gnss_solution_t make_fix(bool time_valid, int32_t lat, int32_t lon)
{
    purrgo_gnss_solution_t s;
    memset(&s, 0, sizeof s);
    s.valid = true;
    s.fix_type = 3;
    s.fix_quality = 1;
    s.satellites_tracked = 9;
    s.hdop_100 = 100;
    s.pdop_100 = 150;
    s.lat_1e7 = lat;
    s.lon_1e7 = lon;
    s.alt_m = 150;
    s.speed_knots = 500;
    s.time_valid = time_valid;
    if (time_valid) {
        s.year = 26; s.month = 11; s.day = 25; s.hours = 10; s.minutes = 20; s.seconds = 30;
    } else {
        s.year = 6; s.month = 5; s.day = 1; s.hours = 12;   /* чужая дата, как у приёмника после сброса */
    }
    return s;
}

static void app_start(void)
{
    fake_fs_reset();
    fake_power_reset();
    purrgo_logger_reset();
    memset(&app_config, 0, sizeof app_config);
    app_config.log_mode = LOGGER_MODE_STANDARD;
    g_now += 200000;
    purrgo_app_init();
    purrgo_logger_set_mode(LOGGER_MODE_STANDARD);
}

static void update(const purrgo_gnss_solution_t *fix)
{
    g_now += 1000;
    purrgo_app_update(fix);
}

static void test_no_time_no_track(void)
{
    uint32_t epoch = 0;
    char name[32];

    app_start();
    const purrgo_gnss_solution_t bad = make_fix(false, 537135000, 284199000);
    for (int i = 0; i < 5; i++) { update(&bad); }
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);
    CHECK(purrgo_logger_get_active_filename() == NULL || purrgo_logger_get_active_filename()[0] == '\0');

    /* Время подтвердилось: трек стартует, имя - UTC-секунды этого времени (HEX, 8 знаков). */
    const purrgo_gnss_solution_t good = make_fix(true, 537135000, 284199000);
    update(&good);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    CHECK(purrgo_time_datetime_to_epoch(26, 11, 25, 10, 20, 30, &epoch));
    snprintf(name, sizeof name, "%08X.gpx", (unsigned)epoch);
    CHECK(strstr(purrgo_logger_get_active_filename(), name) != NULL);
}

static void test_points_need_time(void)
{
    app_start();
    purrgo_gnss_solution_t fix = make_fix(true, 537135000, 284199000);
    update(&fix);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);

    const uint32_t before = purrgo_logger_get_track_serial();

    /* Позиция сдвинулась заметно, но время недостоверно: точка не пишется. */
    purrgo_gnss_solution_t moved = make_fix(false, 537145000, 284209000);
    update(&moved);
    CHECK(purrgo_logger_get_track_serial() == before);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);

    /* Время вернулось: запись продолжается. */
    moved = make_fix(true, 537145000, 284209000);
    moved.seconds = 40;
    update(&moved);
    CHECK(purrgo_logger_get_track_serial() > before);
}

int main(void)
{
    test_no_time_no_track();
    test_points_need_time();
    return test_finish("test_logger_time_gate");
}
