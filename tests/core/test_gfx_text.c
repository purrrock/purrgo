#include "purrgo/gfx_text.h"
#include "purrgo/gfx_renderer.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define WIDTH 64
#define HEIGHT 64

static uint8_t framebuffer[WIDTH * HEIGHT];
static int pixels_drawn = 0;
static int last_pixel_x = -1;
static int last_pixel_y = -1;

static void draw_pixel(void *user_data, int16_t x, int16_t y, gfx_color_t color) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        framebuffer[y * WIDTH + x] = (uint8_t)color;
        pixels_drawn++;
        last_pixel_x = x;
        last_pixel_y = y;
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
    last_pixel_x = -1;
    last_pixel_y = -1;
    ctx->color_bg = 0; // Black
    ctx->color_fg = 1; // White
}

static bool test_null_pointers() {
    printf("Running test_null_pointers...\n");
    // Should not crash
    gfx_draw_string(NULL, 10, 10, "Hello");

    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);

    gfx_draw_string(&ctx, 10, 10, NULL);

    // Test nulls in gfx_draw_string_halo
    gfx_draw_string_halo(NULL, 10, 10, "Hello");
    gfx_draw_string_halo(&ctx, 10, 10, NULL);

    // Test nulls in gfx_draw_char
    gfx_draw_char(NULL, 10, 10, 'A');

    // Test with missing draw_pixel callback
    gfx_context_t no_draw_ctx;
    gfx_init(&no_draw_ctx, WIDTH, HEIGHT, framebuffer, NULL, read_pixel);
    gfx_draw_string(&no_draw_ctx, 10, 10, "Hello");
    gfx_draw_string_halo(&no_draw_ctx, 10, 10, "Hello");
    gfx_draw_char(&no_draw_ctx, 10, 10, 'A');

    printf("PASSED test_null_pointers\n");
    return true;
}

static bool test_empty_string() {
    printf("Running test_empty_string...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_test_state(&ctx);

    gfx_draw_string(&ctx, 10, 10, "");

    if (pixels_drawn != 0) {
        printf("FAILED test_empty_string: Drew %d pixels\n", pixels_drawn);
        return false;
    }

    printf("PASSED test_empty_string\n");
    return true;
}

static bool test_newline_handling() {
    printf("Running test_newline_handling...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_test_state(&ctx);

    gfx_draw_string(&ctx, 10, 10, "A\nB");

    // We expect 'A' at 10,10, and 'B' at 10,18
    // font is 5x7 + 1 pixel inter-letter spacing = 6x8 cell.

    // We expect pixels drawn for two characters
    // 'A' and 'B' + background fills
    // Background fills draw a 6x8 box per char (48 pixels each).
    if (pixels_drawn != 96) {
        printf("FAILED test_newline_handling: Expected 96 pixels, got %d\n", pixels_drawn);
        return false;
    }

    // Test a simple pixel in B (say the top left of B, bounding box should start at x=10, y=18)
    // Actually the whole 6x8 block is drawn. We just check pixel count and ensure no crashes,
    // and that 'last_pixel_y' reached at least 18 + 7 = 25 (the bottom row of 'B').
    if (last_pixel_y < 18 || last_pixel_y > 25) {
         printf("FAILED test_newline_handling: last_pixel_y %d is out of expected bounds\n", last_pixel_y);
         return false;
    }

    printf("PASSED test_newline_handling\n");
    return true;
}

static bool test_string_halo() {
    printf("Running test_string_halo...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_test_state(&ctx);

    gfx_draw_string_halo(&ctx, 10, 10, "Hi");

    if (pixels_drawn == 0) {
        printf("FAILED test_string_halo: No pixels drawn\n");
        return false;
    }

    // Halo draws quite a few pixels (3x3 brush per font pixel), plus text.
    // Just ensuring we drew > 0 pixels and didn't crash.

    // Test newline in halo
    reset_test_state(&ctx);
    gfx_draw_string_halo(&ctx, 10, 10, "Hi\nThere");

    if (pixels_drawn == 0) {
        printf("FAILED test_string_halo: No pixels drawn for newline\n");
        return false;
    }

    // We check that it went down to the second line. 'last_pixel_y' reached at least 18 + 7 = 25
    if (last_pixel_y < 18 || last_pixel_y > 35) {
         printf("FAILED test_string_halo: last_pixel_y %d is out of expected bounds for newline\n", last_pixel_y);
         return false;
    }

    printf("PASSED test_string_halo\n");
    return true;
}

static bool test_single_char() {
    printf("Running test_single_char...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_test_state(&ctx);

    gfx_draw_char(&ctx, 10, 10, 'A');

    if (pixels_drawn != 48) {
         printf("FAILED test_single_char: Expected 48 pixels (6x8 cell), got %d\n", pixels_drawn);
         return false;
    }

    printf("PASSED test_single_char\n");
    return true;
}

static bool test_out_of_bounds() {
    printf("Running test_out_of_bounds...\n");
static bool test_single_char_string() {
    printf("Running test_single_char_string...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_test_state(&ctx);

    // Completely outside the frame buffer
    gfx_draw_string(&ctx, -100, -100, "Invisible");

    if (pixels_drawn != 0) {
        printf("FAILED test_out_of_bounds: Expected 0 pixels, got %d\n", pixels_drawn);
        return false;
    }

    // Partially outside
    reset_test_state(&ctx);
    gfx_draw_string(&ctx, WIDTH - 3, HEIGHT - 3, "Part");

    if (pixels_drawn == 0 || pixels_drawn >= 4 * 48) {
        printf("FAILED test_out_of_bounds: Expected partial draw, got %d pixels\n", pixels_drawn);
        return false;
    }

    printf("PASSED test_out_of_bounds\n");
    return true;
}

static bool test_extended_ascii() {
    printf("Running test_extended_ascii...\n");
    gfx_draw_string(&ctx, 10, 10, "A");

    if (pixels_drawn != 48) {
         printf("FAILED test_single_char_string: Expected 48 pixels (6x8 cell), got %d\n", pixels_drawn);
         return false;
    }

    printf("PASSED test_single_char_string\n");
    return true;
}

static bool test_multiple_newlines() {
    printf("Running test_multiple_newlines...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_test_state(&ctx);

    // Draw chars >= 128
    gfx_draw_string(&ctx, 10, 10, "\x80\xFF");

    if (pixels_drawn != 96) {
        printf("FAILED test_extended_ascii: Expected 96 pixels, got %d\n", pixels_drawn);
        return false;
    }

    printf("PASSED test_extended_ascii\n");
    return true;
}

static bool test_multiple_newlines() {
    printf("Running test_multiple_newlines...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_test_state(&ctx);

    // A at 10,10, \n\n moves to 10,26, B at 10,26
    gfx_draw_string(&ctx, 10, 10, "A\n\nB");

    if (pixels_drawn != 96) {
        printf("FAILED test_multiple_newlines: Expected 96 pixels, got %d\n", pixels_drawn);
        return false;
    }

    if (last_pixel_y < 26 || last_pixel_y > 33) {
         printf("FAILED test_multiple_newlines: last_pixel_y %d is out of expected bounds [26, 33]\n", last_pixel_y);
         return false;
    }

    printf("PASSED test_multiple_newlines\n");
    gfx_draw_string(&ctx, 10, 10, "\n\n\n");

    if (pixels_drawn != 0) {
        printf("FAILED test_multiple_newlines: Expected 0 pixels, got %d\n", pixels_drawn);
        return false;
    }

    // Now test with characters between newlines
    reset_test_state(&ctx);
    gfx_draw_string(&ctx, 10, 10, "A\n\nB");

    // A at 10,10. B should be at 10, 10 + 8*2 = 26
    if (pixels_drawn != 96) {
        printf("FAILED test_multiple_newlines: Expected 96 pixels, got %d\n", pixels_drawn);
        return false;
    }

    // B is drawn at y=26 to 33. Check last pixel bounds.
    if (last_pixel_y < 26 || last_pixel_y > 33) {
         printf("FAILED test_multiple_newlines: last_pixel_y %d is out of expected bounds\n", last_pixel_y);
         return false;
    }

    printf("PASSED test_multiple_newlines\n");
    return true;
}

static bool test_out_of_bounds_text() {
    printf("Running test_out_of_bounds_text...\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);

    // Negative coordinates
    reset_test_state(&ctx);
    gfx_draw_string(&ctx, -10, -10, "A");

    // Some pixels may be drawn in the buffer if they overlap, but clipping prevents drawing outside 0..WIDTH-1, 0..HEIGHT-1.
    // At x=-10, y=-10, a 6x8 char ends at x=-5, y=-3. It should draw 0 pixels.
    if (pixels_drawn != 0) {
        printf("FAILED test_out_of_bounds_text: Expected 0 pixels, got %d\n", pixels_drawn);
        return false;
    }

    // Coordinates past the right edge
    reset_test_state(&ctx);
    gfx_draw_string(&ctx, WIDTH + 10, 10, "A");
    if (pixels_drawn != 0) {
        printf("FAILED test_out_of_bounds_text: Expected 0 pixels, got %d\n", pixels_drawn);
        return false;
    }

    // Coordinates past the bottom edge
    reset_test_state(&ctx);
    gfx_draw_string(&ctx, 10, HEIGHT + 10, "A");
    if (pixels_drawn != 0) {
        printf("FAILED test_out_of_bounds_text: Expected 0 pixels, got %d\n", pixels_drawn);
        return false;
    }

    // Partially out of bounds
    reset_test_state(&ctx);
    gfx_draw_string(&ctx, WIDTH - 3, 10, "A"); // 3 pixels of the 6-pixel char width should draw
    // The character writes pixels inside the framebuffer bounds.
    // Instead of exactly guessing the number, just ensure it doesn't crash and draws something.
    if (pixels_drawn == 0 || pixels_drawn > 48) {
        printf("FAILED test_out_of_bounds_text (partial): Drew %d pixels\n", pixels_drawn);
        return false;
    }

    printf("PASSED test_out_of_bounds_text\n");
    return true;
}

int main() {
    bool success = true;

    if (!test_null_pointers()) success = false;
    if (!test_empty_string()) success = false;
    if (!test_newline_handling()) success = false;
    if (!test_string_halo()) success = false;
    if (!test_single_char()) success = false;
    if (!test_out_of_bounds()) success = false;
    if (!test_extended_ascii()) success = false;
    if (!test_multiple_newlines()) success = false;
    if (!test_single_char_string()) success = false;
    if (!test_multiple_newlines()) success = false;
    if (!test_out_of_bounds_text()) success = false;

    if (success) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}
