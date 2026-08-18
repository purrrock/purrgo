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

    return 0;
}