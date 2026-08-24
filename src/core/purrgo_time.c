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

bool purrgo_time_datetime_to_epoch(uint8_t year, uint8_t month, uint8_t day,
                                   uint8_t h, uint8_t m, uint8_t s,
                                   uint32_t* epoch_out) {
    if (year > 99) return false;
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > purrgo_time_days_in_month(month, year)) return false;
    if (h > 23) return false;
    if (m > 59) return false;
    if (s > 59) return false;

    static const uint16_t days_before_month[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    // Подсчет прошедших лет и дней до начала текущего года
    uint32_t days = year * 365 + (year + 3) / 4 + days_before_month[month - 1] + day - 1;

    // Корректировка, если текущий год високосный и месяц больше февраля
    if (purrgo_time_is_leap_year(year) && month > 2) {
        days++;
    }

    if (epoch_out) {
        *epoch_out = ((days * 24 + h) * 60 + m) * 60 + s;
    }
    return true;
}

void purrgo_time_epoch_to_datetime(uint32_t epoch, uint8_t* year, uint8_t* month, uint8_t* day,
                                   uint8_t* h, uint8_t* m, uint8_t* s) {
    uint32_t time_of_day = epoch % 86400;
    uint32_t days = epoch / 86400;

    *h = time_of_day / 3600;
    *m = (time_of_day % 3600) / 60;
    *s = time_of_day % 60;

    uint32_t y = 0;
    while (1) {
        uint32_t days_in_year = purrgo_time_is_leap_year((uint8_t)y) ? 366 : 365;
        if (days >= days_in_year) {
            days -= days_in_year;
            y++;
        } else {
            break;
        }
    }
    // wrap around at 100 years, according to logic format
    *year = (uint8_t)(y % 100);

    uint8_t mo = 1;
    while (1) {
        uint32_t days_in_mo = purrgo_time_days_in_month(mo, *year);
        if (days >= days_in_mo) {
            days -= days_in_mo;
            mo++;
        } else {
            break;
        }
    }
    *month = mo;
    *day = (uint8_t)(days + 1);
}

void purrgo_time_apply_timezone(const purrgo_gnss_solution_t* utc, purrgo_gnss_solution_t* local, int16_t tz_offset_minutes) {
    *local = *utc;

    if (!local->valid) return;

    uint32_t utc_epoch;
    if (!purrgo_time_datetime_to_epoch(utc->year, utc->month, utc->day, utc->hours, utc->minutes, utc->seconds, &utc_epoch)) {
        return; // Некорректное время в UTC
    }

    int64_t local_epoch_64 = (int64_t)utc_epoch + ((int64_t)tz_offset_minutes * 60);

    // Коррекция wrap around назад в прошлое (year 0 -> year 99)
    if (local_epoch_64 < 0) {
        // Мы предполагаем, что смещение не будет больше 100 лет,
        // поэтому просто добавим 100 лет в секундах.
        // 100 лет = 36525 дней = 3155760000 секунд
        // Но лучше использовать цикл, как было в старом коде,
        // или пересчитать в дни.
        // В старом коде 2000-01-01 -> 2099-12-31 при смещении назад.
        // 100 лет с 2000 до 2100 - это 24 високосных года (2000,2004..2096).
        // 24 + 76 = 100 лет. 100*365 + 25 = 36525 дней.
        int64_t century_seconds = 36525LL * 86400LL;
        while (local_epoch_64 < 0) {
            local_epoch_64 += century_seconds;
        }
    }

    // В старом коде year == 99 и month == 12 и день 31 -> 2000 год.
    // Поскольку у нас 100 лет - это ровно 36525 дней, мы можем взять % century_seconds.
    int64_t century_seconds = 36525LL * 86400LL;
    local_epoch_64 = local_epoch_64 % century_seconds;

    purrgo_time_epoch_to_datetime((uint32_t)local_epoch_64,
                                  &local->year, &local->month, &local->day,
                                  &local->hours, &local->minutes, &local->seconds);
}