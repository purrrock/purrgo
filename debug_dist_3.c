#include <stdio.h>
#include <stdint.h>

int main() {
    int64_t vp_width = 400;
    int64_t dynamic_cam_min_x = -1350000;
    int64_t dynamic_cam_max_x = 1350000;
    int64_t geo_width = dynamic_cam_max_x - dynamic_cam_min_x;
    int32_t dist_stop_zone = geo_width / 32; // 84375

    // forward project
    int64_t dx_raw = dist_stop_zone - dynamic_cam_min_x; // 84375 - (-1350000) = 1434375
    int64_t projected_x = (dx_raw * vp_width) / geo_width; // 1434375 * 400 / 2700000 = 573750000 / 2700000 = 212
    int16_t sx = (int16_t)projected_x;
    int16_t center_x = vp_width / 2; // 200
    int32_t dx = sx - center_x; // 212 - 200 = 12
    int32_t follow_stop_x = vp_width / 16; // 25

    // dx is 12 <= 25 !
    printf("dx = %d, follow_stop_x = %d\n", dx, follow_stop_x);
    return 0;
}
