#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

int main() {
    int64_t map_vp_height = 222;
    int64_t target_y = 120;
    int64_t map_vp_offset_y = 9;
    int64_t geo_height = 1000000;
    int64_t rad_y = geo_height / 2;

    int64_t y_term = map_vp_height - target_y + map_vp_offset_y;
    int64_t candidate_lat = 0 - (y_term * geo_height) / map_vp_height + rad_y;

    printf("y_term: %ld\n", y_term);
    printf("candidate_lat: %ld\n", candidate_lat);
    return 0;
}
