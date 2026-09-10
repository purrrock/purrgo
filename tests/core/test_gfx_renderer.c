#include "purrgo/gfx_renderer.h"
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

static bool test_set_clip() {
    printf("Running test_set_clip...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);

    bool passed = true;

    // Test 1: Normal clipping
    gfx_set_clip(&ctx, 10, 20, 50, 60);
    if (ctx.clip_x != 10 || ctx.clip_y != 20 || ctx.clip_w != 50 || ctx.clip_h != 60) {
        printf("FAILED: test_set_clip normal clipping\n");
        passed = false;
    }

    // Test 2: Negative width and height
    gfx_set_clip(&ctx, 10, 20, -5, -10);
    if (ctx.clip_x != 10 || ctx.clip_y != 20 || ctx.clip_w != 0 || ctx.clip_h != 0) {
        printf("FAILED: test_set_clip negative width/height\n");
        passed = false;
    }

    // Test 3: Negative x/y
    gfx_set_clip(&ctx, -10, -20, 50, 60);
    if (ctx.clip_x != 0 || ctx.clip_y != 0 || ctx.clip_w != 40 || ctx.clip_h != 40) {
        printf("FAILED: test_set_clip negative x/y\n");
        passed = false;
    }

    // Test 4: Extremely negative x/y leading to negative dimensions
    gfx_set_clip(&ctx, -60, -70, 50, 60);
    if (ctx.clip_x != 0 || ctx.clip_y != 0 || ctx.clip_w != 0 || ctx.clip_h != 0) {
        printf("FAILED: test_set_clip extremely negative x/y\n");
        passed = false;
    }

    // Test 5: Out of bounds positive x/y
    gfx_set_clip(&ctx, WIDTH + 10, HEIGHT + 20, 50, 60);
    if (ctx.clip_x != WIDTH || ctx.clip_y != HEIGHT || ctx.clip_w != 0 || ctx.clip_h != 0) {
        printf("FAILED: test_set_clip out of bounds positive x/y\n");
        passed = false;
    }

    // Test 6: Dimensions exceeding boundaries
    gfx_set_clip(&ctx, WIDTH - 10, HEIGHT - 20, 50, 60);
    if (ctx.clip_x != WIDTH - 10 || ctx.clip_y != HEIGHT - 20 || ctx.clip_w != 10 || ctx.clip_h != 20) {
        printf("FAILED: test_set_clip dimensions exceeding boundaries\n");
        passed = false;
    }

    // Test 7: Null context
    gfx_set_clip(NULL, 10, 20, 50, 60); // Should not crash

    // Test 8: Context Reset Clip
    gfx_reset_clip(&ctx);
    if (ctx.clip_x != 0 || ctx.clip_y != 0 || ctx.clip_w != WIDTH || ctx.clip_h != HEIGHT) {
        printf("FAILED: test_set_clip reset_clip failed\n");
        passed = false;
    }

    if (passed) printf("PASSED test_set_clip\n");
    return passed;
}

static bool test_draw_hline() {
    printf("Running test_draw_hline...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    bool passed = true;

    // Test 1: Normal drawing
    gfx_draw_hline(&ctx, 10, 20, 50);
    for (int x = 10; x <= 20; x++) {
        if (!check_pixel(x, 50, 1)) passed = false;
    }
    if (!check_pixel(9, 50, 0)) passed = false;
    if (!check_pixel(21, 50, 0)) passed = false;

    // Test 2: Drawing backwards (x_start > x_end)
    reset_framebuffer(&ctx);
    gfx_draw_hline(&ctx, 30, 25, 60);
    for (int x = 25; x <= 30; x++) {
        if (!check_pixel(x, 60, 1)) passed = false;
    }

    // Test 3: Clipping Y (Out of bounds Y should not draw)
    reset_framebuffer(&ctx);
    gfx_set_clip(&ctx, 0, 10, WIDTH, HEIGHT - 20);
    gfx_draw_hline(&ctx, 10, 20, 5); // Should not draw
    if (!check_pixel(15, 5, 0)) passed = false;
    gfx_draw_hline(&ctx, 10, 20, HEIGHT - 5); // Should not draw
    if (!check_pixel(15, HEIGHT - 5, 0)) passed = false;

    // Test 4: Clipping X (Partially clipped)
    reset_framebuffer(&ctx);
    gfx_set_clip(&ctx, 10, 0, WIDTH - 20, HEIGHT);
    gfx_draw_hline(&ctx, 5, 15, 70); // Partially clipped left
    for (int x = 5; x < 10; x++) if (!check_pixel(x, 70, 0)) passed = false;
    for (int x = 10; x <= 15; x++) if (!check_pixel(x, 70, 1)) passed = false;

    gfx_draw_hline(&ctx, WIDTH - 15, WIDTH - 5, 80); // Partially clipped right
    for (int x = WIDTH - 15; x < WIDTH - 10; x++) if (!check_pixel(x, 80, 1)) passed = false;
    for (int x = WIDTH - 10; x <= WIDTH - 5; x++) if (!check_pixel(x, 80, 0)) passed = false;

    // Test 5: Completely outside X clipping
    reset_framebuffer(&ctx);
    gfx_draw_hline(&ctx, 2, 5, 90);
    if (!check_pixel(4, 90, 0)) passed = false;
    gfx_draw_hline(&ctx, WIDTH - 5, WIDTH - 2, 90);
    if (!check_pixel(WIDTH - 3, 90, 0)) passed = false;

    if (passed) printf("PASSED test_draw_hline\n");
    return passed;
}

int main() {
    bool success = true;

    if (!test_set_clip()) success = false;
    if (!test_draw_hline()) success = false;

    if (success) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}
