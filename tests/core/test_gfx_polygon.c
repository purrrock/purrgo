#include "purrgo/gfx_polygon.h"
#include "purrgo/gfx_renderer.h"
#include <stdio.h>
#include <string.h>

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

static void reset_framebuffer(gfx_context_t *ctx) {
    memset(framebuffer, 0, sizeof(framebuffer));
    pixels_drawn = 0;
    ctx->color_bg = 0;
    ctx->color_fg = 1;
}

static void print_framebuffer() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            printf("%c", framebuffer[y * WIDTH + x] ? '#' : '.');
        }
        printf("\n");
    }
}

static void test_simple_polygon() {
    printf("Running test_simple_polygon...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel);
    reset_framebuffer(&ctx);

    gfx_point_t points[] = {
        {10, 10},
        {50, 10},
        {50, 50},
        {10, 50}
    };
    uint32_t parts[] = {0};

    gfx_fill_polygon(&ctx, points, 4, parts, 1);

    // Expected area: 41x41 = 1681
    if (pixels_drawn != 1681) {
        printf("FAILED test_simple_polygon: Expected 1681 pixels, got %d\n", pixels_drawn);
        print_framebuffer();
    } else {
        printf("PASSED test_simple_polygon\n");
    }
}

static void test_polygon_with_one_hole() {
    printf("Running test_polygon_with_one_hole...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel);
    reset_framebuffer(&ctx);

    gfx_point_t points[] = {
        // Outer ring (CW)
        {10, 10},
        {50, 10},
        {50, 50},
        {10, 50},
        // Inner hole (CCW)
        {20, 20},
        {20, 40},
        {40, 40},
        {40, 20}
    };
    uint32_t parts[] = {0, 4};

    gfx_fill_polygon(&ctx, points, 8, parts, 2);

    // Outer: 41x41 = 1681
    // Hole: 21x21 = 441
    // Result: 1681 - 441 = 1240
    if (pixels_drawn != 1240) {
        printf("FAILED test_polygon_with_one_hole: Expected 1240 pixels, got %d\n", pixels_drawn);
        print_framebuffer();
    } else {
        printf("PASSED test_polygon_with_one_hole\n");
    }
}

static void test_polygon_with_multiple_holes() {
    printf("Running test_polygon_with_multiple_holes...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel);
    reset_framebuffer(&ctx);

    gfx_point_t points[] = {
        // Outer ring (CW)
        {10, 10},
        {50, 10},
        {50, 50},
        {10, 50},
        // Inner hole 1 (CCW)
        {15, 15},
        {15, 25},
        {25, 25},
        {25, 15},
        // Inner hole 2 (CCW)
        {35, 35},
        {35, 45},
        {45, 45},
        {45, 35}
    };
    uint32_t parts[] = {0, 4, 8};

    gfx_fill_polygon(&ctx, points, 12, parts, 3);

    // Outer: 41x41 = 1681
    // Hole 1: 11x11 = 121
    // Hole 2: 11x11 = 121
    // Result: 1681 - 242 = 1439
    if (pixels_drawn != 1439) {
        printf("FAILED test_polygon_with_multiple_holes: Expected 1439 pixels, got %d\n", pixels_drawn);
        print_framebuffer();
    } else {
        printf("PASSED test_polygon_with_multiple_holes\n");
    }
}

static void test_hole_partially_out_of_viewport() {
    printf("Running test_hole_partially_out_of_viewport...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel);
    reset_framebuffer(&ctx);

    gfx_point_t points[] = {
        // Outer ring (CW)
        {-10, -10},
        {70, -10},
        {70, 70},
        {-10, 70},
        // Inner hole (CCW) - intersecting the left/top edges
        {-20, -20},
        {-20, 20},
        {20, 20},
        {20, -20}
    };
    uint32_t parts[] = {0, 4};

    gfx_fill_polygon(&ctx, points, 8, parts, 2);

    // Total framebuffer is 64x64 = 4096
    // Hole inside viewport: x in [0, 20], y in [0, 20] -> 21x21 = 441
    // Result: 4096 - 441 = 3655
    if (pixels_drawn != 3655) {
        printf("FAILED test_hole_partially_out_of_viewport: Expected 3655 pixels, got %d\n", pixels_drawn);
        print_framebuffer();
    } else {
        printf("PASSED test_hole_partially_out_of_viewport\n");
    }
}

int main() {
    test_simple_polygon();
    test_polygon_with_one_hole();
    test_polygon_with_multiple_holes();
    test_hole_partially_out_of_viewport();
    return 0;
}