#include "display.h"
#include <string.h>

// Included relative to apps/emulator/
#include "../font8x8_basic.h"

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

void display_draw_char(int x, int y, char c, uint8_t color, uint8_t bg_color) {
    if (c < 0 || c >= 128) {
        c = '?';
    }

    const char* bitmap = font8x8_basic[(int)c];
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if ((bitmap[row] >> col) & 1) {
                display_set_pixel(x + col, y + row, color);
            } else {
                display_set_pixel(x + col, y + row, bg_color);
            }
        }
    }
}

void display_draw_string(int x, int y, const char* str, uint8_t color, uint8_t bg_color) {
    int cur_x = x;
    int cur_y = y;
    while (*str) {
        if (*str == '\n') {
            cur_x = x;
            cur_y += 8;
        } else {
            display_draw_char(cur_x, cur_y, *str, color, bg_color);
            cur_x += 8;
            if (cur_x + 8 > DISPLAY_WIDTH) {
                cur_x = 0;
                cur_y += 8;
            }
        }
        str++;
    }
}

const uint8_t* display_get_framebuffer(void) {
    return framebuffer;
}
