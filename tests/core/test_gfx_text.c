#include "purrgo/gfx_text.h"
#include "purrgo/font5x7.h"
#include "purrgo/gfx_renderer.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Directly include the source file to access the static function for testing.
#include "../../src/core/gfx/gfx_text.c"

#define WIDTH 64
#define HEIGHT 64

static uint8_t framebuffer[WIDTH * HEIGHT];
static int pixels_drawn = 0;

static void draw_pixel(void *user_data, int16_t x, int16_t y, gfx_color_t color) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        if (framebuffer[y * WIDTH + x] != (uint8_t)color) {
            pixels_drawn++;
            framebuffer[y * WIDTH + x] = (uint8_t)color;
        }
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

static bool test_gfx_draw_char() {
    printf("Running test_gfx_draw_char...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);
    ctx.color_fg = 3; // White text
    ctx.color_bg = 1; // Dark Gray background

    // 'A' is character 65
    gfx_draw_char(&ctx, 10, 10, 'A');

    // Expected to draw an 6x8 block (48 pixels), including the inter-character spacing column
    if (pixels_drawn != 48) {
        printf("FAILED test_gfx_draw_char: Expected 48 pixels drawn (6x8 block), got %d\n", pixels_drawn);
        return false;
    }

    // Top-left pixel of background should be dark gray (1)
    if (framebuffer[10 * WIDTH + 10] != 1) {
        printf("FAILED test_gfx_draw_char: Background pixel mismatch at (10, 10)\n");
        return false;
    }

    // Dynamically calculate expected pixel based on font5x7
    // Find a pixel that should be foreground for 'A'
    int found_fg_col = -1;
    int found_fg_row = -1;
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            if ((font5x7['A'][col] >> row) & 1) {
                found_fg_col = col;
                found_fg_row = row;
                break;
            }
        }
        if (found_fg_col != -1) break;
    }

    if (found_fg_col != -1 && found_fg_row != -1) {
        if (framebuffer[(10 + found_fg_row) * WIDTH + (10 + found_fg_col)] != 3) {
            printf("FAILED test_gfx_draw_char: Foreground pixel mismatch at (%d, %d)\n", 10 + found_fg_col, 10 + found_fg_row);
            return false;
        }
    }

    // Inter-character space at x=15 should be background
    if (framebuffer[10 * WIDTH + 15] != 1) {
        printf("FAILED test_gfx_draw_char: Inter-character pixel mismatch at (15, 10)\n");
        return false;
    }

    printf("PASSED test_gfx_draw_char\n");
    return true;
}

static bool test_gfx_draw_string_halo() {
    printf("Running test_gfx_draw_string_halo...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    // Fill the background with a specific color to check for transparency
    memset(framebuffer, 2, sizeof(framebuffer)); // Fill with Light Gray (2)
    pixels_drawn = 0; // reset counter after memset

    // String "A"
    // 'A' has halo (WHITE) and inner text (BLACK)
    // Transparent drawing means it shouldn't overwrite the light gray background with anything else in the text's box,
    // other than the halo (WHITE, 3) and text itself (BLACK, 0).
    ctx.color_fg = 0;
    gfx_draw_string_halo(&ctx, 10, 10, "A");

    // Let's verify a pixel that is far outside the string, should still be 2.
    if (framebuffer[0 * WIDTH + 0] != 2) {
        printf("FAILED test_gfx_draw_string_halo: Unrelated pixel modified\n");
        return false;
    }

    // Dynamically calculate expected pixel based on font5x7
    // Find a pixel that should be text for 'A'
    int found_text_col = -1;
    int found_text_row = -1;
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            if ((font5x7['A'][col] >> row) & 1) {
                found_text_col = col;
                found_text_row = row;
                break;
            }
        }
        if (found_text_col != -1) break;
    }

    if (found_text_col != -1 && found_text_row != -1) {
        // Text pixel should be BLACK (0)
        if (framebuffer[(10 + found_text_row) * WIDTH + (10 + found_text_col)] != 0) {
            printf("FAILED test_gfx_draw_string_halo: Text pixel mismatch at (%d, %d), expected 0\n", 10 + found_text_col, 10 + found_text_row);
            return false;
        }

        // Halo pixel (one to the left of the text pixel) should be WHITE (3)
        if (framebuffer[(10 + found_text_row) * WIDTH + (10 + found_text_col - 1)] != 3) {
            printf("FAILED test_gfx_draw_string_halo: Halo pixel mismatch at (%d, %d), expected 3\n", 10 + found_text_col - 1, 10 + found_text_row);
            return false;
        }
    }

    // A pixel far outside the character should remain untouched (2)
    if (framebuffer[10 * WIDTH + 20] != 2) {
        printf("FAILED test_gfx_draw_string_halo: Untouched pixel changed at (20, 10)\n");
        return false;
    }

    printf("PASSED test_gfx_draw_string_halo\n");
    return true;
}

static bool test_gfx_draw_char_transparent_direct() {
    printf("Running test_gfx_draw_char_transparent_direct...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    // Fill the frame buffer with a specific color (2)
    memset(framebuffer, 2, sizeof(framebuffer));

    // 'A' is character 65
    // We expect it to draw only the pixels defined in font5x7['A']
    ctx.color_fg = 3; // White text
    gfx_draw_char_transparent(&ctx, 10, 10, 'A');

    // Dynamically calculate expected pixels based on font5x7
    int pixels_drawn = 0;
    const unsigned char* bitmap = font5x7['A'];

    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            bool should_be_drawn = (bitmap[col] >> row) & 1;
            uint8_t expected_color = should_be_drawn ? 3 : 2;
            uint8_t actual_color = framebuffer[(10 + row) * WIDTH + (10 + col)];

            if (actual_color != expected_color) {
                printf("FAILED test_gfx_draw_char_transparent_direct: Pixel at (%d, %d) mismatch. Expected %d, got %d\n",
                       10 + col, 10 + row, expected_color, actual_color);
                return false;
            }
            if (should_be_drawn) {
                pixels_drawn++;
            }
        }
    }

    // Check an outside pixel
    if (framebuffer[0 * WIDTH + 0] != 2) {
        printf("FAILED test_gfx_draw_char_transparent_direct: Outside pixel modified\n");
        return false;
    }

    printf("PASSED test_gfx_draw_char_transparent_direct\n");
    return true;
}

int main() {
    bool success = true;

    if (!test_gfx_draw_char()) success = false;
    if (!test_gfx_draw_string_halo()) success = false;
    if (!test_gfx_draw_char_transparent_direct()) success = false;

    if (success) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}
