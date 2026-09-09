#include "purrgo/gfx_line.h"
#include "purrgo/gfx_renderer.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define WIDTH 64
#define HEIGHT 64

static uint8_t framebuffer[WIDTH * HEIGHT];
static int pixels_drawn = 0;

static void draw_pixel(void *user_data, int16_t x, int16_t y, gfx_color_t color) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        if (framebuffer[y * WIDTH + x] != color) {
            pixels_drawn++;
        }
        framebuffer[y * WIDTH + x] = (uint8_t)color;
    }
}

static gfx_color_t read_pixel(void *user_data, int16_t x, int16_t y) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        return framebuffer[y * WIDTH + x];
    }
    return 0;
}

static void reset_framebuffer(gfx_context_t *ctx) {
    memset(framebuffer, 0, sizeof(framebuffer));
    pixels_drawn = 0;
    gfx_init(ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    ctx->color_fg = 1;
}

static bool check_pixel(int16_t x, int16_t y, uint8_t expected) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        return framebuffer[y * WIDTH + x] == expected;
    }
    return expected == 0;
}

static void print_framebuffer(void) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            printf("%c", framebuffer[y * WIDTH + x] ? '#' : '.');
        }
        printf("\n");
    }
}

static bool test_horizontal_line() {
    gfx_context_t ctx;
    reset_framebuffer(&ctx);

    gfx_draw_line(&ctx, 10, 10, 20, 10);

    bool passed = true;
    for (int i = 10; i <= 20; i++) {
        if (!check_pixel(i, 10, 1)) {
            passed = false;
        }
    }
    if (pixels_drawn != 11) {
        passed = false;
    }

    if (passed) printf("PASSED: test_horizontal_line\n");
    else printf("FAILED: test_horizontal_line\n");
    return passed;
}

static bool test_vertical_line() {
    gfx_context_t ctx;
    reset_framebuffer(&ctx);

    gfx_draw_line(&ctx, 15, 5, 15, 15);

    bool passed = true;
    for (int i = 5; i <= 15; i++) {
        if (!check_pixel(15, i, 1)) {
            passed = false;
        }
    }
    if (pixels_drawn != 11) passed = false;

    if (passed) printf("PASSED: test_vertical_line\n");
    else printf("FAILED: test_vertical_line\n");
    return passed;
}

static bool test_diagonal_line() {
    gfx_context_t ctx;
    reset_framebuffer(&ctx);

    gfx_draw_line(&ctx, 0, 0, 5, 5);

    bool passed = true;
    for (int i = 0; i <= 5; i++) {
        if (!check_pixel(i, i, 1)) {
            passed = false;
        }
    }
    if (pixels_drawn != 6) passed = false;

    if (passed) printf("PASSED: test_diagonal_line\n");
    else printf("FAILED: test_diagonal_line\n");
    return passed;
}

static bool test_line_clipping() {
    gfx_context_t ctx;
    reset_framebuffer(&ctx);

    // Set clip rect smaller than framebuffer
    ctx.clip_x = 10;
    ctx.clip_y = 10;
    ctx.clip_w = 20;
    ctx.clip_h = 20;

    // Line crosses the clip rect from left to right
    gfx_draw_line(&ctx, 5, 15, 35, 15);

    bool passed = true;

    // Check outside clipping area (left)
    for (int i = 5; i < 10; i++) {
        if (!check_pixel(i, 15, 0)) passed = false;
    }

    // Check inside clipping area
    for (int i = 10; i < 30; i++) {
        if (!check_pixel(i, 15, 1)) passed = false;
    }

    // Check outside clipping area (right)
    for (int i = 30; i <= 35; i++) {
        if (!check_pixel(i, 15, 0)) passed = false;
    }

    if (passed) printf("PASSED: test_line_clipping\n");
    else printf("FAILED: test_line_clipping\n");
    return passed;
}

static bool test_dashed_line() {
    gfx_context_t ctx;
    reset_framebuffer(&ctx);

    // 4 pixel dash, 4 pixel gap
    gfx_draw_dashed_line(&ctx, 10, 10, 30, 10);

    bool passed = true;

    // Check dash 1 (10-13)
    if (!check_pixel(10, 10, 1) || !check_pixel(13, 10, 1)) passed = false;
    // Check gap 1 (14-17)
    if (!check_pixel(14, 10, 0) || !check_pixel(17, 10, 0)) passed = false;
    // Check dash 2 (18-21)
    if (!check_pixel(18, 10, 1) || !check_pixel(21, 10, 1)) passed = false;

    if (passed) printf("PASSED: test_dashed_line\n");
    else printf("FAILED: test_dashed_line\n");
    return passed;
}

static bool test_thick_line() {
    gfx_context_t ctx;
    reset_framebuffer(&ctx);

    gfx_draw_thick_line(&ctx, 10, 10, 20, 10, 3);

    bool passed = true;

    for (int x = 10; x <= 20; x++) {
        for (int y = 9; y <= 11; y++) {
            if (!check_pixel(x, y, 1)) {
                passed = false;
                break;
            }
        }
    }

    // Test that the ends have caps
    if (!check_pixel(9, 10, 1) || !check_pixel(9, 9, 1) || !check_pixel(9, 11, 1)) passed = false;
    if (!check_pixel(21, 10, 1) || !check_pixel(21, 9, 1) || !check_pixel(21, 11, 1)) passed = false;

    if (passed) printf("PASSED: test_thick_line\n");
    else printf("FAILED: test_thick_line\n");
    return passed;
}

static bool test_dotted_line() {
    gfx_context_t ctx;
    reset_framebuffer(&ctx);

    gfx_draw_dotted_line(&ctx, 10, 10, 20, 10);

    bool passed = true;
    for (int x = 10; x <= 20; x++) {
        int expected = (x - 10) % 2 == 0 ? 1 : 0;
        if (!check_pixel(x, 10, expected)) {
            passed = false;
        }
    }

    if (passed) printf("PASSED: test_dotted_line\n");
    else printf("FAILED: test_dotted_line\n");
    return passed;
}

static bool test_railway_line() {
    gfx_context_t ctx;
    reset_framebuffer(&ctx);

    // Fill background with a different color so we can see what railway draws
    memset(framebuffer, 2, sizeof(framebuffer)); // Fill with color 2

    gfx_draw_railway_line(&ctx, 10, 10, 30, 10);

    bool passed = true;

    // 3 pixels thick, BLACK(0) background, WHITE(3) dashes on top
    // Check some specific pixels

    // Dash 1 (10-13)
    if (!check_pixel(10, 10, WHITE)) passed = false;
    if (!check_pixel(10, 9, BLACK)) passed = false;
    if (!check_pixel(10, 11, BLACK)) passed = false;

    // Gap 1 (14-17)
    if (!check_pixel(14, 10, BLACK)) passed = false; // It's black in the gap because of the thick line underneath
    if (!check_pixel(14, 9, BLACK)) passed = false;
    if (!check_pixel(14, 11, BLACK)) passed = false;

    if (passed) printf("PASSED: test_railway_line\n");
    else {
        printf("FAILED: test_railway_line\n");
        // print_framebuffer();
    }
    return passed;
}

int main() {
    bool success = true;

    if (!test_horizontal_line()) success = false;
    if (!test_vertical_line()) success = false;
    if (!test_diagonal_line()) success = false;
    if (!test_line_clipping()) success = false;
    if (!test_dashed_line()) success = false;
    if (!test_thick_line()) success = false;
    if (!test_dotted_line()) success = false;
    if (!test_railway_line()) success = false;

    if (success) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}
