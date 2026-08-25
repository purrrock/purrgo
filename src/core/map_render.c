#include "map_render.h"
#include "map_mlp.h"
#include "map_projection.h"
#include "purrgo/logger.h"
#include "purrgo/gfx_line.h"
#include "purrgo/gfx_polygon.h"
#include <stddef.h>

static gfx_point_t s_polygon_buffer[PURRGO_MAP_MAX_POINTS];

void map_render_feature(
    purrgo_fs_t *mlp_fs,
    uint32_t v1_offset,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    gfx_context_t *gfx,
    bool is_polygon_layer,
    purrgo_map_style_t style,
    map_diag_t *diag
) {
    if (mlp_fs == NULL || cam == NULL || vp == NULL || gfx == NULL) {
        return;
    }

    map_mlp_iter_t iter;
    if (!map_mlp_iter_init(&iter, mlp_fs, v1_offset)) {
        if (is_polygon_layer && diag != NULL) {
            diag->polygons_skipped++;
        }
        return;
    }

    gfx_point_t *screen_points = is_polygon_layer ? s_polygon_buffer : NULL;
    int16_t prev_sx = 0;
    int16_t prev_sy = 0;

    int32_t raw_x, raw_y;
    bool is_new_part;

    while (map_mlp_iter_next(&iter, &raw_x, &raw_y, &is_new_part)) {
        uint32_t point_index = iter.points_read - 1;
        int16_t sx, sy;

        project_to_screen(raw_x, raw_y, cam, vp, &sx, &sy);

        if (is_polygon_layer) {
            screen_points[point_index].x = sx;
            screen_points[point_index].y = sy;
        }

        if (!is_polygon_layer) {
            uint32_t current_part_offset = (iter.num_parts > 0) ? iter.parts[iter.current_part_idx] : 0;

            if (point_index > current_part_offset) {
                if (diag != NULL && diag->lines_drawn == 0) {
                    PURRGO_LOG(
                        "MAP: FIRST LINE "
                        "screen=(%d,%d)->(%d,%d) "
                        "viewport=(%d,%d,%u,%u)\n",
                        (int)prev_sx, (int)prev_sy, (int)sx, (int)sy,
                        (int)vp->offset_x, (int)vp->offset_y,
                        (unsigned)vp->width, (unsigned)vp->height
                    );
                }

                gfx_color_t prev_fg = gfx->color_fg;
                switch (style) {
                    case PURRGO_STYLE_DARK_GRAY_THICK_LINE:
                        gfx_set_color(gfx, DARK_GRAY, gfx->color_bg);
                        gfx_draw_thick_line(gfx, prev_sx, prev_sy, sx, sy, 3);
                        break;
                    case PURRGO_STYLE_DARK_GRAY_SEMITHICK_LINE:
                        gfx_set_color(gfx, DARK_GRAY, gfx->color_bg);
                        gfx_draw_thick_line(gfx, prev_sx, prev_sy, sx, sy, 2);
                        break;
                    case PURRGO_STYLE_DARK_GRAY_LINE:
                        gfx_set_color(gfx, DARK_GRAY, gfx->color_bg);
                        gfx_draw_line(gfx, prev_sx, prev_sy, sx, sy);
                        break;
                    case PURRGO_STYLE_DARK_GRAY_DASHED_LINE:
                        gfx_set_color(gfx, DARK_GRAY, gfx->color_bg);
                        gfx_draw_dashed_line(gfx, prev_sx, prev_sy, sx, sy);
                        break;
                    case PURRGO_STYLE_DARK_GRAY_DOTTED_LINE:
                        gfx_set_color(gfx, DARK_GRAY, gfx->color_bg);
                        gfx_draw_dotted_line(gfx, prev_sx, prev_sy, sx, sy);
                        break;
                    case PURRGO_STYLE_RAILWAY_LINE:
                        gfx_draw_railway_line(gfx, prev_sx, prev_sy, sx, sy);
                        break;
                    default:
                        break;
                }
                gfx_set_color(gfx, prev_fg, gfx->color_bg);

                if (diag != NULL) {
                    diag->lines_drawn++;
                }
            }
        }

        prev_sx = sx;
        prev_sy = sy;
    }

    /* Check for complete iteration for polygon correctness */
    if (iter.points_read < iter.num_points) {
        if (is_polygon_layer && diag != NULL) {
            diag->polygons_skipped++;
        }
        return;
    }

    if (is_polygon_layer) {
        uint32_t part_count = iter.num_parts;

        if (part_count == 0) {
            PURRGO_LOG("MAP: polygon geometry has no parts points=%ld\n", (long)iter.num_points);
            if (diag != NULL) {
                diag->polygons_skipped++;
            }
            return;
        }

        if (iter.num_points > UINT16_MAX || part_count > UINT16_MAX) {
            PURRGO_LOG("MAP: polygon geometry exceeds uint16_t limit points=%ld parts=%ld\n",
                (long)iter.num_points, (long)part_count);
            if (diag != NULL) {
                diag->polygons_skipped++;
            }
            return;
        }

        gfx_color_t prev_fg = gfx->color_fg;
        if (style == PURRGO_STYLE_LIGHT_GRAY_FILL) {
            gfx_set_color(gfx, LIGHT_GRAY, gfx->color_bg);
        } else if (style == PURRGO_STYLE_DARK_GRAY_FILL) {
            gfx_set_color(gfx, DARK_GRAY, gfx->color_bg);
        }

        gfx_fill_compound_polygon(
            gfx,
            screen_points,
            (uint16_t)iter.num_points,
            (uint32_t*)iter.parts,
            (uint16_t)part_count
        );

        gfx_set_color(gfx, prev_fg, gfx->color_bg);

        if (diag != NULL) {
            diag->polygons_filled++;
        }
    }
}
