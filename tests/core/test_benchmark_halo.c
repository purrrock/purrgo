#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

#include "purrgo/gfx_text.h"
#include "purrgo/gfx_renderer.h"

#define WIDTH 128
#define HEIGHT 128

static uint8_t framebuffer[WIDTH * HEIGHT / 8]; // Dummy buffer

static void draw_pixel(void *userdata, int16_t x, int16_t y, gfx_color_t color) {
    // Just a dummy to simulate drawing, we don't even write to memory to focus on CPU overhead of the function calls
}

static gfx_color_t read_pixel(void *userdata, int16_t x, int16_t y) {
    return 0;
}

int main() {
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);

    const char* text = "Hello World! 1234567890\nTest Line 2 with more text!";

    clock_t start = clock();
    for (int i = 0; i < 10000; i++) {
        gfx_draw_string_halo(&ctx, 10, 10, text);
    }
    clock_t end = clock();

    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Halo benchmark took %f seconds\n", cpu_time_used);

    return 0;
}
