#ifndef PURRGO_DISPLAY_HAL_H
#define PURRGO_DISPLAY_HAL_H

#include <stdint.h>

#define MAX_PARTIAL_REFRESHES 5

/*
 * Full screen refresh.
 */
void display_refresh(void);

/*
 * Accumulate a partial screen refresh region.
 *
 * Note: This function implements a deferred contract. It does NOT immediately
 * refresh the physical display. Instead, it accumulates the requested region
 * into a bounding box.
 * You MUST call display_flush() later to actually perform the physical refresh.
 * Multiple calls to this function before a display_flush() will be merged
 * into a single physical update.
 *
 * @param x X coordinate of the top-left corner.
 * @param y Y coordinate of the top-left corner.
 * @param w Width of the region.
 * @param h Height of the region.
 */
void display_refresh_region(int16_t x, int16_t y, int16_t w, int16_t h);

/*
 * Flushes accumulated partial refresh regions to the physical display.
 *
 * This function completes the deferred contract initiated by
 * display_refresh_region(). It executes a single physical E-Ink update
 * using the bounding box of all accumulated regions, and increments the
 * partial refresh budget counter exactly once.
 *
 * Should be called once at the end of a logical UI update
 * (e.g., at the end of purrgo_app_ui_render).
 */
void display_flush(void);

#endif /* PURRGO_DISPLAY_HAL_H */
