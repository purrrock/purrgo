#include <stdio.h>
#include <stdint.h>

int main() {
    int32_t dist_start_zone = 1296000;
    int32_t candidate_lon = 2538000;
    int32_t candidate_lat = 0;

    // forward project
    int64_t dynamic_cam_min_x = candidate_lon - 1350000;
    int64_t dx_raw = dist_start_zone - dynamic_cam_min_x; // 1296000 - 1188000 = 108000
    int64_t projected_x = (dx_raw * 400) / 2700000; // 108000 * 400 / 2700000 = 16!

    int16_t sx = (int16_t)projected_x;
    int32_t dx = sx - 200;
    if (dx < 0) dx = -dx;
    printf("new_sx=%d, dx=%d\n", sx, dx);
    return 0;
}
