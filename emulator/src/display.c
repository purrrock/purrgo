#include "display.h"
#include "purrgo/gfx_fb2bpp.h"

static uint8_t framebuffer[DISPLAY_FB_SIZE];

void display_init(void) {
    display_clear(COLOR_WHITE); // Typical e-ink default
}

void display_clear(uint8_t color) {
    gfx_fb2bpp_clear(framebuffer, DISPLAY_WIDTH, DISPLAY_HEIGHT, color);
}

void display_set_pixel(int16_t x, int16_t y, uint8_t color) {
    gfx_fb2bpp_set_pixel(framebuffer, DISPLAY_WIDTH, DISPLAY_HEIGHT, x, y, color);
}

const uint8_t* display_get_framebuffer(void) {
    return framebuffer;
}

uint8_t display_get_pixel(int16_t x, int16_t y) {
    return gfx_fb2bpp_get_pixel(framebuffer, DISPLAY_WIDTH, DISPLAY_HEIGHT, x, y);
}
