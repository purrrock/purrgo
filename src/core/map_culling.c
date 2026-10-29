#include "map_culling.h"

bool bbox_intersects_camera(
    int32_t xmin,
    int32_t ymin,
    int32_t xmax,
    int32_t ymax,
    const purrgo_bbox_t *cam
) {
    if (ymax < cam->min_y || ymin > cam->max_y) {
        return false;
    }

    if (cam->min_x <= cam->max_x) {
        return (
            xmax >= cam->min_x &&
            xmin <= cam->max_x
        );
    }

    return (
        xmin <= cam->max_x ||
        xmax >= cam->min_x
    );
}

bool map_feature_too_small(
    int32_t xmin,
    int32_t ymin,
    int32_t xmax,
    int32_t ymax,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    int32_t min_px
) {
    if (min_px <= 0 || cam == NULL || vp == NULL || vp->width == 0u || vp->height == 0u) {
        return false;
    }

    /* Камера через антимеридиан (min_x > max_x) и вырожденная камера: не пропускаем. */
    if (cam->min_x >= cam->max_x || cam->min_y >= cam->max_y) {
        return false;
    }

    const int64_t span_x = (int64_t)cam->max_x - (int64_t)cam->min_x;
    const int64_t span_y = (int64_t)cam->max_y - (int64_t)cam->min_y;
    const int64_t dx = (int64_t)xmax - (int64_t)xmin;
    const int64_t dy = (int64_t)ymax - (int64_t)ymin;

    /*
     * Размер объекта в пикселях: d * viewport / span. Сравнение без деления:
     * d * viewport < min_px * span (произведения до ~2^48, int64 хватает).
     */
    return (dx * (int64_t)vp->width  < (int64_t)min_px * span_x) &&
           (dy * (int64_t)vp->height < (int64_t)min_px * span_y);
}
