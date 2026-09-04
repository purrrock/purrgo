#include "purrgo/gfx_polygon.h"
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

static void print_framebuffer() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            printf("%c", framebuffer[y * WIDTH + x] ? '#' : '.');
        }
        printf("\n");
    }
}

static bool check_pixel(int16_t x, int16_t y, uint8_t expected) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return false;
    }
    return framebuffer[y * WIDTH + x] == expected;
}

static bool test_simple_polygon() {
    printf("Running test_simple_polygon...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    gfx_point_t points[] = {
        {10, 10},
        {50, 10},
        {50, 50},
        {10, 50}
    };
    uint32_t parts[] = {0};

    gfx_fill_compound_polygon(&ctx, points, 4, parts, 1);

    // Bounds of gfx_draw_hline(x_start, x_end) fills up to x_end inclusive
    // [10, 49] x [10, 49]
    // Width is 40 (10 to 49), height is 40 (10 to 49) = 1600.
    // However, because of Bresenham/scanline logic:
    // (50 - 10) * (50 - 10) -> exactly 40x40.
    // The exact count depends on the intersection algorithm. Let's explicitly check pixels instead of just a raw count.

    bool passed = true;

    // Check center (inside)
    if (!check_pixel(30, 30, 1)) {
        printf("FAILED: Pixel at (30,30) should be 1\n");
        passed = false;
    }
    // Check outside (left)
    if (!check_pixel(5, 30, 0)) {
        printf("FAILED: Pixel at (5,30) should be 0\n");
        passed = false;
    }
    // Check outside (right)
    if (!check_pixel(55, 30, 0)) {
        printf("FAILED: Pixel at (55,30) should be 0\n");
        passed = false;
    }

    if (!passed) {
        printf("FAILED test_simple_polygon\n");
        print_framebuffer();
    } else {
        printf("PASSED test_simple_polygon (Count: %d)\n", pixels_drawn);
    }
    return passed;
}

static bool test_polygon_with_one_hole() {
    printf("Running test_polygon_with_one_hole...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
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

    gfx_fill_compound_polygon(&ctx, points, 8, parts, 2);

    bool passed = true;

    // Check inside outer ring, outside hole
    if (!check_pixel(15, 30, 1)) {
        printf("FAILED: Pixel at (15,30) should be 1\n");
        passed = false;
    }

    // Check inside hole
    if (!check_pixel(30, 30, 0)) {
        printf("FAILED: Pixel at (30,30) should be 0 (hole)\n");
        passed = false;
    }

    // Check outside polygon
    if (!check_pixel(5, 30, 0)) {
        printf("FAILED: Pixel at (5,30) should be 0\n");
        passed = false;
    }

    if (!passed) {
        printf("FAILED test_polygon_with_one_hole\n");
        print_framebuffer();
    } else {
        printf("PASSED test_polygon_with_one_hole (Count: %d)\n", pixels_drawn);
    }
    return passed;
}

static bool test_polygon_with_multiple_holes() {
    printf("Running test_polygon_with_multiple_holes...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
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

    gfx_fill_compound_polygon(&ctx, points, 12, parts, 3);

    bool passed = true;

    // Inside polygon, outside holes
    if (!check_pixel(30, 30, 1)) passed = false;

    // Inside hole 1
    if (!check_pixel(20, 20, 0)) passed = false;

    // Inside hole 2
    if (!check_pixel(40, 40, 0)) passed = false;

    if (!passed) {
        printf("FAILED test_polygon_with_multiple_holes\n");
        print_framebuffer();
    } else {
        printf("PASSED test_polygon_with_multiple_holes (Count: %d)\n", pixels_drawn);
    }
    return passed;
}

static bool test_hole_partially_out_of_viewport() {
    printf("Running test_hole_partially_out_of_viewport...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
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

    gfx_fill_compound_polygon(&ctx, points, 8, parts, 2);

    bool passed = true;

    // Inside hole (on screen)
    if (!check_pixel(10, 10, 0)) passed = false;

    // Inside polygon (on screen)
    if (!check_pixel(40, 40, 1)) passed = false;

    if (!passed) {
        printf("FAILED test_hole_partially_out_of_viewport\n");
        print_framebuffer();
    } else {
        printf("PASSED test_hole_partially_out_of_viewport (Count: %d)\n", pixels_drawn);
    }
    return passed;
}

static bool test_polygon_too_many_nodes() {
    printf("Running test_polygon_too_many_nodes...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    // To hit >64 intersections on a single horizontal scanline,
    // we generate a single "comb" or "zigzag" polygon.
    // Each tooth of the comb goes up and down, generating 2 intersections per tooth.
    // 70 teeth = 140 intersections.
    gfx_point_t points[300]; // 70 * 4 + some buffer
    uint32_t parts[] = {0};
    uint16_t num_points = 0;

    int16_t start_x = 0;

    // Create the top zigzags
    for (int i = 0; i < 70; i++) {
        points[num_points++] = (gfx_point_t){start_x, 10};
        start_x++;
        points[num_points++] = (gfx_point_t){start_x, 30};
        start_x++;
    }

    // Close the polygon safely
    points[num_points++] = (gfx_point_t){start_x, 50};
    points[num_points++] = (gfx_point_t){0, 50};

    // The scanline at y=20 will intersect all the vertical lines.
    // It should hit >= 64 intersections and abort (return).
    // The screen shouldn't be filled completely inappropriately or cause memory faults.
    gfx_fill_compound_polygon(&ctx, points, num_points, parts, 1);

    bool passed = true;

    // Because it aborted midway, we expect significantly fewer pixels drawn, or zero for that scanline.
    // Mostly we want to ensure valgrind/asan doesn't crash here.
    if (pixels_drawn > 2000) {
        printf("FAILED test_polygon_too_many_nodes: Should have aborted, but drew too many pixels\n");
        passed = false;
    }

    if (!passed) {
        printf("FAILED test_polygon_too_many_nodes\n");
    } else {
        printf("PASSED test_polygon_too_many_nodes (Count: %d)\n", pixels_drawn);
    }
    return passed;
}

static bool test_even_odd_winding() {
    printf("Running test_even_odd_winding...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    gfx_point_t points[] = {
        // Outer ring (CW)
        {10, 10},
        {50, 10},
        {50, 50},
        {10, 50},
        // Inner hole (also CW - testing even-odd rule independence)
        {20, 20},
        {40, 20},
        {40, 40},
        {20, 40}
    };
    uint32_t parts[] = {0, 4};

    gfx_fill_compound_polygon(&ctx, points, 8, parts, 2);

    bool passed = true;

    // Check inside outer ring, outside hole
    if (!check_pixel(15, 30, 1)) {
        printf("FAILED: Pixel at (15,30) should be 1\n");
        passed = false;
    }

    // Check inside hole (should be empty even though both CW)
    if (!check_pixel(30, 30, 0)) {
        printf("FAILED: Pixel at (30,30) should be 0 (hole)\n");
        passed = false;
    }

    if (!passed) {
        printf("FAILED test_even_odd_winding\n");
        print_framebuffer();
    } else {
        printf("PASSED test_even_odd_winding (Count: %d)\n", pixels_drawn);
    }
    return passed;
}


int main() {
    bool success = true;

    if (!test_simple_polygon()) success = false;
    if (!test_polygon_with_one_hole()) success = false;
    if (!test_polygon_with_multiple_holes()) success = false;
    if (!test_hole_partially_out_of_viewport()) success = false;
    if (!test_polygon_too_many_nodes()) success = false;
    if (!test_even_odd_winding()) success = false;

    if (success) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}
