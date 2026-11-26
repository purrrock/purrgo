/*
 * Метка времени файлов FatFs (_FS_NORTC = 0).
 *
 * Время берётся из GNSS-решения (time_valid). Пока приёмник времени не сообщил, используется оценка
 * последнего известного времени (purrgo/utc_hint.h: время приёмника в этом сеансе плюс прошедшее с тех
 * пор, а сразу после включения - время, сохранённое в PURRGO.CFG, ключ LAST_UTC): метка не меньше
 * последней известной даты, но может отставать от настоящей. Если неизвестно и это, используется
 * фиксированная дата, чтобы метка FAT оставалась корректной.
 */

#include "ff.h"
#include "diskio.h"

#include "purrgo/app_fsm.h"
#include "purrgo/gnss_types.h"
#include "purrgo/purrgo_time.h"
#include "purrgo/utc_hint.h"

#include <stddef.h>
#include <stdint.h>

DWORD get_fattime(void)
{
    const purrgo_gnss_solution_t *gnss = purrgo_app_get_gnss_solution();
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t hour;
    uint32_t minute;
    uint32_t second;
    uint32_t epoch;
    uint8_t yy, mo, dd, hh, mi, ss;

    if ((gnss != NULL) && gnss->time_valid && (gnss->year > 0))
    {
        year = 2000U + (uint32_t)gnss->year;
        month = (uint32_t)gnss->month;
        day = (uint32_t)gnss->day;
        hour = (uint32_t)gnss->hours;
        minute = (uint32_t)gnss->minutes;
        second = (uint32_t)gnss->seconds;
    }
    else if (purrgo_utc_hint_estimate(&epoch) &&
             purrgo_time_epoch_to_datetime(epoch, &yy, &mo, &dd, &hh, &mi, &ss))
    {
        /* Приёмник времени не знает, но последнее известное время есть: метка не раньше него. */
        year = 2000U + (uint32_t)yy;
        month = (uint32_t)mo;
        day = (uint32_t)dd;
        hour = (uint32_t)hh;
        minute = (uint32_t)mi;
        second = (uint32_t)ss;
    }
    else
    {
        /* Нет никакого времени: 1 января 2026, 00:00:00. */
        year = 2026U;
        month = 1U;
        day = 1U;
        hour = 0U;
        minute = 0U;
        second = 0U;
    }

    return ((DWORD)(year - 1980U) << 25)
         | ((DWORD)month << 21)
         | ((DWORD)day << 16)
         | ((DWORD)hour << 11)
         | ((DWORD)minute << 5)
         | ((DWORD)(second / 2U));
}
