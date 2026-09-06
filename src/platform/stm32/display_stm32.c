#include "display_stm32.h"
#include "purrgo/display_hal.h"
#include <stddef.h>
#include <string.h>

/*
 * STM32 Display Stub Driver with Framebuffer
 *
 * This implementation provides the logical display interface required by the
 * common PurrGO code, as well as the initialization interface for the STM32 application.
 *
 * It maintains an in-memory framebuffer and manipulates pixels just like the PC emulator,
 * allowing UI rendering logic to execute completely.
 *
 * Hardware access (SPI, GPIO, DMA, E-Ink controller registers) is intentionally
 * absent because the physical display is not connected yet during the current
 * hardware bring-up stage.
 */

/* Size of the framebuffer in bytes: (Width * Height * BPP) / 8 */
#define DISPLAY_FB_SIZE ((DISPLAY_WIDTH * DISPLAY_HEIGHT * DISPLAY_BPP) / 8)

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

void display_set_pixel(int16_t x, int16_t y, uint8_t color) {
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

uint8_t display_get_pixel(int16_t x, int16_t y) {
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) {
        return 0; // COLOR_BLACK
    }

    int pixel_idx = y * DISPLAY_WIDTH + x;
    int byte_idx = pixel_idx / 4;
    int bit_shift = (3 - (pixel_idx % 4)) * 2;

    return (framebuffer[byte_idx] >> bit_shift) & 0x03;
}

const uint8_t* display_get_framebuffer(void) {
    return framebuffer;
}

/*
 * Implementation of common PurrGO display HAL.
 */
void display_refresh(void) {
    /* Stub: physical display is not connected yet. */
}

void display_refresh_region(int16_t x, int16_t y, int16_t w, int16_t h) {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    /* Stub: physical display is not connected yet. */
}
