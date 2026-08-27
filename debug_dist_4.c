#include <stdio.h>
#include <stdint.h>

int main() {
    int64_t dynamic_cam_min_x = -1350000;
    int64_t dynamic_cam_max_x = 1350000;
    int32_t vp_width = 400;
    int32_t vp_offset_x = 0;

    int64_t geo_width = dynamic_cam_max_x - dynamic_cam_min_x;

    int32_t dist_start_zone = (geo_width * ((400 / 2) - 8)) / 400;
    int32_t fix_lon_1e7 = dist_start_zone;

    int64_t dx_raw = fix_lon_1e7 - dynamic_cam_min_x;
    int64_t projected_x = (dx_raw * vp_width) / geo_width;

    int16_t sx = (int16_t)projected_x;
    int16_t center_x = vp_width / 2;
    int32_t dx = sx - center_x;
    if (dx < 0) dx = -dx;

    int32_t follow_start_x = (vp_width / 2) - 16;
    printf("sx=%d, center_x=%d, dx=%d, follow_start_x=%d\n", sx, center_x, dx, follow_start_x);
    return 0;
}
