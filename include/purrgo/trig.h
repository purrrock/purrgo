#ifndef PURRGO_TRIG_H
#define PURRGO_TRIG_H

/*
 * Единственная таблица синуса/косинуса проекта: sin_lut[91] / cos_lut[91]
 * (угол 0..90 градусов с шагом 1 градус, масштаб 10000), сгенерирована
 * tools/generate_sun_tables.py и лежит в src/core/sun_tables.c.
 * Здесь только приведение угла по квадрантам; широта в 1e-7 градуса
 * интерполируется в purrgo_geo_cos_10k() (geo.c).
 */

#include <stdint.h>
#include "purrgo/sun_tables.h"

/* sin(deg) * 10000; deg - целые градусы, любое значение (в т.ч. отрицательное). */
static inline int32_t purrgo_sin_10k(int32_t deg)
{
    deg %= 360;
    if (deg < 0) {
        deg += 360;
    }
    if (deg <= 90) {
        return sin_lut[deg];
    }
    if (deg <= 180) {
        return sin_lut[180 - deg];
    }
    if (deg <= 270) {
        return -sin_lut[deg - 180];
    }
    return -sin_lut[360 - deg];
}

/* cos(deg) * 10000; deg - целые градусы, любое значение. */
static inline int32_t purrgo_cos_10k(int32_t deg)
{
    return purrgo_sin_10k(deg + 90);
}

#endif /* PURRGO_TRIG_H */
