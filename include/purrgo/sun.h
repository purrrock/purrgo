#ifndef PURRGO_SUN_H
#define PURRGO_SUN_H

#include <stdint.h>
#include <stdbool.h>

// Статус солнечного дня
typedef enum {
    SUN_STATUS_NORMAL,      // Обычный день (есть и восход, и закат)
    SUN_STATUS_POLAR_DAY,   // Полярный день (Солнце не заходит)
    SUN_STATUS_POLAR_NIGHT  // Полярная ночь (Солнце не восходит)
} purrgo_sun_status_t;

// Результат астрономического расчета
typedef struct {
    purrgo_sun_status_t status;
    
    uint8_t sunrise_hour;      // Час восхода (локальное время, 0..23)
    uint8_t sunrise_minute;    // Минута восхода (0..59)
    
    uint8_t sunset_hour;       // Час заката (локальное время, 0..23)
    uint8_t sunset_minute;     // Минута заката (0..59)
    
    int16_t time_to_event_min; // Минут до ближайшего события (заката или восхода)
    bool is_daytime;           // true — сейчас день, false — ночь
} purrgo_sun_info_t;

// Вычисление параметров восхода/заката
void purrgo_sun_calc(int32_t lat_1e7, int32_t lon_1e7,
                     uint8_t year_2digit, uint8_t month, uint8_t day,
                     uint8_t utc_hour, uint8_t utc_minute,
                     int16_t tz_offset_minutes,
                     purrgo_sun_info_t *info);

#endif // PURRGO_SUN_H