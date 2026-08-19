#include "purrgo/sun.h"
#include <math.h>

#define DEG_TO_RAD (3.14159265358979323846f / 180.0f)
#define RAD_TO_DEG (180.0f / 3.14159265358979323846f)

// Расчет дня в году (1..366)
static uint16_t get_day_of_year(uint8_t year_2digit, uint8_t month, uint8_t day) {
    static const uint16_t days_before_month[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    uint16_t doy = days_before_month[month - 1] + day;
    if (month > 2 && (year_2digit % 4 == 0)) {
        doy++; // Високосный год
    }
    return doy;
}

void purrgo_sun_calc(int32_t lat_1e7, int32_t lon_1e7,
                     uint8_t year_2digit, uint8_t month, uint8_t day,
                     uint8_t utc_hour, uint8_t utc_minute,
                     int16_t tz_offset_minutes,
                     purrgo_sun_info_t *info) {
    if (!info) return;

    float lat = (float)lat_1e7 / 10000000.0f;
    float lon = (float)lon_1e7 / 10000000.0f;

    uint16_t N = get_day_of_year(year_2digit, month, day);

    // 1. Приближенное время полдня в суточном ходе (в днях)
    float lng_hour = lon / 15.0f;
    
    // 2. Склонение Солнца (Solar Declination)
    float M = (0.985600f * N) - 3.289f;
    float L = M + (1.916f * sinf(M * DEG_TO_RAD)) + (0.020f * sinf(2.0f * M * DEG_TO_RAD)) + 282.634f;
    if (L >= 360.0f) L -= 360.0f;
    if (L < 0.0f)   L += 360.0f;

    float RA = RAD_TO_DEG * atanf(0.91764f * tanf(L * DEG_TO_RAD));
    if (RA >= 360.0f) RA -= 360.0f;
    if (RA < 0.0f)   RA += 360.0f;

    // Коррекция квадранта прямого восхождения
    float Lquadrant  = floorf(L / 90.0f) * 90.0f;
    float RAquadrant = floorf(RA / 90.0f) * 90.0f;
    RA = RA + (Lquadrant - RAquadrant);
    RA = RA / 15.0f; // Перевод в часы

    float sinDec = 0.39782f * sinf(L * DEG_TO_RAD);
    float cosDec = cosf(asinf(sinDec));

    // 3. Часовой угол Солнца (Зенитный угол заката/восхода = 90.833° с учетом рефракции)
    float cosH = (cosf(90.833f * DEG_TO_RAD) - (sinDec * sinf(lat * DEG_TO_RAD))) / (cosDec * cosf(lat * DEG_TO_RAD));

    if (cosH > 1.0f) {
        info->status = SUN_STATUS_POLAR_NIGHT;
        info->is_daytime = false;
        info->time_to_event_min = -1;
        return;
    }
    if (cosH < -1.0f) {
        info->status = SUN_STATUS_POLAR_DAY;
        info->is_daytime = true;
        info->time_to_event_min = -1;
        return;
    }

    info->status = SUN_STATUS_NORMAL;

    // 4. Расчет времени восхода и заката по UTC (в часах с дробной частью)
    float H_sunrise = 360.0f - RAD_TO_DEG * acosf(cosH);
    float H_sunset  = RAD_TO_DEG * acosf(cosH);

    float T_sunrise = H_sunrise / 15.0f + RA - (0.06571f * N) - 6.622f - lng_hour;
    float T_sunset  = H_sunset  / 15.0f + RA - (0.06571f * N) - 6.622f - lng_hour;

    // Нормализация к 0..24 часам UTC
    while (T_sunrise >= 24.0f) T_sunrise -= 24.0f;
    while (T_sunrise < 0.0f)   T_sunrise += 24.0f;
    while (T_sunset  >= 24.0f) T_sunset  -= 24.0f;
    while (T_sunset  < 0.0f)   T_sunset  += 24.0f;

    // 5. Перевод в минуты от начала суток по UTC
    int32_t sunrise_utc_min = (int32_t)(T_sunrise * 60.0f);
    int32_t sunset_utc_min  = (int32_t)(T_sunset  * 60.0f);

    // 6. Перевод в локальное время
    int32_t sunrise_loc_min = sunrise_utc_min + tz_offset_minutes;
    int32_t sunset_loc_min  = sunset_utc_min  + tz_offset_minutes;

    while (sunrise_loc_min < 0)      sunrise_loc_min += 1440;
    while (sunrise_loc_min >= 1440)  sunrise_loc_min -= 1440;
    while (sunset_loc_min < 0)       sunset_loc_min  += 1440;
    while (sunset_loc_min >= 1440)   sunset_loc_min  -= 1440;

    info->sunrise_hour   = (uint8_t)(sunrise_loc_min / 60);
    info->sunrise_minute = (uint8_t)(sunrise_loc_min % 60);
    info->sunset_hour    = (uint8_t)(sunset_loc_min / 60);
    info->sunset_minute  = (uint8_t)(sunset_loc_min % 60);

    // 7. Определение времени до ближайшего заката / восхода
    int32_t now_utc_min = (int32_t)utc_hour * 60 + utc_minute;

    // Проверяем, находится ли текущее время между восходом и закатом по UTC
    if (sunrise_utc_min < sunset_utc_min) {
        info->is_daytime = (now_utc_min >= sunrise_utc_min && now_utc_min < sunset_utc_min);
    } else {
        // Переход через полночь UTC
        info->is_daytime = (now_utc_min >= sunrise_utc_min || now_utc_min < sunset_utc_min);
    }

    if (info->is_daytime) {
        // До заката
        int32_t diff = sunset_utc_min - now_utc_min;
        if (diff < 0) diff += 1440;
        info->time_to_event_min = (int16_t)diff;
    } else {
        // До восхода
        int32_t diff = sunrise_utc_min - now_utc_min;
        if (diff < 0) diff += 1440;
        info->time_to_event_min = (int16_t)diff;
    }
}