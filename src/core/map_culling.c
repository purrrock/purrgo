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
