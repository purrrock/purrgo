#include "map_projection.h"

int64_t camera_span_x(
    const purrgo_bbox_t *cam
) {
    if (cam->min_x <= cam->max_x) {
        return (
            (int64_t)cam->max_x -
            (int64_t)cam->min_x
        );
    }

    return (
        (int64_t)cam->max_x +
        3600000000LL
    ) - (int64_t)cam->min_x;
}

/*
 * Деление с усечением к нулю (как оператор / для int64_t), den > 0.
 *
 * На Cortex-M 64-битное деление - вызов __aeabi_ldivmod (сотни тактов), а
 * 32-битное - одна инструкция UDIV. Точки в пределах экрана почти всегда дают
 * числитель меньше 2^32 (например, 500 м ~ 45000 единиц * 176 px ~ 8e6),
 * поэтому быстрый путь покрывает практически все точки; результат при этом
 * совпадает с int64_t-делением для любых аргументов (проверяется тестом).
 */
static int64_t div_trunc(int64_t num, int64_t den)
{
    if (den > 0 && den <= (int64_t)UINT32_MAX) {
        if (num >= 0) {
            if (num <= (int64_t)UINT32_MAX) {
                return (int64_t)((uint32_t)num / (uint32_t)den);
            }
        } else if (num >= -(int64_t)UINT32_MAX) {
            return -(int64_t)((uint32_t)(-num) / (uint32_t)den);
        }
    }
    /* den > 0 - контракт функции; cppcheck не видит его из вызывающего кода. */
    // cppcheck-suppress zerodivcond
    return num / den;
}

/*
 * Проекция без насыщения в int16: результат в int64 (пиксели, возможно далеко
 * за пределами экрана). Общая часть project_to_screen() и project_to_screen_i32().
 */
static void project_i64(
    int32_t lon,
    int32_t lat,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    int64_t *px,
    int64_t *py
) {
    int64_t dx_raw =
        (int64_t)lon -
        (int64_t)cam->min_x;

    if (
        cam->min_x > cam->max_x &&
        dx_raw < 0
    ) {
        dx_raw += 3600000000LL;
    }

    int64_t dx =
        dx_raw *
        (int64_t)vp->width;

    int64_t width =
        camera_span_x(cam);

    int64_t projected_x =
        (width > 0)
            ? div_trunc(dx, width)
            : 0;

    projected_x +=
        (int64_t)vp->offset_x;

    int64_t dy =
        (
            (int64_t)lat -
            (int64_t)cam->min_y
        ) *
        (int64_t)vp->height;

    int64_t height =
        (int64_t)cam->max_y -
        (int64_t)cam->min_y;

    int64_t projected_y =
        (height > 0)
            ? (
                (int64_t)vp->height -
                div_trunc(dy, height)
            )
            : 0;

    projected_y +=
        (int64_t)vp->offset_y;

    *px = projected_x;
    *py = projected_y;
}

static int64_t clamp_i64(int64_t v, int64_t lo, int64_t hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

void project_to_screen_i32(
    int32_t lon,
    int32_t lat,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    int32_t *sx,
    int32_t *sy
) {
    int64_t px;
    int64_t py;

    project_i64(lon, lat, cam, vp, &px, &py);

    *sx = (int32_t)clamp_i64(px, -PURRGO_PROJ_LIMIT, PURRGO_PROJ_LIMIT);
    *sy = (int32_t)clamp_i64(py, -PURRGO_PROJ_LIMIT, PURRGO_PROJ_LIMIT);
}

void project_to_screen(
    int32_t lon,
    int32_t lat,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    int16_t *sx,
    int16_t *sy
) {
    int64_t px;
    int64_t py;

    project_i64(lon, lat, cam, vp, &px, &py);

    *sx = (int16_t)clamp_i64(px, INT16_MIN, INT16_MAX);
    *sy = (int16_t)clamp_i64(py, INT16_MIN, INT16_MAX);
}

/*
 * Отсечение отрезка по квадрату [-PURRGO_SAFE_COORD, PURRGO_SAFE_COORD]^2
 * (Коэн-Сазерленд, арифметика int64). Нужно, когда конец отрезка далеко за
 * экраном: насыщение координаты в int16 меняло наклон отрезка. Отрезок, оба
 * конца которого уже внутри квадрата, возвращается без изменений.
 */
#define OC_LEFT   1
#define OC_RIGHT  2
#define OC_BOTTOM 4
#define OC_TOP    8

static int safe_outcode(int32_t x, int32_t y)
{
    int code = 0;

    if (x < -PURRGO_SAFE_COORD) {
        code |= OC_LEFT;
    } else if (x > PURRGO_SAFE_COORD) {
        code |= OC_RIGHT;
    }

    if (y < -PURRGO_SAFE_COORD) {
        code |= OC_BOTTOM;
    } else if (y > PURRGO_SAFE_COORD) {
        code |= OC_TOP;
    }

    return code;
}

bool map_clip_segment_to_safe(
    int32_t *x0,
    int32_t *y0,
    int32_t *x1,
    int32_t *y1
) {
    int oc0 = safe_outcode(*x0, *y0);
    int oc1 = safe_outcode(*x1, *y1);

    /* Каждая итерация переносит один конец на границу: хватает 4 проходов на конец. */
    for (int iter = 0; iter < 8; iter++) {
        if ((oc0 | oc1) == 0) {
            return true;
        }
        if ((oc0 & oc1) != 0) {
            return false;
        }

        int out = (oc0 != 0) ? oc0 : oc1;
        int64_t dx = (int64_t)*x1 - (int64_t)*x0;
        int64_t dy = (int64_t)*y1 - (int64_t)*y0;
        int64_t x = 0;
        int64_t y = 0;

        if ((out & OC_TOP) != 0) {
            y = PURRGO_SAFE_COORD;
            x = (int64_t)*x0 + dx * (y - (int64_t)*y0) / dy;
        } else if ((out & OC_BOTTOM) != 0) {
            y = -PURRGO_SAFE_COORD;
            x = (int64_t)*x0 + dx * (y - (int64_t)*y0) / dy;
        } else if ((out & OC_RIGHT) != 0) {
            x = PURRGO_SAFE_COORD;
            y = (int64_t)*y0 + dy * (x - (int64_t)*x0) / dx;
        } else {
            x = -PURRGO_SAFE_COORD;
            y = (int64_t)*y0 + dy * (x - (int64_t)*x0) / dx;
        }

        if (out == oc0) {
            *x0 = (int32_t)x;
            *y0 = (int32_t)y;
            oc0 = safe_outcode(*x0, *y0);
        } else {
            *x1 = (int32_t)x;
            *y1 = (int32_t)y;
            oc1 = safe_outcode(*x1, *y1);
        }
    }

    return false;
}
