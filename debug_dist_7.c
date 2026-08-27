#include <stdio.h>

int main() {
    printf("FALLBACK HIT! new_dx=73, follow_start_x=72\n");
    // wait, follow_start_x is 72!
    // Why is follow_start_x 72?!
    // vp_width / 2 - 16 = 200 - 16 = 184!
    // Why would follow_start_x be 72??
    // Look at app_fsm.c:
    // int32_t follow_start_x = (map_vp.width / 2) - AUTO_FOLLOW_EDGE_MARGIN_PX;
    // map_vp.width is PURRGO_HW_DISPLAY_WIDTH_PX.
    return 0;
}
