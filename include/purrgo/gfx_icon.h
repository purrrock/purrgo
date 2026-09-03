#ifndef GFX_ICON_H
#define GFX_ICON_H

#include "purrgo/gfx_renderer.h"
#include <stdint.h>

/**
 * @brief Отрисовка 7x7 иконки с прозрачностью.
 *
 * @param ctx Контекст графического ядра.
 * @param x Координата X центра иконки.
 * @param y Координата Y центра иконки.
 * @param icon 7x7 массив пикселей иконки, где бит 2 - прозрачность, биты 1:0 - цвет.
 */
void gfx_draw_icon_7x7(gfx_context_t *ctx, int16_t x, int16_t y, const uint8_t icon[7][7]);

#endif /* GFX_ICON_H */
