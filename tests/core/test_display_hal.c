#include <stdio.h>
#include <assert.h>
#include "purrgo/display_hal.h"
#include "purrgo/logger.h"

// Mock logging to prevent unresolved references during compilation
void purrgo_logger_write(const char *format, ...) {
    (void)format;
}

// Define mocked dependencies since we are testing display_hal accumulation logic
// which resides in the platform-specific display implementations. We will test
// the PC version here by including the source file directly.
#include "../../src/platform/pc/display_hal.c"

int main() {
    // Reset internal state
    display_refresh();

    // Test that partial refreshes are accumulated and do not increment counter yet
    display_refresh_region(10, 10, 20, 20); // x2=29, y2=29
    assert(pending_has_region == 1);
    assert(pending_x1 == 10);
    assert(pending_y1 == 10);
    assert(pending_x2 == 29);
    assert(pending_y2 == 29);

    display_refresh_region(100, 100, 50, 50); // x2=149, y2=149
    assert(pending_has_region == 1);
    assert(pending_x1 == 10);
    assert(pending_y1 == 10);
    assert(pending_x2 == 149);
    assert(pending_y2 == 149);

    // Ensure counter hasn't been incremented by the region requests
    assert(partial_refresh_count == 0);

    // Now flush and verify counter increment
    display_flush();
    assert(partial_refresh_count == 1);
    assert(pending_has_region == 0);

    printf("display_hal accumulation tests passed!\n");
    return 0;
}
