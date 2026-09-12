#include "purrgo/display_hal.h"
#include "purrgo/logger.h"
#include "purrgo/hardware_config.h"

static int partial_refresh_count = 0;

static int16_t pending_x1 = -1;
static int16_t pending_y1 = -1;
static int16_t pending_x2 = -1;
static int16_t pending_y2 = -1;
static int pending_has_region = 0;

void display_refresh(void) {
    PURRGO_LOG("FULL REFRESH\r\n");
    partial_refresh_count = 0;
    pending_has_region = 0;
}

void display_refresh_region(int16_t x, int16_t y, int16_t w, int16_t h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > PURRGO_HW_DISPLAY_WIDTH_PX) { w = PURRGO_HW_DISPLAY_WIDTH_PX - x; }
    if (y + h > PURRGO_HW_DISPLAY_HEIGHT_PX) { h = PURRGO_HW_DISPLAY_HEIGHT_PX - y; }

    if (w <= 0 || h <= 0) {
        return;
    }

    if (!pending_has_region) {
        pending_x1 = x;
        pending_y1 = y;
        pending_x2 = x + w - 1;
        pending_y2 = y + h - 1;
        pending_has_region = 1;
    } else {
        if (x < pending_x1) pending_x1 = x;
        if (y < pending_y1) pending_y1 = y;
        if (x + w - 1 > pending_x2) pending_x2 = x + w - 1;
        if (y + h - 1 > pending_y2) pending_y2 = y + h - 1;
    }
}

void display_flush(void) {
    if (!pending_has_region) {
        return;
    }

    int16_t w = pending_x2 - pending_x1 + 1;
    int16_t h = pending_y2 - pending_y1 + 1;

    if (partial_refresh_count >= MAX_PARTIAL_REFRESHES) {
        display_refresh();
    } else {
        PURRGO_LOG("PARTIAL REFRESH x=%d y=%d w=%d h=%d\r\n", pending_x1, pending_y1, w, h);
        partial_refresh_count++;
        pending_has_region = 0;
    }
}
