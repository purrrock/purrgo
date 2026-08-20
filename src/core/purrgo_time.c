#include "purrgo/purrgo_time.h"

bool purrgo_time_is_leap_year(uint8_t year) {
    return (year % 4 == 0);
}

uint8_t purrgo_time_days_in_month(uint8_t month, uint8_t year) {
    if (month == 2) {
        return purrgo_time_is_leap_year(year) ? 29 : 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;
    }
    return 31;
}

void purrgo_time_apply_timezone(const purrgo_gnss_solution_t* utc, purrgo_gnss_solution_t* local, int16_t tz_offset_minutes) {
    *local = *utc;

    if (!local->valid) return;

    int32_t total_mins = (int32_t)local->hours * 60 + (int32_t)local->minutes + tz_offset_minutes;

    // Коррекция перехода через полночь назад
    while (total_mins < 0) {
        total_mins += 1440;

        if (local->day > 1) {
            local->day--;
        } else {
            if (local->month > 1) {
                local->month--;
            } else {
                local->month = 12;
                if (local->year > 0) {
                    local->year--;
                } else {
                    local->year = 99; // 2000 -> 2099
                }
            }
            local->day = purrgo_time_days_in_month(local->month, local->year);
        }
    }

    // Коррекция перехода через полночь вперед
    while (total_mins >= 1440) {
        total_mins -= 1440;

        uint8_t dim = purrgo_time_days_in_month(local->month, local->year);
        if (local->day < dim) {
            local->day++;
        } else {
            local->day = 1;
            if (local->month < 12) {
                local->month++;
            } else {
                local->month = 1;
                if (local->year < 99) {
                    local->year++;
                } else {
                    local->year = 0; // 2099 -> 2000
                }
            }
        }
    }

    local->hours = (uint8_t)(total_mins / 60);
    local->minutes = (uint8_t)(total_mins % 60);
}