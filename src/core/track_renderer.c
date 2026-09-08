#include "purrgo/track_renderer.h"
#include "purrgo/track_logger.h"
#include "map_projection.h"
#include "purrgo/gfx_line.h"

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

bool purrgo_track_render_last_segment(
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *vp,
    purrgo_rect_t *out_rect)
{
    if (!gfx || !camera || !vp || !out_rect) return false;

    track_point_t prev_point, last_point;
    if (!purrgo_logger_get_last_two_points(&prev_point, &last_point)) {
        return false;
    }

    int16_t prev_sx, prev_sy, sx, sy;
    project_to_screen(prev_point.lon_1e7, prev_point.lat_1e7, camera, vp, &prev_sx, &prev_sy);
    project_to_screen(last_point.lon_1e7, last_point.lat_1e7, camera, vp, &sx, &sy);

    int16_t min_x = (prev_sx < sx) ? prev_sx : sx;
    int16_t max_x = (prev_sx > sx) ? prev_sx : sx;
    int16_t min_y = (prev_sy < sy) ? prev_sy : sy;
    int16_t max_y = (prev_sy > sy) ? prev_sy : sy;

    // Add strict 1-pixel margin
    min_x -= 1;
    max_x += 1;
    min_y -= 1;
    max_y += 1;

    // Early out if strictly outside viewport
    if (max_x < vp->offset_x || min_x >= vp->offset_x + vp->width ||
        max_y < vp->offset_y || min_y >= vp->offset_y + vp->height) {
        return false;
    }

    // Отрисовывать трек нужно тонкой черной линией.
    gfx_set_color(gfx, BLACK, gfx->color_bg);
    gfx_draw_line(gfx, prev_sx, prev_sy, sx, sy);

    // Clamp to viewport
    if (min_x < vp->offset_x) min_x = vp->offset_x;
    if (max_x >= vp->offset_x + vp->width) max_x = vp->offset_x + vp->width - 1;
    if (min_y < vp->offset_y) min_y = vp->offset_y;
    if (max_y >= vp->offset_y + vp->height) max_y = vp->offset_y + vp->height - 1;

    int16_t clip_w = max_x - min_x + 1;
    int16_t clip_h = max_y - min_y + 1;

    if (clip_w > 0 && clip_h > 0) {
        out_rect->x = min_x;
        out_rect->y = min_y;
        out_rect->w = clip_w;
        out_rect->h = clip_h;
        return true;
    }

    return false;
}
