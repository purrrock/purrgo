#include <stdio.h>
#include <stdint.h>

int main() {
    int64_t cam_min_x = -1350000;
    int64_t cam_max_x = 1350000;
    int32_t vp_width = 400;
    int32_t vp_offset_x = 0;

    int32_t lon = 1350000 / 32; // geo_width / 32
    int64_t dx_raw = lon - cam_min_x;
    int64_t width = cam_max_x - cam_min_x;
    int64_t projected_x = (dx_raw * vp_width) / width + vp_offset_x;

    int16_t sx = (int16_t)projected_x;
    int16_t center_x = vp_offset_x + vp_width / 2;
    int32_t dx = sx - center_x;
    if (dx < 0) dx = -dx;

    int32_t follow_stop_x = vp_width / 16;
    printf("sx: %d, center_x: %d, dx: %d, follow_stop_x: %d\n", sx, center_x, dx, follow_stop_x);
    return 0;
}
