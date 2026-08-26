#include "purrgo/display_hal.h"
#include <stdio.h>

void display_refresh(void) {
    // No-op for PC/tests
}

void display_refresh_region(int16_t x, int16_t y, int16_t w, int16_t h) {
    // No-op for PC/tests
    (void)x; (void)y; (void)w; (void)h;
}
