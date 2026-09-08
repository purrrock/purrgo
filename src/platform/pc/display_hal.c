#include "purrgo/display_hal.h"
#include "purrgo/logger.h"

static int partial_refresh_count = 0;

void display_refresh(void) {
    PURRGO_LOG("FULL REFRESH\r\n");
    partial_refresh_count = 0;
}

void display_refresh_region(int16_t x, int16_t y, int16_t w, int16_t h) {
    if (partial_refresh_count >= MAX_PARTIAL_REFRESHES) {
        display_refresh();
    }
    PURRGO_LOG("PARTIAL REFRESH x=%d y=%d w=%d h=%d\r\n", x, y, w, h);
    partial_refresh_count++;
}
