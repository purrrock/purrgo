#ifndef PURRGO_TIME_H
#define PURRGO_TIME_H

#include "purrgo/gnss_types.h"
#include <stdbool.h>
#include <stdint.h>

// Проверка года на високосность (формат двухзначного года 2000-2099)
bool purrgo_time_is_leap_year(uint8_t year);

// Получение количества дней в заданном месяце с учетом високосного года
uint8_t purrgo_time_days_in_month(uint8_t month, uint8_t year);

// Применение смещения часового пояса к UTC времени с пересчетом календаря
void purrgo_time_apply_timezone(const purrgo_gnss_solution_t* utc, purrgo_gnss_solution_t* local, int16_t tz_offset_minutes);

#endif // PURRGO_TIME_H