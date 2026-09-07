#include "purrgo/display_hal.h"
#include "purrgo/logger.h"

void display_refresh(void) {
    PURRGO_LOG("FULL REFRESH\r\n");
}

void display_refresh_region(int16_t x, int16_t y, int16_t w, int16_t h) {
    PURRGO_LOG("PARTIAL REFRESH x=%d y=%d w=%d h=%d\r\n", x, y, w, h);
}
