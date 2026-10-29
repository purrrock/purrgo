#ifndef PURRGO_GFX_FB2BPP_H
#define PURRGO_GFX_FB2BPP_H

#include <stdint.h>
#include <string.h>

/*
 * Заливка горизонтального отрезка в 2-bpp framebuffer.
 *
 * Раскладка (та же, что у display_set_pixel() в прошивке): пиксели идут строка
 * за строкой, по четыре в байте, первый пиксель - в старших битах:
 *
 *     pixel_index = y * width + x
 *     byte        = fb[pixel_index >> 2]
 *     shift       = (3 - (pixel_index & 3)) * 2
 *
 * Заливаются пиксели x0..x1 (включительно, порядок любой) строки y, значение
 * color & 3. Координаты за пределами экрана отсекаются. Результат совпадает с
 * цепочкой display_set_pixel() по тем же пикселям, но целые байты в середине
 * отрезка заполняются одним memset().
 */
void gfx_fb2bpp_fill_span(
    uint8_t *fb,
    int16_t width,
    int16_t height,
    int16_t x0,
    int16_t x1,
    int16_t y,
    uint8_t color
);

/*
 * Единственная реализация раскладки 2-bpp framebuffer для прошивки и эмулятора
 * (display_set_pixel/display_get_pixel/display_clear). Внутри - static inline:
 * вызываются на каждый пиксель из draw_pixel-колбэка gfx.
 *
 * set/get: координаты вне экрана игнорируются (get возвращает 0 = чёрный).
 * clear: color & 3 во все пиксели.
 */
static inline void gfx_fb2bpp_set_pixel(uint8_t *fb, int16_t width, int16_t height,
                                        int16_t x, int16_t y, uint8_t color)
{
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return;
    }
    const uint32_t p = (uint32_t)y * (uint32_t)width + (uint32_t)x;
    const unsigned shift = (3u - (p & 3u)) << 1;
    fb[p >> 2] = (uint8_t)((fb[p >> 2] & ~(0x03u << shift)) | ((unsigned)(color & 3u) << shift));
}

static inline uint8_t gfx_fb2bpp_get_pixel(const uint8_t *fb, int16_t width, int16_t height,
                                           int16_t x, int16_t y)
{
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return 0;
    }
    const uint32_t p = (uint32_t)y * (uint32_t)width + (uint32_t)x;
    return (uint8_t)((fb[p >> 2] >> ((3u - (p & 3u)) << 1)) & 0x03u);
}

static inline void gfx_fb2bpp_clear(uint8_t *fb, int16_t width, int16_t height, uint8_t color)
{
    memset(fb, (int)((color & 3u) * 0x55u), ((size_t)width * (size_t)height + 3u) / 4u);
}

#endif /* PURRGO_GFX_FB2BPP_H */
