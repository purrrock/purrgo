#ifndef PURRGO_GNSS_TYPES_H
#define PURRGO_GNSS_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// Универсальная структура GNSS-решения, независимая от текстового NMEA или бинарного UBX.
//
// Используются строго целочисленные типы для совместимости со STM32 без FPU
// (например, Cortex-M3 Blue Pill).
//
// GNSS Contract:
// - `valid` indicates if a navigation solution exists and coordinates (lat/lon)
//   are populated. It is derived directly from the 'A' (Active) status in RMC frames.
// - `course_valid` indicates whether `course_deg_100` contains a valid course
//   over ground value. Course validity is independent from position validity,
//   because a GNSS receiver may have a valid position but no valid course.
// - Other fields (like alt_m, speed, hdop) are merged continuously from
//   available NMEA sentences (RMC, GGA, GSA).
//   If a specific sentence omits optional data, the previous value is retained.
// - If the parser state is reset or no valid fix has ever been obtained,
//   these fields may contain 0.
//   0 should NOT be considered a safe indicator of invalidity,
//   except for `fix_quality`, `fix_type`, and fields with an explicit
//   validity flag.
// - `fix_quality`: 0 = Invalid, 1 = GPS fix (SPS), 2 = DGPS fix. (Derived from GGA).
// - `fix_type`: 1 = No fix, 2 = 2D fix, 3 = 3D fix. (Derived from GSA).
typedef struct {
    bool valid;              // Флаг валидности координат (из $GPRMC: true if 'A', false if 'V')
    bool course_valid;      // Флаг валидности курса over ground

    int32_t lat_1e7;         // Широта в градусах * 10^7
                             // (например, 53.4281566° -> 534281566).
    int32_t lon_1e7;         // Долгота в градусах * 10^7

    int32_t speed_knots;     // Скорость в узлах * 100
                             // (для сохранения 2 знаков после запятой)
    int32_t alt_m;           // Высота над уровнем моря в метрах (из GGA, если присутствует)
    uint8_t satellites_tracked; // Количество отслеживаемых спутников (из GGA)

    uint8_t hours;            // Время UTC
    uint8_t minutes;
    uint8_t seconds;
    uint8_t day;              // Дата UTC
    uint8_t month;
    uint8_t year;              // 2-digit representation of the year (2000-2099)

    // Дополнительные поля
    int32_t course_deg_100;   // Истинный курс (Course over ground)
                              // в градусах * 100 (из RMC)
    int32_t hdop_100;         // Horizontal Dilution of Precision * 100 (из GGA или GSA)
    int32_t vdop_100;         // Vertical Dilution of Precision * 100 (из GSA)
    int32_t pdop_100;         // Position Dilution of Precision * 100 (из GSA)

    uint8_t fix_quality;      // Качество фикса (из $GPGGA:
                              // 0=Invalid, 1=GPS, 2=DGPS)
    uint8_t fix_type;         // Тип фикса (из $GPGSA:
                              // 1=Нет фикса, 2=2D, 3=3D)
} purrgo_gnss_solution_t;

#endif // PURRGO_GNSS_TYPES_H