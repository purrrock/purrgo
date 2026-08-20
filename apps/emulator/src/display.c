#include "display.h"
#include <string.h>

static uint8_t framebuffer[DISPLAY_FB_SIZE];

void display_init(void) {
    display_clear(COLOR_WHITE); // Typical e-ink default
}

void display_clear(uint8_t color) {
    color &= 0x03;
    uint8_t byte_val = (color << 6) | (color << 4) | (color << 2) | color;
    for (int i = 0; i < DISPLAY_FB_SIZE; ++i) {
        framebuffer[i] = byte_val;
    }
}

void display_set_pixel(int x, int y, uint8_t color) {
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) {
        return;
    }

    color &= 0x03;
    int pixel_idx = y * DISPLAY_WIDTH + x;
    int byte_idx = pixel_idx / 4;
    int bit_shift = (3 - (pixel_idx % 4)) * 2; // MSB first for pixels

    framebuffer[byte_idx] &= ~(0x03 << bit_shift); // Clear existing
    framebuffer[byte_idx] |= (color << bit_shift); // Set new
}

const uint8_t* display_get_framebuffer(void) {
    return framebuffer;
}
