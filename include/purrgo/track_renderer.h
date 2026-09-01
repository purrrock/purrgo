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

#endif // PURRGO_TRACK_RENDERER_H
