#ifndef PURRGO_TIME_H
#define PURRGO_TIME_H

#include "purrgo/gnss_types.h"
#include <stdbool.h>
#include <stdint.h>

// Проверка года на високосность (формат двухзначного года 2000-2099)
bool purrgo_time_is_leap_year(uint8_t year);

// Получение количества дней в заданном месяце с учетом високосного года
uint8_t purrgo_time_days_in_month(uint8_t month, uint8_t year);

// Преобразование даты и времени (формат двухзначного года 2000-2099) в секунды с начала 2000 года (epoch).
// Возвращает true, если входные данные корректны, false в противном случае (строгая валидация).
bool purrgo_time_datetime_to_epoch(uint8_t year, uint8_t month, uint8_t day,
                                   uint8_t h, uint8_t m, uint8_t s,
                                   uint32_t* epoch_out);

// Преобразование секунд с начала 2000 года в дату и время (формат двухзначного года 2000-2099).
void purrgo_time_epoch_to_datetime(uint32_t epoch, uint8_t* year, uint8_t* month, uint8_t* day,
                                   uint8_t* h, uint8_t* m, uint8_t* s);

// Применение смещения часового пояса к UTC времени с пересчетом календаря
void purrgo_time_apply_timezone(const purrgo_gnss_solution_t* utc, purrgo_gnss_solution_t* local, int16_t tz_offset_minutes);

#endif // PURRGO_TIME_H