#include "purrgo/gfx_circle.h"
#include "purrgo/gfx_renderer.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define WIDTH 64
#define HEIGHT 64

static uint8_t framebuffer[WIDTH * HEIGHT];
static int pixels_drawn = 0;
static int pixels_read = 0;

static void draw_pixel(void *user_data, int16_t x, int16_t y, gfx_color_t color) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        if (framebuffer[y * WIDTH + x] == 0 && color != 0) {
            pixels_drawn++;
        } else if (framebuffer[y * WIDTH + x] != 0 && color == 0) {
            pixels_drawn--;
        }
        framebuffer[y * WIDTH + x] = (uint8_t)color;
    }
}

static gfx_color_t read_pixel(void *user_data, int16_t x, int16_t y) {
    pixels_read++;
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        return framebuffer[y * WIDTH + x];
    }
    return 0;
}

static void reset_framebuffer(gfx_context_t *ctx) {
    memset(framebuffer, 0, sizeof(framebuffer));
    pixels_drawn = 0;
    pixels_read = 0;
    ctx->color_bg = 0;
    ctx->color_fg = 1;
    // reset clipping region to match what gfx_init does
    gfx_reset_clip(ctx);
}

static void print_framebuffer() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            printf("%c", framebuffer[y * WIDTH + x] ? '0' + framebuffer[y * WIDTH + x] : '.');
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

static bool test_draw_circle() {
    printf("Running test_draw_circle...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);
    ctx.color_fg = 1; // Mark edge with 1

    int16_t cx = 32;
    int16_t cy = 32;
    int16_t r = 10;

    gfx_draw_circle(&ctx, cx, cy, r);

    bool passed = true;

    // Check center (inside, should be empty)
    if (!check_pixel(cx, cy, 0)) {
        printf("FAILED: Pixel at (%d,%d) should be 0\n", cx, cy);
        passed = false;
    }

    // Check specific edge points
    if (!check_pixel(cx + r, cy, 1)) {
        printf("FAILED: Pixel at right edge (%d,%d) should be 1\n", cx + r, cy);
        passed = false;
    }
    if (!check_pixel(cx - r, cy, 1)) {
        printf("FAILED: Pixel at left edge (%d,%d) should be 1\n", cx - r, cy);
        passed = false;
    }
    if (!check_pixel(cx, cy + r, 1)) {
        printf("FAILED: Pixel at bottom edge (%d,%d) should be 1\n", cx, cy + r);
        passed = false;
    }
    if (!check_pixel(cx, cy - r, 1)) {
        printf("FAILED: Pixel at top edge (%d,%d) should be 1\n", cx, cy - r);
        passed = false;
    }

    // Check outside point
    if (!check_pixel(cx + r + 2, cy, 0)) {
        printf("FAILED: Pixel outside (%d,%d) should be 0\n", cx + r + 2, cy);
        passed = false;
    }

    if (!passed) {
        printf("FAILED test_draw_circle\n");
        print_framebuffer();
    } else {
        printf("PASSED test_draw_circle (Count: %d)\n", pixels_drawn);
    }
    return passed;
}

static bool test_fill_circle() {
    printf("Running test_fill_circle...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);
    ctx.color_bg = 2; // filled with color_bg as per implementation of gfx_fill_circle
    ctx.color_fg = 1;

    int16_t cx = 20;
    int16_t cy = 20;
    int16_t r = 5;

    gfx_fill_circle(&ctx, cx, cy, r);

    bool passed = true;

    // Center should be color_bg
    if (!check_pixel(cx, cy, 2)) {
        printf("FAILED: Pixel at (%d,%d) should be 2\n", cx, cy);
        passed = false;
    }

    // Edges should be color_bg
    if (!check_pixel(cx + r, cy, 2)) {
        printf("FAILED: Pixel at right edge (%d,%d) should be 2\n", cx + r, cy);
        passed = false;
    }
    if (!check_pixel(cx - r, cy, 2)) {
        printf("FAILED: Pixel at left edge (%d,%d) should be 2\n", cx - r, cy);
        passed = false;
    }

    // Outside should be empty (0)
    if (!check_pixel(cx + r + 2, cy, 0)) {
        printf("FAILED: Pixel outside (%d,%d) should be 0\n", cx + r + 2, cy);
        passed = false;
    }

    // Check ctx.color_fg was restored
    if (ctx.color_fg != 1) {
        printf("FAILED: Context color_fg was not restored\n");
        passed = false;
    }

    if (!passed) {
        printf("FAILED test_fill_circle\n");
        print_framebuffer();
    } else {
        printf("PASSED test_fill_circle\n");
    }
    return passed;
}

static bool test_draw_poi_circle() {
    printf("Running test_draw_poi_circle...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);
    ctx.color_bg = 0;
    ctx.color_fg = 1;

    int16_t cx = 40;
    int16_t cy = 40;
    int16_t r = 8;

    gfx_draw_poi_circle(&ctx, cx, cy, r);

    bool passed = true;

    // POI center should be DARK_GRAY (1)
    if (!check_pixel(cx, cy, DARK_GRAY)) {
        printf("FAILED: POI center at (%d,%d) should be DARK_GRAY (%d)\n", cx, cy, DARK_GRAY);
        passed = false;
    }

    // POI edge should be WHITE (3)
    if (!check_pixel(cx + r, cy, WHITE)) {
        printf("FAILED: POI edge at (%d,%d) should be WHITE (%d)\n", cx + r, cy, WHITE);
        passed = false;
    }
    if (!check_pixel(cx, cy - r, WHITE)) {
        printf("FAILED: POI edge at (%d,%d) should be WHITE (%d)\n", cx, cy - r, WHITE);
        passed = false;
    }

    // Outside should be 0
    if (!check_pixel(cx + r + 2, cy, 0)) {
        printf("FAILED: Outside pixel at (%d,%d) should be 0\n", cx + r + 2, cy);
        passed = false;
    }

    // Check ctx.color_fg was restored
    if (ctx.color_fg != 1) {
        printf("FAILED: Context color_fg was not restored\n");
        passed = false;
    }

    if (!passed) {
        printf("FAILED test_draw_poi_circle\n");
        print_framebuffer();
    } else {
        printf("PASSED test_draw_poi_circle\n");
    }
    return passed;
}

static bool test_circle_edge_cases() {
    printf("Running test_circle_edge_cases...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    bool passed = true;

    // Test negative radius - should return silently without drawing
    gfx_draw_circle(&ctx, 10, 10, -5);
    if (pixels_drawn != 0) {
        printf("FAILED: drew pixels with negative radius\n");
        passed = false;
    }

    // Test null context
    gfx_draw_circle(NULL, 10, 10, 5);
    gfx_fill_circle(NULL, 10, 10, 5);
    gfx_draw_poi_circle(NULL, 10, 10, 5);
    // If it didn't crash, it passed this part.

    // Test context without draw_pixel
    gfx_context_t bad_ctx = ctx;
    bad_ctx.draw_pixel = NULL;
    gfx_fill_circle(&bad_ctx, 10, 10, 5);
    gfx_draw_poi_circle(&bad_ctx, 10, 10, 5);

    // Partially out of bounds drawing
    reset_framebuffer(&ctx);
    gfx_draw_circle(&ctx, 0, 0, 10);
    if (pixels_drawn == 0) {
        printf("FAILED: Partially out of bounds circle didn't draw any pixels\n");
        passed = false;
    }

    if (!passed) {
        printf("FAILED test_circle_edge_cases\n");
    } else {
        printf("PASSED test_circle_edge_cases\n");
    }

    return passed;
}

int main() {
    bool success = true;

    if (!test_draw_circle()) success = false;
    if (!test_fill_circle()) success = false;
    if (!test_draw_poi_circle()) success = false;
    if (!test_circle_edge_cases()) success = false;

    if (success) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}
