#include "display_stm32.h"
#include "purrgo/display_hal.h"
#include "purrgo/logger.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "display_st7789.h"

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
    // Use memset for efficient framebuffer initialization
    memset(framebuffer, byte_val, DISPLAY_FB_SIZE);
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
 * Implementation of GFX-to-display pixel callbacks.
 */

void stm32_draw_pixel_cb(
    void *fb,
    int16_t x,
    int16_t y,
    gfx_color_t color
)
{
    /*
     * Текущий display_stm32 предоставляет собственный framebuffer
     * и API display_set_pixel(). Поэтому fb здесь непосредственно
     * не используется.
     */
    (void)fb;

    display_set_pixel(x, y, color);
}

gfx_color_t stm32_read_pixel_cb(
    void *fb,
    int16_t x,
    int16_t y
)
{
    /*
     * Аналогично callback записи, framebuffer принадлежит
     * display_stm32 и доступен через display_get_pixel().
     */
    (void)fb;

    return display_get_pixel(x, y);
}

/*
 * Implementation of common PurrGO display HAL.
 */
static int partial_refresh_count = 0;

static int16_t pending_x1 = -1;
static int16_t pending_y1 = -1;
static int16_t pending_x2 = -1;
static int16_t pending_y2 = -1;
static int pending_has_region = 0;

static void do_refresh_region(int16_t x, int16_t y, int16_t w, int16_t h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISPLAY_WIDTH) { w = DISPLAY_WIDTH - x; }
    if (y + h > DISPLAY_HEIGHT) { h = DISPLAY_HEIGHT - y; }

    if (w <= 0 || h <= 0) {
        return;
    }

    ST7789_SetWindow(x, y, x + w - 1, y + h - 1);

    ST7789_StartPixels();

    #define TRANSFER_BUF_PIXELS 128
    uint8_t buf[TRANSFER_BUF_PIXELS * 2];
    int buf_idx = 0;

    for (int row = y; row < y + h; row++) {
        for (int col = x; col < x + w; col++) {
            uint32_t pixel_idx = row * DISPLAY_WIDTH + col;
            int byte_idx = pixel_idx / 4;
            int bit_shift = (3 - (pixel_idx % 4)) * 2;
            uint8_t color2bpp = (framebuffer[byte_idx] >> bit_shift) & 0x03;

            uint16_t rgb565 = ST7789_COLOR_BLACK;
            switch (color2bpp) {
                case COLOR_BLACK: rgb565 = ST7789_COLOR_BLACK; break;
                case COLOR_DARK_GRAY: rgb565 = 0x52AA; break;
                case COLOR_LIGHT_GRAY: rgb565 = 0xAD55; break; /* Appropriate light-gray RGB565 */
                case COLOR_WHITE: rgb565 = ST7789_COLOR_WHITE; break;
            }

            buf[buf_idx * 2] = rgb565 >> 8;
            buf[buf_idx * 2 + 1] = rgb565 & 0xFF;
            buf_idx++;

            if (buf_idx >= TRANSFER_BUF_PIXELS) {
                ST7789_WritePixels(buf, buf_idx * 2);
                buf_idx = 0;
            }
        }
    }

    if (buf_idx > 0) {
        ST7789_WritePixels(buf, buf_idx * 2);
    }

    ST7789_EndPixels();
}

void display_refresh(void) {
//    PURRGO_LOG("FULL REFRESH\r\n");
    partial_refresh_count = 0;
    pending_has_region = 0;
    do_refresh_region(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

void display_refresh_region(int16_t x, int16_t y, int16_t w, int16_t h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISPLAY_WIDTH) { w = DISPLAY_WIDTH - x; }
    if (y + h > DISPLAY_HEIGHT) { h = DISPLAY_HEIGHT - y; }

    if (w <= 0 || h <= 0) {
        return;
    }

    if (!pending_has_region) {
        pending_x1 = x;
        pending_y1 = y;
        pending_x2 = x + w - 1;
        pending_y2 = y + h - 1;
        pending_has_region = 1;
    } else {
        if (x < pending_x1) pending_x1 = x;
        if (y < pending_y1) pending_y1 = y;
        if (x + w - 1 > pending_x2) pending_x2 = x + w - 1;
        if (y + h - 1 > pending_y2) pending_y2 = y + h - 1;
    }
}

void display_flush(void) {
    if (!pending_has_region) {
        return;
    }

    int16_t w = pending_x2 - pending_x1 + 1;
    int16_t h = pending_y2 - pending_y1 + 1;

    if (partial_refresh_count >= MAX_PARTIAL_REFRESHES) {
        display_refresh();
    } else {
        partial_refresh_count++;
        do_refresh_region(pending_x1, pending_y1, w, h);
        pending_has_region = 0;
    }
}
