#ifndef PURRGO_TRACK_RENDERER_H
#define PURRGO_TRACK_RENDERER_H

#include "purrgo/gfx_renderer.h"
#include "purrgo/map.h"

/**
 * @brief Renders the current track from a GPX file onto the vector map.
 *
 * Implements a streaming parser that doesn't expect closing XML tags
 * (since the track might be actively recorded). Reuses previous
 * camera/viewport state to only render newly added points between calls,
 * significantly reducing IO and CPU usage.
 *
 * @param gfx Graphics context to draw the track on.
 * @param camera Current geographic bounding box of the camera.
 * @param vp Current screen viewport.
 * @param gpx_filepath Path to the GPX file.
 */
void purrgo_track_render(
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *vp,
    const char *gpx_filepath
);

/**
 * @brief Renders only the most recently added segment of the track.
 *
 * Uses the RAM track buffer to draw a single line between the last two
 * recorded track points. Returns the bounding box of the modified area.
 *
 * @param gfx Graphics context to draw the track on.
 * @param camera Current geographic bounding box of the camera.
 * @param vp Current screen viewport.
 * @param min_x Returns the minimum X coordinate of the modified area.
 * @param min_y Returns the minimum Y coordinate of the modified area.
 * @param max_x Returns the maximum X coordinate of the modified area.
 * @param max_y Returns the maximum Y coordinate of the modified area.
 * @return true if a segment was drawn, false if not enough points are available.
 */
bool purrgo_track_render_last_segment(
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *vp,
    int16_t *min_x,
    int16_t *min_y,
    int16_t *max_x,
    int16_t *max_y
);

#endif // PURRGO_TRACK_RENDERER_H
