#include <stdint.h>
#include <stdio.h>

int main() {
    int64_t dynamic_cam_min_x = -450000;
    int64_t dynamic_cam_max_x = 450000;
    int32_t vp_width = 400;

    int64_t geo_width = 900000;
    int32_t dist_stop_zone = geo_width / 32;
    int32_t fix_lon_1e7 = dist_stop_zone;

    int64_t dx_raw = fix_lon_1e7 - dynamic_cam_min_x;
    int64_t projected_x = (dx_raw * vp_width) / geo_width;

    int16_t sx = (int16_t)projected_x;
    int16_t center_x = vp_width / 2;
    int32_t dx = sx - center_x;
    if (dx < 0) dx = -dx;

    int32_t follow_stop_x = vp_width / 16;
    printf("dx: %d, stop_x: %d\n", dx, follow_stop_x);
    if (dx <= follow_stop_x) {
        printf("<= TRUE\n");
    } else {
        printf("<= FALSE\n");
    }
    return 0;
}
