#include "display_stm32.h"
#include "purrgo/display_hal.h"
#include <stddef.h>

/*
 * STM32 Display Stub Driver
 *
 * This implementation provides the logical display interface required by the
 * common PurrGO code, as well as the initialization interface for the STM32 application.
 *
 * Hardware access (SPI, GPIO, DMA, E-Ink controller registers) is intentionally
 * absent because the physical display is not connected yet during the current
 * hardware bring-up stage.
 *
 * This stub does not allocate a large framebuffer to conserve memory.
 * A real physical display driver will replace this implementation later.
 */

void display_init(void) {
    /*
     * Stub: physical display is not connected yet.
     * SPI and GPIO are not initialized here.
     */
}

void display_clear(uint8_t color) {
    (void)color;
    /* Stub: no physical display I/O. */
}

void display_set_pixel(int16_t x, int16_t y, uint8_t color) {
    (void)x;
    (void)y;
    (void)color;
    /* Stub: no framebuffer or physical display access. */
}

uint8_t display_get_pixel(int16_t x, int16_t y) {
    (void)x;
    (void)y;
    /* Stub: no framebuffer. Return a safe, documented value. */
    return COLOR_BLACK;
}

const uint8_t* display_get_framebuffer(void) {
    /*
     * Stub: to avoid unnecessary large memory allocations,
     * the framebuffer is not allocated in this stub.
     */
    return NULL;
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
