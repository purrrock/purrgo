/*
 * Метка времени файлов FatFs (_FS_NORTC = 0).
 *
 * Время берётся из GNSS-решения; пока фикса нет, используется фиксированная
 * дата, чтобы метка FAT оставалась корректной. Тот же код, что в
 * FATFS/App/fatfs.c проекта F411.
 */

#include "ff.h"
#include "diskio.h"

#include "purrgo/app_fsm.h"
#include "purrgo/gnss_types.h"

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

    if ((gnss != NULL) && gnss->valid && (gnss->year > 0))
    {
        year = 2000U + (uint32_t)gnss->year;
        month = (uint32_t)gnss->month;
        day = (uint32_t)gnss->day;
        hour = (uint32_t)gnss->hours;
        minute = (uint32_t)gnss->minutes;
        second = (uint32_t)gnss->seconds;
    }
    else
    {
        /* Нет спутникового времени: 1 января 2026, 00:00:00. */
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
