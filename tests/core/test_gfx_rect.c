#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_renderer.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define WIDTH 64
#define HEIGHT 64

static uint8_t framebuffer[WIDTH * HEIGHT];
static int pixels_drawn = 0;

static void draw_pixel(void *user_data, int16_t x, int16_t y, gfx_color_t color) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        if (framebuffer[y * WIDTH + x] == 0 && color != 0) {
            pixels_drawn++;
        }
#define WIDTH 100
#define HEIGHT 100

static uint8_t framebuffer[WIDTH * HEIGHT];
static int draw_calls = 0;

static void mock_draw_pixel(void *user_data, int16_t x, int16_t y, gfx_color_t color) {
    draw_calls++;
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
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
    ctx->color_bg = 0;
    ctx->color_fg = 1;
static void reset_test_state(gfx_context_t *ctx) {
    memset(framebuffer, 0, sizeof(framebuffer));
    ctx->color_bg = 2; // Use a specific color for fill
    ctx->color_fg = 1; // Use a specific color for draw
    draw_calls = 0;
    gfx_set_clip(ctx, 10, 10, 80, 80); // Set a clipping region
}

static bool check_pixel(int16_t x, int16_t y, uint8_t expected) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return false;
    }
    return framebuffer[y * WIDTH + x] == expected;
}

static bool test_draw_rect_basic() {
    printf("Running test_draw_rect_basic...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    gfx_draw_rect(&ctx, 10, 10, 20, 15);

    bool passed = true;

    // Check corners
    if (!check_pixel(10, 10, 1)) passed = false;
    if (!check_pixel(29, 10, 1)) passed = false;
    if (!check_pixel(10, 24, 1)) passed = false;
    if (!check_pixel(29, 24, 1)) passed = false;

    // Check inside (should be empty)
    if (!check_pixel(15, 15, 0)) passed = false;

    // Check outside (should be empty)
    if (!check_pixel(9, 10, 0)) passed = false;

    // Width 20, Height 15 => 20*2 + 13*2 = 40 + 26 = 66 pixels.
    if (pixels_drawn != 66) {
        printf("FAILED test_draw_rect_basic: Expected 66 pixels, got %d\n", pixels_drawn);
        passed = false;
    }

    if (!passed) {
        printf("FAILED test_draw_rect_basic\n");
    } else {
        printf("PASSED test_draw_rect_basic\n");
    }
    return passed;
}

static bool test_draw_rect_invalid() {
    printf("Running test_draw_rect_invalid...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    gfx_draw_rect(NULL, 10, 10, 20, 20);
    gfx_draw_rect(&ctx, 10, 10, 0, 20);
    gfx_draw_rect(&ctx, 10, 10, 20, -5);

    bool passed = (pixels_drawn == 0);

    if (!passed) {
        printf("FAILED test_draw_rect_invalid: Expected 0 pixels, got %d\n", pixels_drawn);
    } else {
        printf("PASSED test_draw_rect_invalid\n");
    }
    return passed;
}

static bool test_fill_rect_basic() {
    printf("Running test_fill_rect_basic...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    // Fill rect uses color_bg!
    ctx.color_bg = 2;

    gfx_fill_rect(&ctx, 10, 10, 10, 5);

    bool passed = true;

    // Check inside
    if (!check_pixel(15, 12, 2)) passed = false;

    // Check edges
    if (!check_pixel(10, 10, 2)) passed = false;
    if (!check_pixel(19, 14, 2)) passed = false;

    // Check outside
    if (!check_pixel(9, 10, 0)) passed = false;
    if (!check_pixel(20, 10, 0)) passed = false;
    if (!check_pixel(10, 15, 0)) passed = false;

    if (pixels_drawn != 50) {
        printf("FAILED test_fill_rect_basic: Expected 50 pixels, got %d\n", pixels_drawn);
        passed = false;
    }

    if (!passed) {
        printf("FAILED test_fill_rect_basic\n");
    } else {
        printf("PASSED test_fill_rect_basic\n");
    }
    return passed;
}

static bool test_fill_rect_clipping() {
    printf("Running test_fill_rect_clipping...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);
    ctx.color_bg = 3;

    gfx_set_clip(&ctx, 5, 5, 10, 10); // Clip region: x [5, 14], y [5, 14]

    gfx_fill_rect(&ctx, 0, 0, 20, 20); // Try to fill [0, 19] x [0, 19]

    bool passed = true;

    // Inside clip
    if (!check_pixel(5, 5, 3)) passed = false;
    if (!check_pixel(14, 14, 3)) passed = false;

    // Outside clip
    if (!check_pixel(4, 5, 0)) passed = false;
    if (!check_pixel(5, 4, 0)) passed = false;
    if (!check_pixel(15, 14, 0)) passed = false;
    if (!check_pixel(14, 15, 0)) passed = false;

    // Expected drawn pixels: 10 * 10 = 100
    if (pixels_drawn != 100) {
        printf("FAILED test_fill_rect_clipping: Expected 100 pixels, got %d\n", pixels_drawn);
        passed = false;
    }

    if (!passed) {
        printf("FAILED test_fill_rect_clipping\n");
    } else {
        printf("PASSED test_fill_rect_clipping\n");
    }
    return passed;
}

static bool test_fill_rect_invalid() {
    printf("Running test_fill_rect_invalid...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    gfx_fill_rect(NULL, 10, 10, 20, 20);
    gfx_fill_rect(&ctx, 10, 10, 0, 20);
    gfx_fill_rect(&ctx, 10, 10, 20, -5);

    ctx.draw_pixel = NULL;
    gfx_fill_rect(&ctx, 10, 10, 20, 20);

    bool passed = (pixels_drawn == 0);

    if (!passed) {
        printf("FAILED test_fill_rect_invalid: Expected 0 pixels, got %d\n", pixels_drawn);
    } else {
        printf("PASSED test_fill_rect_invalid\n");
    }
    return passed;
    if (framebuffer[y * WIDTH + x] != expected) {
        printf("FAILED: Pixel at (%d, %d) is %d, expected %d\n", x, y, framebuffer[y * WIDTH + x], expected);
        return false;
    }
    return true;
}

static bool test_gfx_fill_rect_null_context() {
    printf("Running test_gfx_fill_rect_null_context...\n");
    gfx_fill_rect(NULL, 0, 0, 10, 10);

    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, NULL, read_pixel);
    gfx_fill_rect(&ctx, 0, 0, 10, 10);

    printf("PASSED test_gfx_fill_rect_null_context\n");
    return true;
}

static bool test_gfx_fill_rect_invalid_dimensions() {
    printf("Running test_gfx_fill_rect_invalid_dimensions...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, mock_draw_pixel, read_pixel);
    reset_test_state(&ctx);

    gfx_fill_rect(&ctx, 20, 20, 0, 10);
    if (draw_calls != 0) return false;

    gfx_fill_rect(&ctx, 20, 20, 10, -5);
    if (draw_calls != 0) return false;

    printf("PASSED test_gfx_fill_rect_invalid_dimensions\n");
    return true;
}

static bool test_gfx_fill_rect_out_of_bounds() {
    printf("Running test_gfx_fill_rect_out_of_bounds...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, mock_draw_pixel, read_pixel);
    reset_test_state(&ctx);

    // Completely left
    gfx_fill_rect(&ctx, -20, 20, 10, 10);
    if (draw_calls != 0) return false;

    // Completely right
    gfx_fill_rect(&ctx, 95, 20, 10, 10);
    if (draw_calls != 0) return false;

    // Completely above
    gfx_fill_rect(&ctx, 20, -10, 10, 10);
    if (draw_calls != 0) return false;

    // Completely below
    gfx_fill_rect(&ctx, 20, 95, 10, 10);
    if (draw_calls != 0) return false;

    printf("PASSED test_gfx_fill_rect_out_of_bounds\n");
    return true;
}

static bool test_gfx_fill_rect_partial_intersection() {
    printf("Running test_gfx_fill_rect_partial_intersection...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, mock_draw_pixel, read_pixel);
    reset_test_state(&ctx);

    // Intersects left boundary (clip_x = 10, clip_y = 10)
    gfx_fill_rect(&ctx, 5, 20, 10, 10); // x from 5 to 14, clipped to 10 to 14 -> 5 pixels wide
    if (draw_calls != 50) return false; // 5 * 10

    reset_test_state(&ctx);
    // Intersects right boundary (clip_w = 80 -> right = 90)
    gfx_fill_rect(&ctx, 85, 20, 10, 10); // x from 85 to 94, clipped to 85 to 89 -> 5 pixels wide
    if (draw_calls != 50) return false;

    reset_test_state(&ctx);
    // Intersects top boundary (clip_y = 10)
    gfx_fill_rect(&ctx, 20, 5, 10, 10); // y from 5 to 14, clipped to 10 to 14 -> 5 pixels high
    if (draw_calls != 50) return false;

    reset_test_state(&ctx);
    // Intersects bottom boundary (clip_h = 80 -> bottom = 90)
    gfx_fill_rect(&ctx, 20, 85, 10, 10); // y from 85 to 94, clipped to 85 to 89 -> 5 pixels high
    if (draw_calls != 50) return false;

    printf("PASSED test_gfx_fill_rect_partial_intersection\n");
    return true;
}

static bool test_gfx_fill_rect_fully_inside() {
    printf("Running test_gfx_fill_rect_fully_inside...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, mock_draw_pixel, read_pixel);
    reset_test_state(&ctx);

    gfx_fill_rect(&ctx, 20, 20, 10, 10);
    if (draw_calls != 100) return false;

    // Check some pixels
    if (!check_pixel(19, 20, 0)) return false; // Outside left
    if (!check_pixel(20, 19, 0)) return false; // Outside top
    if (!check_pixel(20, 20, 2)) return false; // Inside top-left
    if (!check_pixel(29, 29, 2)) return false; // Inside bottom-right
    if (!check_pixel(30, 29, 0)) return false; // Outside right
    if (!check_pixel(29, 30, 0)) return false; // Outside bottom

    printf("PASSED test_gfx_fill_rect_fully_inside\n");
    return true;
}

static bool test_gfx_draw_rect_null_context() {
    printf("Running test_gfx_draw_rect_null_context...\n");
    gfx_draw_rect(NULL, 0, 0, 10, 10);

    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, NULL, read_pixel);
    gfx_draw_rect(&ctx, 0, 0, 10, 10);

    printf("PASSED test_gfx_draw_rect_null_context\n");
    return true;
}

static bool test_gfx_draw_rect_invalid_dimensions() {
    printf("Running test_gfx_draw_rect_invalid_dimensions...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, mock_draw_pixel, read_pixel);
    reset_test_state(&ctx);

    gfx_draw_rect(&ctx, 20, 20, 0, 10);
    if (draw_calls != 0) return false;

    gfx_draw_rect(&ctx, 20, 20, 10, -5);
    if (draw_calls != 0) return false;

    printf("PASSED test_gfx_draw_rect_invalid_dimensions\n");
    return true;
}

static bool test_gfx_draw_rect_bounds() {
    printf("Running test_gfx_draw_rect_bounds...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, mock_draw_pixel, read_pixel);
    reset_test_state(&ctx);

    gfx_draw_rect(&ctx, 20, 20, 10, 10);

    // Width 10, Height 10 => Perimeter is 10 + 10 + 8 + 8 = 36 pixels
    if (draw_calls != 36) {
        printf("FAILED: Expected 36 draw calls, got %d\n", draw_calls);
        return false;
    }

    // Check some pixels
    if (!check_pixel(20, 20, 1)) return false; // Top-left corner
    if (!check_pixel(29, 20, 1)) return false; // Top-right corner
    if (!check_pixel(20, 29, 1)) return false; // Bottom-left corner
    if (!check_pixel(29, 29, 1)) return false; // Bottom-right corner
    if (!check_pixel(25, 20, 1)) return false; // Top edge
    if (!check_pixel(25, 29, 1)) return false; // Bottom edge
    if (!check_pixel(20, 25, 1)) return false; // Left edge
    if (!check_pixel(29, 25, 1)) return false; // Right edge
    if (!check_pixel(25, 25, 0)) return false; // Inside

    printf("PASSED test_gfx_draw_rect_bounds\n");
    return true;
}

int main() {
    bool success = true;

    if (!test_draw_rect_basic()) success = false;
    if (!test_draw_rect_invalid()) success = false;
    if (!test_fill_rect_basic()) success = false;
    if (!test_fill_rect_clipping()) success = false;
    if (!test_fill_rect_invalid()) success = false;
    if (!test_gfx_fill_rect_null_context()) success = false;
    if (!test_gfx_fill_rect_invalid_dimensions()) success = false;
    if (!test_gfx_fill_rect_out_of_bounds()) success = false;
    if (!test_gfx_fill_rect_partial_intersection()) success = false;
    if (!test_gfx_fill_rect_fully_inside()) success = false;

    if (!test_gfx_draw_rect_null_context()) success = false;
    if (!test_gfx_draw_rect_invalid_dimensions()) success = false;
    if (!test_gfx_draw_rect_bounds()) success = false;

    if (success) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}
