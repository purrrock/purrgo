/*
 * Последнее известное время UTC: "пол" из PURRGO.CFG, время сеанса, оценка и проверка времени приёмника.
 * Описание - purrgo/utc_hint.h. Без зависимостей от платформы: момент "сейчас" передают вызывающие
 * (filter/service), поэтому модуль безопасно подключать из конфигурации и из host-тестов.
 */
#include "purrgo/utc_hint.h"

#include <string.h>

#include "purrgo/logger.h"
#include "purrgo/purrgo_time.h"
#include "utc_hint_priv.h"

#define LOG_INTERVAL_MS 30000U

static uint32_t s_floor;

static bool s_have_good;           /* в этом сеансе приёмник давал время */
static uint32_t s_good_epoch;      /* оно, секунды от 2000 */
static uint32_t s_good_ms;         /* и момент его получения */
static uint32_t s_last_sol_epoch;  /* время предыдущего решения: обновляем отсчёт только при его смене */
static uint32_t s_clock_ms;        /* момент последнего вызова filter/service */

static bool s_pos_known;
static int32_t s_pos_lat_1e7;
static int32_t s_pos_lon_1e7;
static int32_t s_pos_alt_cm;
static uint32_t s_pos_ms;

static bool s_logged;
static uint32_t s_logged_ms;

void purrgo_utc_hint_clock_reset(void)
{
    s_floor = 0U;
    s_have_good = false;
    s_good_epoch = 0U;
    s_good_ms = 0U;
    s_last_sol_epoch = 0U;
    s_clock_ms = 0U;
    s_pos_known = false;
    s_logged = false;
}

void purrgo_utc_hint_set_floor(uint32_t epoch)
{
    s_floor = epoch;
}

uint32_t purrgo_utc_hint_floor(void)
{
    return s_floor;
}

/* Секунд от from_ms до now_ms; отрицательный интервал (сбой порядка) считается нулевым. */
static uint32_t elapsed_s(uint32_t from_ms, uint32_t now_ms)
{
    const int32_t d = (int32_t)(now_ms - from_ms);

    return (d > 0) ? ((uint32_t)d / 1000U) : 0U;
}

bool purrgo_utc_hint_estimate_at(uint32_t now_ms, uint32_t *epoch_out)
{
    uint32_t e;

    if (s_have_good) {
        e = s_good_epoch + elapsed_s(s_good_ms, now_ms);
        if (e < s_floor) {
            e = s_floor;
        }
    } else if (s_floor != 0U) {
        e = s_floor;
    } else {
        return false;
    }
    if (epoch_out != NULL) {
        *epoch_out = e;
    }
    return true;
}

bool purrgo_utc_hint_estimate(uint32_t *epoch_out)
{
    return purrgo_utc_hint_estimate_at(s_clock_ms, epoch_out);
}

bool purrgo_utc_hint_priv_session_time(uint32_t now_ms, uint32_t *epoch_out, uint32_t *since_s_out)
{
    if (!s_have_good) {
        return false;
    }
    *since_s_out = elapsed_s(s_good_ms, now_ms);
    return purrgo_utc_hint_estimate_at(now_ms, epoch_out);
}

bool purrgo_utc_hint_priv_position(uint32_t now_ms, int32_t *lat, int32_t *lon, int32_t *alt_cm,
                                   uint32_t *elapsed_s_out)
{
    if (!s_pos_known) {
        return false;
    }
    *lat = s_pos_lat_1e7;
    *lon = s_pos_lon_1e7;
    *alt_cm = s_pos_alt_cm;
    *elapsed_s_out = elapsed_s(s_pos_ms, now_ms);
    return true;
}

void purrgo_utc_hint_priv_touch(uint32_t now_ms)
{
    s_clock_ms = now_ms;
}

static bool sol_epoch(const purrgo_gnss_solution_t *s, uint32_t *epoch)
{
    return purrgo_time_datetime_to_epoch(s->year, s->month, s->day, s->hours, s->minutes, s->seconds, epoch);
}

void purrgo_utc_hint_filter(purrgo_gnss_solution_t *sol, uint32_t now_ms)
{
    if (sol == NULL) {
        return;
    }
    s_clock_ms = now_ms;

    if (sol->time_valid) {
        uint32_t e = 0U;
        uint32_t ref = 0U;
        bool ok = sol_epoch(sol, &e);

        if (ok && purrgo_utc_hint_estimate_at(now_ms, &ref) && (e + PURRGO_UTC_FLOOR_TOLERANCE_S) < ref) {
            ok = false;
        }
        if (ok) {
            if (!s_have_good || e != s_last_sol_epoch) {
                s_good_epoch = e;
                s_good_ms = now_ms;
                s_have_good = true;
            }
            s_last_sol_epoch = e;
        } else {
            sol->time_valid = false;
            if (!s_logged || (uint32_t)(now_ms - s_logged_ms) >= LOG_INTERVAL_MS) {
                s_logged = true;
                s_logged_ms = now_ms;
                PURRGO_LOGW("GNSS: time %02u-%02u-%02u %02u:%02u rejected (earlier than last known time)\r\n",
                            (unsigned)sol->year, (unsigned)sol->month, (unsigned)sol->day,
                            (unsigned)sol->hours, (unsigned)sol->minutes);
            }
        }
    }

    if (sol->valid) {
        s_pos_known = true;
        s_pos_lat_1e7 = sol->lat_1e7;
        s_pos_lon_1e7 = sol->lon_1e7;
        s_pos_alt_cm = sol->alt_m * 100;
        s_pos_ms = now_ms;
    }
}
