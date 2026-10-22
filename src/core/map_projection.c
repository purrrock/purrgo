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
    return num / den;
}

void project_to_screen(
    int32_t lon,
    int32_t lat,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    int16_t *sx,
    int16_t *sy
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

    if (projected_x < INT16_MIN) {
        projected_x = INT16_MIN;
    }
    if (projected_x > INT16_MAX) {
        projected_x = INT16_MAX;
    }
    if (projected_y < INT16_MIN) {
        projected_y = INT16_MIN;
    }
    if (projected_y > INT16_MAX) {
        projected_y = INT16_MAX;
    }

    *sx = (int16_t)projected_x;
    *sy = (int16_t)projected_y;
}
