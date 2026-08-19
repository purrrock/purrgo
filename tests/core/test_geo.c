#include "purrgo/geo.h"
#include <assert.h>

int main(void) {
    // Тестовая точка A (0.0000000 N, 0.0000000 E)
    int32_t lat1 = 0;
    int32_t lon1 = 0;
    
    // Тестовая точка B (0.0000000 N, 0.0000000 E)
    int32_t lat2 = 0;
    int32_t lon2 = 0;

    // Базовый тест: дистанция и азимут между двумя одинаковыми точками равны нулю
    assert(purrgo_geo_distance_m(lat1, lon1, lat2, lon2) == 0U);
    assert(purrgo_geo_azimuth_deg(lat1, lon1, lat2, lon2) == 0U);

    // Дополнительный тест: смещение на север (ось Y)
    // 1 градус широты равен примерно 111195 метров
    int32_t lat3 = 10000000; // 1.0000000 N
    int32_t lon3 = 0;
    
    assert(purrgo_geo_distance_m(lat1, lon1, lat3, lon3) == 111195U);
    assert(purrgo_geo_azimuth_deg(lat1, lon1, lat3, lon3) == 0U); // Север

    // South
    int32_t lat4 = -10000000; // 1.0000000 S
    int32_t lon4 = 0;
    assert(purrgo_geo_distance_m(lat1, lon1, lat4, lon4) == 111195U);
    assert(purrgo_geo_azimuth_deg(lat1, lon1, lat4, lon4) == 180U); // Юг

    // East
    int32_t lat5 = 0;
    int32_t lon5 = 10000000; // 1.0000000 E
    assert(purrgo_geo_distance_m(lat1, lon1, lat5, lon5) == 111195U);
    assert(purrgo_geo_azimuth_deg(lat1, lon1, lat5, lon5) == 90U); // Восток

    // West
    int32_t lat6 = 0;
    int32_t lon6 = -10000000; // 1.0000000 W
    assert(purrgo_geo_distance_m(lat1, lon1, lat6, lon6) == 111195U);
    assert(purrgo_geo_azimuth_deg(lat1, lon1, lat6, lon6) == 270U); // Запад

    // Northeast (approx)
    assert(purrgo_geo_azimuth_deg(lat1, lon1, lat3, lon5) == 45U);

    // Southeast (approx)
    assert(purrgo_geo_azimuth_deg(lat1, lon1, lat4, lon5) == 135U);

    // Southwest (approx)
    assert(purrgo_geo_azimuth_deg(lat1, lon1, lat4, lon6) == 225U);

    // Northwest (approx)
    assert(purrgo_geo_azimuth_deg(lat1, lon1, lat3, lon6) == 315U);

    // Coordinate limit tests to verify int64 bounds correctly block overflow
    int32_t lat7 = 900000000; // 90 N
    int32_t lon7 = 1800000000; // 180 E
    int32_t lat8 = -900000000; // 90 S
    int32_t lon8 = -1800000000; // 180 W

    // Distance calculation should not assert/crash
    purrgo_geo_distance_m(lat7, lon7, lat8, lon8);

    // Azimuth test with wrap-around at 180th meridian
    // Moving from 179 E to -179 W (179 W) is a short distance East.
    // Coordinates:
    int32_t wrap_lon1 = 1790000000;
    int32_t wrap_lon2 = -1790000000;
    assert(purrgo_geo_azimuth_deg(0, wrap_lon1, 0, wrap_lon2) == 90U);

    // Distance moving across 180th should be equivalent to 2 degrees
    // We can't strictly compare distance precision easily due to projection,
    // but we can ensure it's not the huge difference of 358 degrees!
    uint32_t dist_across_wrap = purrgo_geo_distance_m(0, wrap_lon1, 0, wrap_lon2);
    // Since 2 degrees is 222390m, we ensure dist is approx around 222390m
    assert(dist_across_wrap > 222000U && dist_across_wrap < 223000U);

    return 0;
}