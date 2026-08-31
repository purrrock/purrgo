#include "map_render.h"
#include "map_mlp.h"
#include "map_projection.h"
#include "purrgo/logger.h"
#include "purrgo/gfx_line.h"
#include "purrgo/gfx_polygon.h"
#include <stddef.h>


#define PURRGO_MAX_LABELS_PER_FRAME 32

typedef struct {
    int16_t min_x;
    int16_t min_y;
    int16_t max_x;
    int16_t max_y;
} label_bbox_t;

static label_bbox_t s_drawn_labels[PURRGO_MAX_LABELS_PER_FRAME];
static uint16_t s_drawn_labels_count = 0;

// Вызывать один раз перед началом рендера нового кадра (например, в purrgo_map_render_viewport)
void map_render_clear_labels(void) {
    s_drawn_labels_count = 0;
}

// Проверка коллизий и резервирование места
bool map_render_try_place_label(int16_t x, int16_t y, uint16_t width_px, uint16_t height_px) {
    label_bbox_t new_box = {
        .min_x = x,
        .min_y = y,
        .max_x = x + (int16_t)width_px,
        .max_y = y + (int16_t)height_px
    };

    for (uint16_t i = 0; i < s_drawn_labels_count; i++) {
        // Логика проверки пересечения BBox
        if (!(new_box.max_x < s_drawn_labels[i].min_x ||
              new_box.min_x > s_drawn_labels[i].max_x ||
              new_box.max_y < s_drawn_labels[i].min_y ||
              new_box.min_y > s_drawn_labels[i].max_y)) {
            return false; // Коллизия найдена, метку не рисуем
        }
    }

    if (s_drawn_labels_count < PURRGO_MAX_LABELS_PER_FRAME) {
        s_drawn_labels[s_drawn_labels_count++] = new_box;
        return true;
    }

    return false; // Лимит меток исчерпан
}



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
