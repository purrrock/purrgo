#include "purrgo/gfx_icon.h"
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
        framebuffer[y * WIDTH + x] = (uint8_t)color;
        pixels_drawn++;
    }
}

static gfx_color_t read_pixel(void *user_data, int16_t x, int16_t y) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        return framebuffer[y * WIDTH + x];
    }
    return 0;
}

static void reset_test_state(gfx_context_t *ctx) {
    memset(framebuffer, 0, sizeof(framebuffer));
    pixels_drawn = 0;
    ctx->color_bg = 0;
    ctx->color_fg = 1;
}

static bool test_null_pointers() {
    printf("Running test_null_pointers...\n");
    uint8_t icon[11][11] = {{0}};

    // Should not crash
    gfx_draw_icon_11x11(NULL, 10, 10, icon);

    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);

    gfx_draw_icon_11x11(&ctx, 10, 10, NULL);

    printf("PASSED test_null_pointers\n");
    return true;
}

static bool test_draw_icon_transparency() {
    printf("Running test_draw_icon_transparency...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_test_state(&ctx);

    // Create a mock icon where center pixel is drawn and colored WHITE (3)
    // and one corner pixel is transparent.
    // Bit 2 (0x04) = drawn. Bits 1:0 (0x03) = color.
    uint8_t icon[11][11] = {{0}};
    icon[5][5] = 0x04 | 0x03; // Center pixel: drawn, white
    icon[0][0] = 0x00;        // Corner pixel: transparent
    icon[10][10] = 0x04 | 0x01; // Bottom right pixel: drawn, dark gray (1)

    // Draw center at (20, 20).
    // The top-left corner should be at 20-5=15, 20-5=15
    gfx_draw_icon_11x11(&ctx, 20, 20, icon);

    if (pixels_drawn != 2) {
        printf("FAILED test_draw_icon_transparency: Expected 2 pixels drawn, got %d\n", pixels_drawn);
        return false;
    }

    if (framebuffer[20 * WIDTH + 20] != 3) {
        printf("FAILED test_draw_icon_transparency: Center pixel color mismatch\n");
        return false;
    }

    if (framebuffer[15 * WIDTH + 15] != 0) {
         printf("FAILED test_draw_icon_transparency: Transparent pixel was drawn\n");
         return false;
    }

    if (framebuffer[25 * WIDTH + 25] != 1) {
         printf("FAILED test_draw_icon_transparency: Bottom right pixel color mismatch\n");
         return false;
    }

    printf("PASSED test_draw_icon_transparency\n");
    return true;
}

static bool test_draw_icon_clipping() {
    printf("Running test_draw_icon_clipping...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_test_state(&ctx);
    gfx_set_clip(&ctx, 10, 10, 20, 20); // clip region [10,29] x [10,29]

    uint8_t icon[11][11] = {{0}};
    // Fill all with drawn white pixels
    for(int i=0; i<11; i++) {
        for(int j=0; j<11; j++) {
            icon[i][j] = 0x04 | 0x03;
        }
    }

    // Draw center at (10, 10) -> top-left will be at (5, 5) which is out of clip region
    // The clip is from x=10..29, y=10..29
    // Image is from x=5..15, y=5..15.
    // Intersect: x=10..15 (6 pixels), y=10..15 (6 pixels) = 36 pixels.
    gfx_draw_icon_11x11(&ctx, 10, 10, icon);

    if (pixels_drawn != 36) {
        printf("FAILED test_draw_icon_clipping: Expected 36 pixels drawn, got %d\n", pixels_drawn);
        return false;
    }

    if (framebuffer[9 * WIDTH + 9] != 0) {
        printf("FAILED test_draw_icon_clipping: Pixel outside clip region modified\n");
        return false;
    }

    if (framebuffer[15 * WIDTH + 15] != 3) {
        printf("FAILED test_draw_icon_clipping: Pixel inside clip region not drawn correctly\n");
        return false;
    }

    printf("PASSED test_draw_icon_clipping\n");
    return true;
}

int main() {
    bool success = true;

    if (!test_null_pointers()) success = false;
    if (!test_draw_icon_transparency()) success = false;
    if (!test_draw_icon_clipping()) success = false;

    if (success) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}
