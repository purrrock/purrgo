#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include "purrgo/display_hal.h"
#include "purrgo/hardware_config.h"

int mock_full_refresh_count = 0;
int mock_partial_refresh_count = 0;
int last_x = -1, last_y = -1, last_w = -1, last_h = -1;

void reset_mock_state() {
    mock_full_refresh_count = 0;
    mock_partial_refresh_count = 0;
    last_x = -1;
    last_y = -1;
    last_w = -1;
    last_h = -1;
}

// Mock logger to intercept display HAL logs
void purrgo_logger_write(const char *format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (strstr(buf, "FULL REFRESH") != NULL) {
        mock_full_refresh_count++;
    } else if (strstr(buf, "PARTIAL REFRESH") != NULL) {
        mock_partial_refresh_count++;
        sscanf(buf, "PARTIAL REFRESH x=%d y=%d w=%d h=%d", &last_x, &last_y, &last_w, &last_h);
    }
}

// To compile the test as an independent unit and not pull the whole HAL platform:
// We declare the functions so we can link against the real library, we shouldn't #include the source.
// However, the test uses purrgo_logger_write intercept which means we must override it,
// which is usually done by linking the mock object before the real object, or the build system does it.
// The user asks NOT to include `src/platform/pc/display_hal.c` directly, but rather test public API.
// Since the project already links `libpurrgo_platform_pc.a` into tests, our mock of `purrgo_logger_write`
// should be picked up correctly if we link it. Let's make sure it does.

void test_normal_accumulation() {
    display_refresh(); // Resets internal state
    reset_mock_state();

    // Accumulate multiple regions
    display_refresh_region(10, 10, 20, 20); // x2=29, y2=29
    display_refresh_region(100, 100, 50, 50); // x2=149, y2=149

    // Counter shouldn't increment until flush, so no logs yet
    assert(mock_partial_refresh_count == 0);
    assert(mock_full_refresh_count == 0);

    display_flush();

    assert(mock_partial_refresh_count == 1);
    assert(mock_full_refresh_count == 0);
    assert(last_x == 10);
    assert(last_y == 10);
    assert(last_w == 140); // 149 - 10 + 1
    assert(last_h == 140); // 149 - 10 + 1
}

void test_out_of_bounds_accumulation() {
    display_refresh();
    reset_mock_state();

    // Partial out of bounds
    display_refresh_region(-10, -10, 50, 50);

    display_flush();
    assert(mock_partial_refresh_count == 1);
    assert(last_x == 0);
    assert(last_y == 0);
    assert(last_w == 40); // 39 - 0 + 1
    assert(last_h == 40);

    // Completely out of bounds (negative)
    display_refresh_region(-50, -50, 10, 10);
    display_flush(); // Should do nothing because region was ignored
    assert(mock_partial_refresh_count == 1); // No change

    // Completely out of bounds (positive)
    display_refresh_region(PURRGO_HW_DISPLAY_WIDTH_PX + 10, PURRGO_HW_DISPLAY_HEIGHT_PX + 10, 10, 10);
    display_flush(); // Should do nothing
    assert(mock_partial_refresh_count == 1); // No change

    // Extending beyond right/bottom
    display_refresh_region(PURRGO_HW_DISPLAY_WIDTH_PX - 10, PURRGO_HW_DISPLAY_HEIGHT_PX - 10, 50, 50);
    display_flush();
    assert(mock_partial_refresh_count == 2);
    assert(last_x == PURRGO_HW_DISPLAY_WIDTH_PX - 10);
    assert(last_y == PURRGO_HW_DISPLAY_HEIGHT_PX - 10);
    assert(last_w == 10);
    assert(last_h == 10);
}

void test_partial_refresh_limit() {
    display_refresh();
    reset_mock_state();

    for (int i = 0; i < MAX_PARTIAL_REFRESHES; i++) {
        display_refresh_region(0, 0, 10, 10);
        display_flush();
        assert(mock_partial_refresh_count == i + 1);
    }

    assert(mock_full_refresh_count == 0);

    // Now we've reached the limit. The next flush should trigger a full refresh.
    display_refresh_region(0, 0, 10, 10);
    display_flush();

    assert(mock_full_refresh_count == 1);

    // Test that counter has reset, so next partial works
    reset_mock_state();
    display_refresh_region(0, 0, 10, 10);
    display_flush();
    assert(mock_partial_refresh_count == 1);
    assert(mock_full_refresh_count == 0);
}

int main() {
    test_normal_accumulation();
    test_out_of_bounds_accumulation();
    test_partial_refresh_limit();

    printf("display_hal advanced accumulation tests passed!\n");
    return 0;
}
