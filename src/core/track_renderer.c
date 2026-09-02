#include "purrgo/track_renderer.h"
#include "purrgo/fs_hal.h"
#include "purrgo/track_logger.h"
#include "map_projection.h"
#include "purrgo/gfx_line.h"
#include <string.h>

void purrgo_track_render(
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *vp,
    const char *gpx_filepath)
{
    if (!gfx || !camera || !vp || !gpx_filepath) return;

    static track_point_t track_points[TRACK_RAM_MAX_POINTS];
    size_t num_points = purrgo_logger_get_track_points(track_points, TRACK_RAM_MAX_POINTS);

    if (num_points == 0) {
        return;
    }

    // Oтрисовывать трек нужно тонкой черной линией.
    gfx_set_color(gfx, BLACK, gfx->color_bg);

    bool has_prev = false;
    int16_t prev_sx = 0, prev_sy = 0;

    for (size_t i = 0; i < num_points; i++) {
        int16_t sx, sy;
        project_to_screen(track_points[i].lon_1e7, track_points[i].lat_1e7, camera, vp, &sx, &sy);

        if (has_prev) {
            gfx_draw_line(gfx, prev_sx, prev_sy, sx, sy);
        }
        prev_sx = sx;
        prev_sy = sy;
        has_prev = true;
    }
}
