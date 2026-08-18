#ifndef PURRGO_NAVIGATION_H
#define PURRGO_NAVIGATION_H

#include "purrgo/gnss_types.h"
#include "purrgo/geo.h"

// Структура навигационной путевой точки (Waypoint)
typedef struct {
    int32_t lat_1e7;
    int32_t lon_1e7;
    char name[16];
} purrgo_waypoint_t;

// Структура текущих данных навигации
typedef struct {
    uint32_t distance_to_wp_m;  // Дистанция до цели в метрах
    uint16_t bearing_to_wp_deg; // Истинный пеленг на цель (от 0 до 359)
    bool is_arrived;            // Флаг вхождения в радиус цели
} purrgo_nav_status_t;

// Обновление навигационных параметров на основе текущих координат
// radius_m - радиус прибытия (например, 15 метров)
void purrgo_nav_update(const purrgo_gnss_solution_t* current_fix, 
                       const purrgo_waypoint_t* target_wp, 
                       uint32_t radius_m,
                       purrgo_nav_status_t* status);

#endif // PURRGO_NAVIGATION_H