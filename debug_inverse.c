#include <stdio.h>
#include <stdint.h>

int main() {
    int64_t target_x = 16;
    int64_t map_vp_offset_x = 0;
    int64_t map_vp_width = 400;

    // suppose we want lon such that project_x == target_x
    // project_x = (lon - min_x) * width_px / geo_width + offset_x
    // target_x - offset_x = (lon - (center - geo_width/2)) * width_px / geo_width
    // target_x - offset_x = (lon - center + geo_width/2) * width_px / geo_width
    // (target_x - offset_x) * geo_width / width_px = lon - center + geo_width/2
    // center = lon + geo_width/2 - (target_x - offset_x) * geo_width / width_px

    printf("Done\n");
    return 0;
}
