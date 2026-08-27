#include <stdio.h>
#include <stdint.h>

int main() {
    int32_t vp_width = 176;
    int32_t follow_start_x = 72;
    int16_t sx = 167;
    int16_t center_x = 88;

    int32_t target_x = 2 * center_x - sx; // 176 - 167 = 9

    int32_t min_x = center_x - follow_start_x; // 88 - 72 = 16
    int32_t max_x = center_x + follow_start_x;
    if (target_x < min_x) target_x = min_x; // target_x clamped to 16!

    int64_t geo_width = 2700000;
    int32_t fix_lon_1e7 = 1227272; // dist_start_zone

    int64_t rad_x = geo_width / 2; // 1350000
    int64_t x_term = target_x; // 16
    int64_t candidate_lon = fix_lon_1e7 - (x_term * geo_width + vp_width / 2) / vp_width + rad_x;
    // 16 * 2700000 = 43200000. 43200000 + 88 = 43200088. / 176 = 245455.
    // 1227272 - 245455 + 1350000 = 2331817.

    // now calculate new_sx
    int64_t new_cam_min_x = candidate_lon - rad_x; // 2331817 - 1350000 = 981817
    int64_t dx_raw = fix_lon_1e7 - new_cam_min_x; // 1227272 - 981817 = 245455
    int64_t projected_x = (dx_raw * vp_width) / geo_width; // 245455 * 176 / 2700000 = 43200080 / 2700000 = 16!

    int16_t new_sx = (int16_t)projected_x; // 16
    int32_t new_dx = new_sx - center_x; // 16 - 88 = -72
    if (new_dx < 0) new_dx = -new_dx; // 72

    printf("new_sx=%d, new_dx=%d, follow_start_x=%d\n", new_sx, new_dx, follow_start_x);
    if (new_dx <= follow_start_x) {
        printf("<= is TRUE\n");
    } else {
        printf("<= is FALSE\n");
    }
    return 0;
}
