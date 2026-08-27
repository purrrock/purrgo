#include <stdio.h>
#include <stdint.h>

int main() {
    int64_t dynamic_cam_min_x = -1350000;
    int64_t dynamic_cam_max_x = 1350000;
    int32_t vp_width = 400;

    int64_t geo_width = dynamic_cam_max_x - dynamic_cam_min_x;

    int32_t dist_start_zone = (geo_width * ((400 / 2) - 8)) / 400;
    int32_t fix_lon_1e7 = dist_start_zone;

    // forward project to find sx
    int64_t dx_raw = fix_lon_1e7 - dynamic_cam_min_x;
    int16_t sx = (int16_t)((dx_raw * vp_width) / geo_width); // 392

    // apply_auto_follow target opposite side:
    int16_t center_x = 200;
    int32_t target_x = 2 * center_x - sx; // 400 - 392 = 8

    // clamp
    int32_t follow_start_x = 200 - 16; // 184
    int32_t min_x = 200 - 184; // 16
    int32_t max_x = 200 + 184; // 384
    if (target_x < min_x) target_x = min_x; // 16

    // calculate candidate lon
    int64_t rad_x = geo_width / 2;
    int64_t x_term = target_x; // 16
    int64_t candidate_lon = fix_lon_1e7 - (x_term * geo_width) / vp_width + rad_x;

    printf("fix_lon_1e7=%d, candidate_lon=%ld\n", fix_lon_1e7, candidate_lon);

    return 0;
}
