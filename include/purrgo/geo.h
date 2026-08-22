#ifndef PURRGO_GEO_H
#define PURRGO_GEO_H

#include <stdint.h>
#include "purrgo/map.h"

// Расчет BBox (bounding box) от центральной точки на основе физической ширины в метрах и пропорций экрана.
// Функция выполняет перевод из метров в градусы (формат 1e7) строго в целых числах.
void purrgo_geo_bbox_from_center(int32_t center_lat_1e7, int32_t center_lon_1e7, uint32_t width_m, const purrgo_viewport_t* vp, purrgo_bbox_t* out_bbox);

// Вычисление расстояния между двумя точками (в метрах)
// Координаты передаются в формате 1e7 (например, 53.7134383 -> 537134383)
uint32_t purrgo_geo_distance_m(int32_t lat1_1e7, int32_t lon1_1e7, int32_t lat2_1e7, int32_t lon2_1e7);

// Вычисление истинного азимута от первой точки ко второй (в градусах от 0 до 359)
uint16_t purrgo_geo_azimuth_deg(int32_t lat1_1e7, int32_t lon1_1e7, int32_t lat2_1e7, int32_t lon2_1e7);

// Вычисление косинуса широты, возвращает значение масштабированное на 10000
int32_t purrgo_geo_cos_10k(int32_t lat_1e7);

#endif // PURRGO_GEO_H