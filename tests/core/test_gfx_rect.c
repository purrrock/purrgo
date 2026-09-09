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
}

int main() {
    bool success = true;

    if (!test_draw_rect_basic()) success = false;
    if (!test_draw_rect_invalid()) success = false;
    if (!test_fill_rect_basic()) success = false;
    if (!test_fill_rect_clipping()) success = false;
    if (!test_fill_rect_invalid()) success = false;

    if (success) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}
