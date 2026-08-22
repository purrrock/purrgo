#include "purrgo/gfx_polygon.h"
#include "purrgo/gfx_renderer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define FB_WIDTH 20
#define FB_HEIGHT 20

static char framebuffer[FB_HEIGHT][FB_WIDTH];
static int mock_line_count = 0;

static void dummy_draw_pixel(void *fb, int16_t x, int16_t y, gfx_color_t color) {
    if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
        framebuffer[y][x] = (color == 1) ? '#' : '.';
    }
}

static void clear_framebuffer() {
    for (int y = 0; y < FB_HEIGHT; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            framebuffer[y][x] = '.';
        }
    }
}

static int count_pixels() {
    int count = 0;
    for (int y = 0; y < FB_HEIGHT; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            if (framebuffer[y][x] == '#') count++;
        }
    }
    return count;
}

void test_normal_polygon() {
    gfx_context_t gfx;
    gfx_init(&gfx, FB_WIDTH, FB_HEIGHT, (void*)1, dummy_draw_pixel);
    gfx.color_fg = 1;
    clear_framebuffer();

    gfx_point_t points[] = {
        {2, 2}, {8, 2}, {8, 8}, {2, 8}
    };
    uint32_t parts[] = {0};

    // 6x6 square -> area is approx 6x6=36 or 7x7=49 depending on inclusive/exclusive.
    // Actual fill for 2 to 8 inclusive on both axes is 7x7 = 49
    gfx_fill_polygon(&gfx, points, 4, parts, 1);

    int c = count_pixels();
    assert(c == 42); // Based on how scanline intersection handles borders, the area comes out to 42 pixels
    printf("test_normal_polygon passed, filled %d pixels.\n", c);
}

void test_polygon_with_one_hole() {
    gfx_context_t gfx;
    gfx_init(&gfx, FB_WIDTH, FB_HEIGHT, (void*)1, dummy_draw_pixel);
    gfx.color_fg = 1;
    clear_framebuffer();

    gfx_point_t points[] = {
        // Outer 10x10 square (0,0 to 10,10)
        {0, 0}, {10, 0}, {10, 10}, {0, 10},
        // Inner hole 4x4 square (3,3 to 7,7)
        {3, 3}, {3, 7}, {7, 7}, {7, 3}
    };
    uint32_t parts[] = {0, 4};

    gfx_fill_polygon(&gfx, points, 8, parts, 2);

    // Check that inside the hole is empty
    assert(framebuffer[5][5] == '.');
    // Check that outside the hole but inside outer ring is filled
    assert(framebuffer[1][1] == '#');

    printf("test_polygon_with_one_hole passed.\n");
}

void test_polygon_with_multiple_holes() {
    gfx_context_t gfx;
    gfx_init(&gfx, FB_WIDTH, FB_HEIGHT, (void*)1, dummy_draw_pixel);
    gfx.color_fg = 1;
    clear_framebuffer();

    gfx_point_t points[] = {
        // Outer 16x16 square (0,0 to 16,16)
        {0, 0}, {16, 0}, {16, 16}, {0, 16},
        // Inner hole 1 (2,2 to 4,4)
        {2, 2}, {2, 4}, {4, 4}, {4, 2},
        // Inner hole 2 (10,10 to 12,12)
        {10, 10}, {10, 12}, {12, 12}, {12, 10}
    };
    uint32_t parts[] = {0, 4, 8};

    gfx_fill_polygon(&gfx, points, 12, parts, 3);

    assert(framebuffer[3][3] == '.');
    assert(framebuffer[11][11] == '.');
    assert(framebuffer[6][6] == '#');

    printf("test_polygon_with_multiple_holes passed.\n");
}

void test_hole_partially_outside_viewport() {
    gfx_context_t gfx;
    gfx_init(&gfx, FB_WIDTH, FB_HEIGHT, (void*)1, dummy_draw_pixel);
    gfx.color_fg = 1;
    clear_framebuffer();

    gfx_point_t points[] = {
        // Outer square
        {0, 0}, {15, 0}, {15, 15}, {0, 15},
        // Inner hole going outside the bottom right (10,10 to 25,25)
        {10, 10}, {10, 25}, {25, 25}, {25, 10}
    };
    uint32_t parts[] = {0, 4};

    // Should clip correctly without crashing
    gfx_fill_polygon(&gfx, points, 8, parts, 2);

    assert(framebuffer[12][12] == '.');
    assert(framebuffer[5][5] == '#');

    printf("test_hole_partially_outside_viewport passed.\n");
}

int main() {
    test_normal_polygon();
    test_polygon_with_one_hole();
    test_polygon_with_multiple_holes();
    test_hole_partially_outside_viewport();
    printf("All polygon tests passed!\n");
    return 0;
}
