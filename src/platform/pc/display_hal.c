#include "purrgo/display_hal.h"
#include <stdio.h>

void display_refresh(void) {
    printf("FULL REFRESH\n");
}

void display_refresh_region(int16_t x, int16_t y, int16_t w, int16_t h) {
    printf("PARTIAL REFRESH x=%d y=%d w=%d h=%d\n", x, y, w, h);
}
