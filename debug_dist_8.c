#include <stdio.h>
#include <stdint.h>

int main() {
    int64_t dynamic_cam_min_x = -1350000;
    int64_t dynamic_cam_max_x = 1350000;
    int32_t vp_width = 176;

    int64_t geo_width = 2700000;

    int32_t dist_start_zone = (geo_width * ((176 / 2) - 8)) / 176;
    int32_t fix_lon_1e7 = dist_start_zone;

    int64_t dx_raw = fix_lon_1e7 - dynamic_cam_min_x;
    int16_t sx = (int16_t)((dx_raw * vp_width) / geo_width);

    int16_t center_x = 88;
    int32_t dx = sx - center_x;
    if (dx < 0) dx = -dx;

    int32_t follow_start_x = 88 - 16;
    printf("sx=%d, center_x=%d, dx=%d, follow_start_x=%d\n", sx, center_x, dx, follow_start_x);
    return 0;
}
