#ifndef PURRGO_GFX_FB2BPP_H
#define PURRGO_GFX_FB2BPP_H

#include <stdint.h>

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

#endif /* PURRGO_GFX_FB2BPP_H */
