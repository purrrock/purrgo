#include "purrgo/geo.h"

// Тригонометрические Look-Up Tables (LUT) для целочисленной арифметики.
// Позволяют избежать использования тяжелой библиотеки <math.h> и эмуляции FPU на STM32F103/U5.

// LUT косинусов для углов от 0 до 90 градусов с шагом 10 градусов.
// Масштабный коэффициент: 10000.
static const int16_t cos_lut[10] = {
    10000, 9848, 9396, 8660, 7660, 6427, 5000, 3420, 1736, 0
};

// LUT арктангенса для соотношения a = min(x,y)/max(x,y)
// Индекс массива соответствует a * 10 (от 0.0 до 1.0 с шагом 0.1).
// Значения — углы в градусах (0..45).
static const uint8_t atan_lut[11] = {
    0, 6, 11, 17, 22, 27, 31, 35, 39, 42, 45
};

// Аппаратный целочисленный квадратный корень (оптимизировано под битовый сдвиг)
static uint32_t isqrt64(uint64_t n) {
    uint64_t root = 0;
    uint64_t bit = 1ULL << 62; // Максимально возможный старший четный бит
    
    while (bit > n) {
        bit >>= 2;
    }
    
    while (bit != 0) {
        if (n >= root + bit) {
            n -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)root;
}

uint32_t purrgo_geo_distance_m(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2) {
    int64_t dlat = (int64_t)lat2 - (int64_t)lat1;
    int64_t dlon = (int64_t)lon2 - (int64_t)lon1;

    // Handle wrap-around at 180th meridian (-180 to +180 degrees)
    // 180 degrees = 1800000000 in 1e7 format. 360 degrees = 3600000000.
    if (dlon > 1800000000LL) {
        dlon -= 3600000000LL;
    } else if (dlon < -1800000000LL) {
        dlon += 3600000000LL;
    }

    // Эквидистантная цилиндрическая проекция (оптимальна для дистанций < 500 км).
    // Формула гаверсинуса (Haversine) требует много вычислений синусов, 
    // поэтому проекция на плоскость является золотым стандартом для туристических GPS логгеров.
    
    int32_t mean_lat = (lat1 / 2) + (lat2 / 2); // Средняя широта для масштабирования долготы
    int32_t mean_lat_abs = mean_lat < 0 ? -mean_lat : mean_lat;
    int32_t mean_lat_deg = mean_lat_abs / 10000000;

    if (mean_lat_deg > 90) mean_lat_deg = 90;

    // Линейная интерполяция косинуса
    int32_t idx = mean_lat_deg / 10;
    int32_t frac = mean_lat_deg % 10;
    int32_t cos_lat = cos_lut[idx];
    if (idx < 9) {
        cos_lat -= ((cos_lut[idx] - cos_lut[idx+1]) * frac) / 10;
    }

    // Масштабирование оси X (долгота сжимается при приближении к полюсам).
    // Используется int64_t, что транслируется в инструкции UMULL/SMULL на Cortex-M3 (3-5 тактов).
    int64_t dx = ((int64_t)dlon * cos_lat) / 10000;
    int64_t dy = dlat;

    // Теорема Пифагора
    uint64_t dx2 = (uint64_t)(dx * dx);
    uint64_t dy2 = (uint64_t)(dy * dy);
    uint32_t dist_1e7 = isqrt64(dx2 + dy2);

    // Перевод из 10^-7 градусов в метры. 1 градус меридиана ≈ 111195 метров.
    uint32_t dist_m = (uint32_t)(((uint64_t)dist_1e7 * 111195ULL) / 10000000ULL);
    
    return dist_m;
}

uint16_t purrgo_geo_azimuth_deg(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2) {
    int64_t dlat = (int64_t)lat2 - (int64_t)lat1;
    int64_t dlon = (int64_t)lon2 - (int64_t)lon1;

    // Handle wrap-around at 180th meridian
    if (dlon > 1800000000LL) {
        dlon -= 3600000000LL;
    } else if (dlon < -1800000000LL) {
        dlon += 3600000000LL;
    }

    int32_t mean_lat = (lat1 / 2) + (lat2 / 2);
    int32_t mean_lat_abs = mean_lat < 0 ? -mean_lat : mean_lat;
    int32_t mean_lat_deg = mean_lat_abs / 10000000;
    if (mean_lat_deg > 90) mean_lat_deg = 90;

    int32_t idx = mean_lat_deg / 10;
    int32_t frac = mean_lat_deg % 10;
    int32_t cos_lat = cos_lut[idx];
    if (idx < 9) {
        cos_lat -= ((cos_lut[idx] - cos_lut[idx+1]) * frac) / 10;
    }

    // Вычисление векторов в проекционной сетке
    int64_t x = dlat; // Ось X направлена на Север
    int64_t y = (dlon * cos_lat) / 10000; // Ось Y направлена на Восток

    int64_t abs_x = x < 0 ? -x : x;
    int64_t abs_y = y < 0 ? -y : y;
    
    if (abs_x == 0 && abs_y == 0) return 0; // Точки совпадают

    int64_t min = abs_x < abs_y ? abs_x : abs_y;
    int64_t max = abs_x > abs_y ? abs_x : abs_y;

    // Нормализация аргумента (0..100) для Look-up table. Safe 64-bit multiplication to avoid overflow.
    int32_t a = (int32_t)((min * 100LL) / max);
    
    // Линейная интерполяция арктангенса базового угла
    int32_t a_idx = a / 10;
    int32_t a_frac = a % 10;
    int32_t angle = atan_lut[a_idx];
    if (a_idx < 10) {
        angle += ((atan_lut[a_idx+1] - atan_lut[a_idx]) * a_frac) / 10;
    }

    // Восстановление реального азимута на основе октанта и квадранта
    if (abs_y > abs_x) {
        angle = 90 - angle; // Смещение относительно оси Y
    }
    
    if (x < 0) {
        angle = 180 - angle; // Точка находится южнее (Квадранты 2 и 3)
    }
    if (y < 0) {
        angle = 360 - angle; // Точка находится западнее (Квадранты 3 и 4)
    }

    return (uint16_t)(angle == 360 ? 0 : angle);
}