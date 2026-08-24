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

void purrgo_geo_bbox_from_center(int32_t center_lat_1e7, int32_t center_lon_1e7, uint32_t width_m, const purrgo_viewport_t* vp, purrgo_bbox_t* out_bbox) {
    if (!vp || !out_bbox) return;

    // Перевод ширины в метрах в градусы широты (1e7)
    // 1 градус широты ≈ 111195 метров
    // Значит 1 метр ≈ 10^7 / 111195 ≈ 90 единиц (точнее 89.93)
    // Используем uint64_t, чтобы избежать переполнения при больших width_m
    uint64_t width_1e7_64 = (uint64_t)width_m * PURRGO_1E7_PER_METER;

    // Ограничиваем максимальную ширину до 360 градусов (3.6e9)
    if (width_1e7_64 > 3600000000ULL) {
        width_1e7_64 = 3600000000ULL;
    }

    uint32_t width_1e7 = (uint32_t)width_1e7_64;

    // Половина ширины для X
    int32_t rad_x_base = width_1e7 / 2;

    // Расчет высоты через пропорцию экрана
    // Используем int64_t, чтобы избежать переполнения до приведения к int32_t.
    int64_t height_1e7_64 = ((int64_t)width_1e7 * vp->height) / vp->width;
    if (height_1e7_64 > 1800000000LL) {
        height_1e7_64 = 1800000000LL;
    }

    // Половина высоты для Y
    int64_t rad_y = height_1e7_64 / 2;

    int32_t cos_val = purrgo_geo_cos_10k(center_lat_1e7);
    if (cos_val < 100) cos_val = 100; // prevent division by zero near poles

    // Масштабируем X координату в зависимости от широты
    int64_t rad_x_64 = ((int64_t)rad_x_base * 10000) / cos_val;

    int64_t min_lon, max_lon;
    if (rad_x_64 >= 1800000000LL) {
        // Если охват >= 360 градусов, берем весь мир
        min_lon = -1800000000LL;
        max_lon = 1800000000LL;
    } else {
        min_lon = (int64_t)center_lon_1e7 - rad_x_64;
        max_lon = (int64_t)center_lon_1e7 + rad_x_64;

        // Нормализация границ долготы до [-1.8e9, +1.8e9] (WGS84 1e7)
        if (min_lon < -1800000000LL) {
            min_lon += 3600000000LL;
        } else if (min_lon > 1800000000LL) {
            min_lon -= 3600000000LL;
        }
        if (max_lon < -1800000000LL) {
            max_lon += 3600000000LL;
        } else if (max_lon > 1800000000LL) {
            max_lon -= 3600000000LL;
        }
    }

    int64_t min_lat = (int64_t)center_lat_1e7 - rad_y;
    int64_t max_lat = (int64_t)center_lat_1e7 + rad_y;

    // Ограничиваем широту до [-9.0e8, +9.0e8]
    if (min_lat < -900000000LL) min_lat = -900000000LL;
    if (max_lat > 900000000LL) max_lat = 900000000LL;

    out_bbox->min_x = (int32_t)min_lon;
    out_bbox->max_x = (int32_t)max_lon;
    out_bbox->min_y = (int32_t)min_lat;
    out_bbox->max_y = (int32_t)max_lat;
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

    // Эквидистантная цилиндрическая проекция.
    // Аппроксимирует поверхность Земли плоскостью для вычисления расстояний.
    // Избегает вычислительно затратных операций, требуемых формулой гаверсинуса (Haversine).
    
    int32_t mean_lat = (lat1 / 2) + (lat2 / 2); // Средняя широта для масштабирования долготы

    // Линейная интерполяция косинуса
    int32_t cos_lat = purrgo_geo_cos_10k(mean_lat);

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

    int32_t cos_lat = purrgo_geo_cos_10k(mean_lat);

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

int32_t purrgo_geo_cos_10k(int32_t lat_1e7) {
    int32_t lat_abs = lat_1e7 < 0 ? -lat_1e7 : lat_1e7;
    int32_t lat_deg = lat_abs / 10000000;
    if (lat_deg > 90) lat_deg = 90;

    int32_t idx = lat_deg / 10;
    int32_t frac = lat_deg % 10;
    int32_t cos_lat = cos_lut[idx];
    if (idx < 9) {
        cos_lat -= ((cos_lut[idx] - cos_lut[idx+1]) * frac) / 10;
    }

    return cos_lat;
}