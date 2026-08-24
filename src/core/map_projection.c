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
            ? (dx / width)
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
                (dy / height)
            )
            : 0;

    projected_y +=
        (int64_t)vp->offset_y;

    if (projected_x < -32768) {
        projected_x = -32768;
    }
    if (projected_x > 32767) {
        projected_x = 32767;
    }
    if (projected_y < -32768) {
        projected_y = -32768;
    }
    if (projected_y > 32767) {
        projected_y = 32767;
    }

    *sx = (int16_t)projected_x;
    *sy = (int16_t)projected_y;
}
