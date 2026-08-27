#include "purrgo/gfx_renderer.h"
#include "purrgo/gfx_line.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_polygon.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define WIDTH 176
#define HEIGHT 264

static uint8_t framebuffer[WIDTH * HEIGHT];

static void draw_pixel(void *user_data, int16_t x, int16_t y, gfx_color_t color) {
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
    ctx->color_bg = 0;
    ctx->color_fg = 1;
}

static bool check_protected_areas() {
    bool passed = true;
    for (int y = 0; y < 9; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (framebuffer[y * WIDTH + x] != 0) {
                printf("FAILED: Pixel at (%d, %d) modified in top protected area\n", x, y);
                passed = false;
            }
        }
    }
    for (int y = HEIGHT - 9; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (framebuffer[y * WIDTH + x] != 0) {
                printf("FAILED: Pixel at (%d, %d) modified in bottom protected area\n", x, y);
                passed = false;
            }
        }
    }
    return passed;
}

static bool check_pixel(int16_t x, int16_t y, uint8_t expected) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return false;
    }
    if (framebuffer[y * WIDTH + x] != expected) {
        printf("FAILED: Pixel at (%d, %d) is %d, expected %d\n", x, y, framebuffer[y * WIDTH + x], expected);
        return false;
    }
    return true;
}

static bool test_clipping_bounds() {
    printf("Running test_clipping_bounds...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    // Initial state check
    if (ctx.clip_x != 0 || ctx.clip_y != 0 || ctx.clip_w != WIDTH || ctx.clip_h != HEIGHT) {
        printf("FAILED: Default clipping not set correctly\n");
        return false;
    }

    gfx_set_clip(&ctx, 0, 9, WIDTH, HEIGHT - 18);

    if (ctx.clip_x != 0 || ctx.clip_y != 9 || ctx.clip_w != WIDTH || ctx.clip_h != HEIGHT - 18) {
        printf("FAILED: gfx_set_clip did not set bounds correctly\n");
        return false;
    }

    // Try setting out of bounds
    gfx_set_clip(&ctx, -10, -5, WIDTH + 20, HEIGHT + 10);
    if (ctx.clip_x != 0 || ctx.clip_y != 0 || ctx.clip_w != WIDTH || ctx.clip_h != HEIGHT) {
        printf("FAILED: gfx_set_clip did not clamp correctly\n");
        return false;
    }

    gfx_reset_clip(&ctx);
    if (ctx.clip_x != 0 || ctx.clip_y != 0 || ctx.clip_w != WIDTH || ctx.clip_h != HEIGHT) {
        printf("FAILED: gfx_reset_clip did not reset correctly\n");
        return false;
    }

    printf("PASSED test_clipping_bounds\n");
    return true;
}

static bool test_draw_pixel_clipping() {
    printf("Running test_draw_pixel_clipping...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    gfx_set_clip(&ctx, 0, 9, WIDTH, HEIGHT - 18);

    gfx_draw_pixel(&ctx, 50, 5); // Top protected
    gfx_draw_pixel(&ctx, 50, HEIGHT - 5); // Bottom protected
    gfx_draw_pixel(&ctx, 50, 50); // Inside map

    bool passed = check_protected_areas();
    if (!check_pixel(50, 50, 1)) passed = false;

    if (passed) printf("PASSED test_draw_pixel_clipping\n");
    return passed;
}

static bool test_draw_line_clipping() {
    printf("Running test_draw_line_clipping...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    gfx_set_clip(&ctx, 0, 9, WIDTH, HEIGHT - 18);

    // Line from top to bottom
    gfx_draw_line(&ctx, 10, 0, 10, HEIGHT - 1);

    // Diagonal line
    gfx_draw_line(&ctx, 0, 0, WIDTH - 1, HEIGHT - 1);

    bool passed = check_protected_areas();

    // Check that part inside the map area is drawn
    if (!check_pixel(10, 50, 1)) passed = false;
    // (63, 148) is a point on the line after clipping
    if (!check_pixel(63, 148, 1)) passed = false;

    if (passed) printf("PASSED test_draw_line_clipping\n");
    return passed;
}

static bool test_draw_polygon_clipping() {
    printf("Running test_draw_polygon_clipping...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    gfx_set_clip(&ctx, 0, 9, WIDTH, HEIGHT - 18);

    // Large polygon that covers the whole screen
    gfx_point_t points[] = {
        {-10, -10},
        {WIDTH + 10, -10},
        {WIDTH + 10, HEIGHT + 10},
        {-10, HEIGHT + 10}
    };
    uint32_t parts[] = {0};

    gfx_fill_compound_polygon(&ctx, points, 4, parts, 1);

    bool passed = check_protected_areas();

    // Inside the map area should be filled
    if (!check_pixel(64, 148, 1)) passed = false;

    if (passed) printf("PASSED test_draw_polygon_clipping\n");
    return passed;
}

static bool test_draw_polygon_clipping_horizontal() {
    printf("Running test_draw_polygon_clipping_horizontal...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    gfx_set_clip(&ctx, 10, 0, WIDTH - 20, HEIGHT);

    // Large polygon that covers the whole screen horizontally
    gfx_point_t points[] = {
        {-10, 50},
        {WIDTH + 10, 50},
        {WIDTH + 10, 150},
        {-10, 150}
    };
    uint32_t parts[] = {0};

    gfx_fill_compound_polygon(&ctx, points, 4, parts, 1);

    bool passed = true;

    // Check protected left/right areas
    for (int y = 50; y < 150; y++) {
        if (!check_pixel(5, y, 0)) passed = false;
        if (!check_pixel(WIDTH - 5, y, 0)) passed = false;
    }

    // Inside the map area should be filled
    if (!check_pixel(64, 100, 1)) passed = false;

    if (passed) printf("PASSED test_draw_polygon_clipping_horizontal\n");
    return passed;
}

static bool test_draw_rect_clipping() {
    printf("Running test_draw_rect_clipping...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    gfx_set_clip(&ctx, 10, 10, WIDTH - 20, HEIGHT - 20);

    ctx.color_bg = 1;
    // Draw a rect that overlaps the clipping bounds
    gfx_fill_rect(&ctx, 0, 0, WIDTH, HEIGHT);

    bool passed = true;

    // Check corners outside the clip
    if (!check_pixel(5, 5, 0)) passed = false;
    if (!check_pixel(WIDTH - 5, HEIGHT - 5, 0)) passed = false;

    // Check inside the clip
    if (!check_pixel(20, 20, 1)) passed = false;

    if (passed) printf("PASSED test_draw_rect_clipping\n");
    return passed;
}

static bool test_draw_hv_line_clipping() {
    printf("Running test_draw_hv_line_clipping...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    gfx_set_clip(&ctx, 10, 10, WIDTH - 20, HEIGHT - 20);

    // Draw lines that overlap the boundaries
    gfx_draw_hline(&ctx, 0, WIDTH, 50);
    gfx_draw_vline(&ctx, 50, 0, HEIGHT);

    bool passed = true;

    // Outside clip
    if (!check_pixel(5, 50, 0)) passed = false;
    if (!check_pixel(WIDTH - 5, 50, 0)) passed = false;
    if (!check_pixel(50, 5, 0)) passed = false;
    if (!check_pixel(50, HEIGHT - 5, 0)) passed = false;

    // Inside clip
    if (!check_pixel(20, 50, 1)) passed = false;
    if (!check_pixel(50, 20, 1)) passed = false;

    if (passed) printf("PASSED test_draw_hv_line_clipping\n");
    return passed;
}


int main() {
    bool success = true;

    if (!test_clipping_bounds()) success = false;
    if (!test_draw_pixel_clipping()) success = false;
    if (!test_draw_line_clipping()) success = false;
    if (!test_draw_polygon_clipping()) success = false;
    if (!test_draw_polygon_clipping_horizontal()) success = false;
    if (!test_draw_rect_clipping()) success = false;
    if (!test_draw_hv_line_clipping()) success = false;

    if (success) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}
