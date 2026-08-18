#ifndef PURRGO_GNSS_TYPES_H
#define PURRGO_GNSS_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// Универсальная структура GNSS-решения, независимая от текстового NMEA или бинарного UBX.
// Используются строго целочисленные типы для совместимости со STM32 без FPU (например, Cortex-M3 Blue Pill).
typedef struct {
    bool valid;           // Флаг валидности координат (из $GPRMC)
    int32_t lat_1e7;      // Широта в градусах * 10^7 (например, 53.4281566° -> 534281566)
    int32_t lon_1e7;      // Долгота в градусах * 10^7
    int32_t speed_knots;  // Скорость в узлах * 100 (для сохранения 2 знаков после запятой)
    int32_t alt_m;        // Высота над уровнем моря в метрах
    uint8_t satellites;   // Количество используемых спутников
    uint8_t hours;        // Время UTC
    uint8_t minutes;
    uint8_t seconds;
    uint8_t day;          // Дата UTC
    uint8_t month;
    uint8_t year;
} purrgo_gnss_solution_t;

#endif // PURRGO_GNSS_TYPES_H