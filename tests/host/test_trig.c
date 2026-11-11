/*
 * Общая тригонометрия на целых числах (purrgo/trig.h, cos_lut/sin_lut из sun_tables.c):
 * квадранты, симметрия таблиц, интерполяция косинуса широты в purrgo_geo_cos_10k().
 */
#include <math.h>
#include <stdlib.h>
#include "purrgo/geo.h"
#include "purrgo/trig.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

int main(void)
{
    const double pi = 3.14159265358979323846;

    /* Таблица sin_lut и cos_lut симметричны: sin(90-d) == cos(d). */
    for (int d = 0; d <= 90; d++) {
        CHECK(sin_lut[90 - d] == cos_lut[d]);
    }

    /* Все целые градусы (в том числе вне 0..359 и отрицательные): отличие от libm <= 1 ед. из 10000. */
    for (int d = -720; d <= 720; d++) {
        const double r = (double)d * pi / 180.0;
        CHECK(abs(purrgo_sin_10k(d) - (int32_t)lround(sin(r) * 10000.0)) <= 1);
        CHECK(abs(purrgo_cos_10k(d) - (int32_t)lround(cos(r) * 10000.0)) <= 1);
    }
    CHECK(purrgo_sin_10k(0) == 0 && purrgo_cos_10k(0) == 10000);
    CHECK(purrgo_sin_10k(90) == 10000 && purrgo_cos_10k(90) == 0);
    CHECK(purrgo_sin_10k(180) == 0 && purrgo_cos_10k(180) == -10000);
    CHECK(purrgo_sin_10k(270) == -10000 && purrgo_cos_10k(270) == 0);
    CHECK(purrgo_sin_10k(-90) == -10000);

    /* Косинус широты (в 1e-7 градуса, знак не важен): ошибка интерполяции <= 2 ед. из 10000. */
    for (int64_t lat = -900000000; lat <= 900000000; lat += 1234567) {
        const double r = (double)lat * 1e-7 * pi / 180.0;
        const int32_t want = (int32_t)lround(cos(r) * 10000.0);
        CHECK(abs(purrgo_geo_cos_10k((int32_t)lat) - want) <= 2);
    }
    CHECK(purrgo_geo_cos_10k(0) == 10000);
    CHECK(purrgo_geo_cos_10k(900000000) == 0);
    CHECK(purrgo_geo_cos_10k(-900000000) == 0);
    CHECK(purrgo_geo_cos_10k(1800000000) == 0); /* за пределами: не выходит за таблицу */
    CHECK(purrgo_geo_cos_10k(535000000) == purrgo_geo_cos_10k(-535000000));

    return test_finish("test_trig");
}
