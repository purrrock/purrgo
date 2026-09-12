#include <stdio.h>
#include <assert.h>
#include "purrgo/display_hal.h"
#include "purrgo/logger.h"
#include "purrgo/hardware_config.h"

// Mock logging to prevent unresolved references during compilation
void purrgo_logger_write(const char *format, ...) {
    (void)format;
}

// Define mocked dependencies since we are testing display_hal accumulation logic
// which resides in the platform-specific display implementations. We will test
// the PC version here by including the source file directly.
#include "../../src/platform/pc/display_hal.c"

void test_normal_accumulation() {
    display_refresh();

    // Accumulate multiple regions
    display_refresh_region(10, 10, 20, 20); // x2=29, y2=29
    assert(pending_has_region == 1);
    assert(pending_x1 == 10 && pending_y1 == 10 && pending_x2 == 29 && pending_y2 == 29);

    display_refresh_region(100, 100, 50, 50); // x2=149, y2=149
    assert(pending_has_region == 1);
    assert(pending_x1 == 10 && pending_y1 == 10 && pending_x2 == 149 && pending_y2 == 149);

    // Counter shouldn't increment until flush
    assert(partial_refresh_count == 0);

    display_flush();
    assert(partial_refresh_count == 1);
    assert(pending_has_region == 0);
}

void test_out_of_bounds_accumulation() {
    display_refresh();

    // Partial out of bounds
    display_refresh_region(-10, -10, 50, 50);
    assert(pending_has_region == 1);
    assert(pending_x1 == 0 && pending_y1 == 0 && pending_x2 == 39 && pending_y2 == 39);

    display_flush();
    assert(partial_refresh_count == 1);
    assert(pending_has_region == 0);

    // Completely out of bounds (negative)
    display_refresh_region(-50, -50, 10, 10);
    assert(pending_has_region == 0); // Should be ignored

    // Completely out of bounds (positive)
    display_refresh_region(PURRGO_HW_DISPLAY_WIDTH_PX + 10, PURRGO_HW_DISPLAY_HEIGHT_PX + 10, 10, 10);
    assert(pending_has_region == 0); // Should be ignored

    // Extending beyond right/bottom
    display_refresh_region(PURRGO_HW_DISPLAY_WIDTH_PX - 10, PURRGO_HW_DISPLAY_HEIGHT_PX - 10, 50, 50);
    assert(pending_has_region == 1);
    assert(pending_x1 == PURRGO_HW_DISPLAY_WIDTH_PX - 10);
    assert(pending_y1 == PURRGO_HW_DISPLAY_HEIGHT_PX - 10);
    assert(pending_x2 == PURRGO_HW_DISPLAY_WIDTH_PX - 1);
    assert(pending_y2 == PURRGO_HW_DISPLAY_HEIGHT_PX - 1);

    display_flush();
    assert(partial_refresh_count == 2);
}

void test_partial_refresh_limit() {
    display_refresh();
    assert(partial_refresh_count == 0);

    for (int i = 0; i < MAX_PARTIAL_REFRESHES; i++) {
        display_refresh_region(0, 0, 10, 10);
        display_flush();
        assert(partial_refresh_count == i + 1);
    }

    // Now we've reached the limit. The next flush should trigger a full refresh.
    display_refresh_region(0, 0, 10, 10);
    display_flush();

    // A full refresh resets the counter
    assert(partial_refresh_count == 0);
    assert(pending_has_region == 0);
}

int main() {
    test_normal_accumulation();
    test_out_of_bounds_accumulation();
    test_partial_refresh_limit();

    printf("display_hal advanced accumulation tests passed!\n");
    return 0;
}
