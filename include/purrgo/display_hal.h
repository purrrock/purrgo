#ifndef PURRGO_DISPLAY_HAL_H
#define PURRGO_DISPLAY_HAL_H

#include <stdint.h>

/*
 * Full screen refresh.
 */
void display_refresh(void);

/*
 * Partial screen refresh.
 *
 * @param x X coordinate of the top-left corner.
 * @param y Y coordinate of the top-left corner.
 * @param w Width of the region.
 * @param h Height of the region.
 */
void display_refresh_region(int16_t x, int16_t y, int16_t w, int16_t h);

#endif /* PURRGO_DISPLAY_HAL_H */
