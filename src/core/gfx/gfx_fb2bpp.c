#include "purrgo/gfx_fb2bpp.h"

#include <stddef.h>
#include <string.h>

static inline void set_pixel_2bpp(uint8_t *fb, uint32_t pixel_index, uint8_t color)
{
    uint32_t byte_index = pixel_index >> 2;
    unsigned shift = (3u - (pixel_index & 3u)) << 1;

    fb[byte_index] = (uint8_t)((fb[byte_index] & ~(0x03u << shift)) | ((unsigned)color << shift));
}

void gfx_fb2bpp_fill_span(
    uint8_t *fb,
    int16_t width,
    int16_t height,
    int16_t x0,
    int16_t x1,
    int16_t y,
    uint8_t color)
{
    if (fb == NULL || y < 0 || y >= height) {
        return;
    }

    if (x0 > x1) {
        int16_t t = x0;
        x0 = x1;
        x1 = t;
    }
    if (x0 < 0) {
        x0 = 0;
    }
    if (x1 >= width) {
        x1 = (int16_t)(width - 1);
    }
    if (x0 > x1) {
        return;
    }

    color &= 0x03;

    uint32_t p = (uint32_t)y * (uint32_t)width + (uint32_t)x0;
    uint32_t end = (uint32_t)y * (uint32_t)width + (uint32_t)x1 + 1u;   /* не включая */

    /* Начало: пиксели до границы байта. */
    while (p < end && (p & 3u) != 0) {
        set_pixel_2bpp(fb, p, color);
        p++;
    }

    /* Середина: целые байты (четыре пикселя одного цвета: color * 0x55). */
    uint32_t full_bytes = (end - p) >> 2;
    if (full_bytes != 0) {
        memset(&fb[p >> 2], (int)(color * 0x55u), full_bytes);
        p += full_bytes << 2;
    }

    /* Конец: оставшиеся пиксели. */
    while (p < end) {
        set_pixel_2bpp(fb, p, color);
        p++;
    }
}
