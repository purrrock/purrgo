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
 * @brief Renders only the newest track segment between the two latest recorded points.
 *
 * This function is intended for incremental rendering of the active track to
 * avoid a full redraw of all track points. It retrieves the last two points
 * from the track logger and draws a single segment connecting them.
 *
 * @param gfx Graphics context to draw the track segment on.
 * @param camera Current geographic bounding box of the camera.
 * @param vp Current screen viewport.
 */
void purrgo_track_render_last_segment(
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *vp
);

#endif // PURRGO_TRACK_RENDERER_H
