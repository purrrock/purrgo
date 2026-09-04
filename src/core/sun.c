#include "purrgo/sun.h"
#include "purrgo/sun_tables.h" // Файл с LUT, сгенерированный скриптом на Python

// Константа: синус угла захода солнца (-0.833 градуса с учетом рефракции атмосферы)
// sin(-0.833) * 10000 = -145
#define SIN_SUNSET_10K -145

// Таблица арккосинуса (от -10000 до 10000 -> углы от 180 до 0 градусов)
// 201 элемент (шаг 0.01) позволяет получить точность до ~1 градуса (4 минуты времени)
extern const uint8_t acos_lut[201]; 

// Целочисленный синус для широты (дополнительная таблица или использование geo.c)
extern const int16_t sin_lut[91]; 
extern const int16_t cos_lut[91];

// Расчет дня в году
static uint16_t get_day_of_year(uint8_t year_2digit, uint8_t month, uint8_t day) {
    static const uint16_t days_before_month[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    uint16_t doy = days_before_month[month - 1] + day;
    if (month > 2 && (year_2digit % 4 == 0)) {
        doy++;
    }
    return doy;
}

void purrgo_sun_calc(int32_t lat_1e7, int32_t lon_1e7,
                     uint8_t year_2digit, uint8_t month, uint8_t day,
                     uint8_t utc_hour, uint8_t utc_minute,
                     int16_t tz_offset_minutes,
                     purrgo_sun_info_t *info) {
    
    uint16_t doy = get_day_of_year(year_2digit, month, day);
    if (doy > 366) doy = 366;

    // 1. Получение астрономических параметров на текущий день из Flash (масштаб 10000)
    int32_t sin_dec = sun_sin_dec_lut[doy];
    int32_t cos_dec = sun_cos_dec_lut[doy];
    int32_t eot_min = sun_eot_lut[doy];

    // 2. Получение тригонометрии для текущей широты
    int32_t lat_abs = lat_1e7 < 0 ? -lat_1e7 : lat_1e7;
    int32_t lat_deg = lat_abs / 10000000;
    if (lat_deg > 90) lat_deg = 90;
    
    int32_t sin_lat = sin_lut[lat_deg];
    if (lat_1e7 < 0) sin_lat = -sin_lat; // Учет Южного полушария
    int32_t cos_lat = cos_lut[lat_deg];

    // 3. Вычисление часового угла: cos(H) = (sin(-0.833) - sin(lat)*sin(dec)) / (cos(lat)*cos(dec))
    int32_t numerator = (SIN_SUNSET_10K * 10000) - (sin_lat * sin_dec);
    int32_t denominator = (cos_lat * cos_dec) / 10000;
    
    if (denominator == 0) denominator = 1; // Защита от деления на ноль на полюсах
    
    int32_t cos_H_10k = numerator / denominator;

    // Проверка полярного дня и ночи
    if (cos_H_10k > 10000) {
        info->status = SUN_STATUS_POLAR_NIGHT;
        info->is_daytime = false;
        info->time_to_event_min = -1; // Сигнал UI скрыть обратный отсчет
        return;
    }
    if (cos_H_10k < -10000) {
        info->status = SUN_STATUS_POLAR_DAY;
        info->is_daytime = true;
        info->time_to_event_min = -1; // Сигнал UI скрыть обратный отсчет
        return;
    }

    info->status = SUN_STATUS_NORMAL;

    // 4. Поиск угла H в градусах через таблицу арккосинуса
    // Нормализация аргумента к индексам массива 0..200
    int32_t acos_idx = (cos_H_10k + 10000) / 100; 
    if (acos_idx < 0) acos_idx = 0;
    if (acos_idx > 200) acos_idx = 200;
    
    int32_t H_deg = acos_lut[acos_idx];

// 5. Перевод долготы в минуты (1 градус = 4 минуты времени)
    // Оптимизация: деление на 2500000 исключает переполнение int32_t
    int32_t lon_min = lon_1e7 / 2500000;

    // 6. Базовое время полудня (12:00 = 720 минут)
    int32_t solar_noon = 720 - lon_min - eot_min;

    // 7. Смещение заката и восхода от полудня (H_deg * 4 мин)
    int32_t event_offset = H_deg * 4;

    int32_t sunrise_utc = solar_noon - event_offset;
    int32_t sunset_utc  = solar_noon + event_offset;

    // 8. Накатываем часовой пояс
    int32_t sunrise_loc = sunrise_utc + tz_offset_minutes;
    int32_t sunset_loc  = sunset_utc + tz_offset_minutes;

    // Нормализация 0..1440 минут
    while (sunrise_loc < 0) sunrise_loc += 1440;
    while (sunrise_loc >= 1440) sunrise_loc -= 1440;
    while (sunset_loc < 0) sunset_loc += 1440;
    while (sunset_loc >= 1440) sunset_loc -= 1440;

    info->sunrise_hour   = (uint8_t)(sunrise_loc / 60);
    info->sunrise_minute = (uint8_t)(sunrise_loc % 60);
    info->sunset_hour    = (uint8_t)(sunset_loc / 60);
    info->sunset_minute  = (uint8_t)(sunset_loc % 60);

    // 9. Определение дневного времени и обратного отсчета
    if (info->status == SUN_STATUS_NORMAL) {
        int32_t current_utc_min = (int32_t)utc_hour * 60 + (int32_t)utc_minute;
        int32_t current_loc_min = current_utc_min + tz_offset_minutes;
        
        while (current_loc_min < 0) current_loc_min += 1440;
        while (current_loc_min >= 1440) current_loc_min -= 1440;

        // Корректная проверка вхождения в интервал (с учетом перехода через полночь)
        if (sunrise_loc <= sunset_loc) {
            info->is_daytime = (current_loc_min >= sunrise_loc && current_loc_min < sunset_loc);
        } else {
            info->is_daytime = (current_loc_min >= sunrise_loc || current_loc_min < sunset_loc);
        }

        // Вычисление дельты времени с защитой от отрицательных значений
        int32_t diff = 0;
        if (info->is_daytime) {
            diff = sunset_loc - current_loc_min;
        } else {
            diff = sunrise_loc - current_loc_min;
        }

        if (diff < 0) {
            diff += 1440;
        }
        
        info->time_to_event_min = (int16_t)diff;

    } else {
        // Логика для полярного дня и полярной ночи
        info->is_daytime = (info->status == SUN_STATUS_POLAR_DAY);
        info->time_to_event_min = -1; // Сигнал UI скрыть обратный отсчет
    }
}