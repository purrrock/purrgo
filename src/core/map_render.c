#include "map_render.h"
#include "map_mlp.h"
#include "map_projection.h"
#include "purrgo/logger.h"
#include "purrgo/gfx_line.h"
#include "purrgo/gfx_polygon.h"
#include "purrgo/gfx_text.h"
#include <stddef.h>
#include <string.h>


#define PURRGO_MAX_LABELS_PER_FRAME 32
#define PURRGO_LABEL_TEXT_MAX_LEN    64


/*
 * BBox уже размещённой подписи.
 *
 * Эти прямоугольники используются для decluttering:
 * новая подпись не должна пересекаться с уже размещённой.
 */
typedef struct {
    int16_t min_x;
    int16_t min_y;
    int16_t max_x;
    int16_t max_y;
} label_bbox_t;


/*
 * Отложенная подпись.
 *
 * Landuse labels сначала складываются сюда во время обработки
 * landuse. После завершения roads они реально отрисовываются.
 */
typedef struct {
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    char text[PURRGO_LABEL_TEXT_MAX_LEN];
} queued_label_t;


/*
 * BBox уже размещённых подписей текущего кадра.
 */
static label_bbox_t s_drawn_labels[PURRGO_MAX_LABELS_PER_FRAME];
static uint16_t s_drawn_labels_count = 0;


/*
 * Очередь подписей, которые должны быть отрисованы позже.
 */
static queued_label_t s_queued_labels[PURRGO_MAX_LABELS_PER_FRAME];
static uint16_t s_queued_labels_count = 0;


/*
 * Буфер точек полигона.
 */
static gfx_point_t s_polygon_buffer[PURRGO_MAP_MAX_POINTS];


void map_render_clear_labels(void)
{
    /*
     * Начинаем новый кадр.
     *
     * Очищаем как BBox уже размещённых подписей,
     * так и очередь отложенных подписей.
     */
    s_drawn_labels_count = 0;
    s_queued_labels_count = 0;
}


bool map_render_try_place_label(
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h
)
{
    label_bbox_t new_box = {
        .min_x = x,
        .min_y = y,
        .max_x = x + (int16_t)w,
        .max_y = y + (int16_t)h
    };

    /*
     * Проверяем пересечение новой подписи
     * со всеми уже размещёнными подписями.
     */
    for (uint16_t i = 0; i < s_drawn_labels_count; i++) {

        /*
         * Проверка пересечения двух прямоугольников.
         */
        if (!(new_box.max_x < s_drawn_labels[i].min_x ||
              new_box.min_x > s_drawn_labels[i].max_x ||
              new_box.max_y < s_drawn_labels[i].min_y ||
              new_box.min_y > s_drawn_labels[i].max_y)) {

            return false;
        }
    }

    /*
     * Если место свободно и лимит ещё не достигнут,
     * резервируем BBox.
     */
    if (s_drawn_labels_count < PURRGO_MAX_LABELS_PER_FRAME) {
        s_drawn_labels[s_drawn_labels_count++] = new_box;
        return true;
    }

    /*
     * Достигнут максимальный размер кэша меток.
     */
    return false;
}


bool map_render_queue_label(
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    const char *text
)
{
    if (text == NULL) {
        return false;
    }

    /*
     * Не допускаем переполнения очереди.
     */
    if (s_queued_labels_count >= PURRGO_MAX_LABELS_PER_FRAME) {
        return false;
    }

    queued_label_t *label = &s_queued_labels[s_queued_labels_count];

    label->x = x;
    label->y = y;
    label->w = w;
    label->h = h;

    /*
     * Копируем текст в собственный буфер очереди.
     *
     * Это важно: вызывающий код может использовать локальный
     * char-массив после возврата из map_render_queue_label().
     */
    uint16_t i = 0;

    while (i < PURRGO_LABEL_TEXT_MAX_LEN - 1 &&
           text[i] != '\0') {

        label->text[i] = text[i];
        i++;
    }

    label->text[i] = '\0';

    s_queued_labels_count++;

    return true;
}


void map_render_draw_queued_labels(gfx_context_t *gfx)
{
    if (gfx == NULL) {
        return;
    }

    /*
     * Обрабатываем очередь в том же порядке,
     * в котором подписи были обнаружены при обходе landuse.
     */
    for (uint16_t i = 0; i < s_queued_labels_count; i++) {

        queued_label_t *label = &s_queued_labels[i];

        /*
         * Только сейчас выполняем collision detection
         * и резервируем место.
         *
         * К этому моменту roads уже нарисованы.
         */
        if (map_render_try_place_label(
                label->x,
                label->y,
                label->w,
                label->h)) {

            gfx_draw_string_halo(
                gfx,
                label->x,
                label->y,
                label->text
            );
        }
    }

    /*
     * Очередь больше не нужна.
     *
     * BBox-кэш НЕ очищаем — его записи должны оставаться
     * для последующих POI labels.
     */
    s_queued_labels_count = 0;
}


void map_render_feature(
    purrgo_fs_t *mlp_fs,
    uint32_t v1_offset,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    gfx_context_t *gfx,
    bool is_polygon_layer,
    purrgo_map_style_t style,
    map_diag_t *diag
)
{
    if (mlp_fs == NULL ||
        cam == NULL ||
        vp == NULL ||
        gfx == NULL) {

        return;
    }

    map_mlp_iter_t iter;

    if (!map_mlp_iter_init(&iter, mlp_fs, v1_offset)) {

        if (is_polygon_layer && diag != NULL) {
            diag->polygons_skipped++;
        }

        return;
    }

    /*
     * Для полигонов используем общий статический буфер.
     * Для линий он не нужен.
     */
    gfx_point_t *screen_points =
        is_polygon_layer ? s_polygon_buffer : NULL;

    int16_t prev_sx = 0;
    int16_t prev_sy = 0;

    int32_t raw_x;
    int32_t raw_y;
    bool is_new_part;


    while (map_mlp_iter_next(
        &iter,
        &raw_x,
        &raw_y,
        &is_new_part)) {

        uint32_t point_index = iter.points_read - 1;

        int16_t sx;
        int16_t sy;

        project_to_screen(
            raw_x,
            raw_y,
            cam,
            vp,
            &sx,
            &sy
        );


        if (is_polygon_layer) {

            screen_points[point_index].x = sx;
            screen_points[point_index].y = sy;
        }


        if (!is_polygon_layer) {

            uint32_t current_part_offset =
                (iter.num_parts > 0)
                    ? iter.parts[iter.current_part_idx]
                    : 0;


            if (point_index > current_part_offset) {

                gfx_color_t prev_fg = gfx->color_fg;


                switch (style) {

                    case PURRGO_STYLE_DARK_GRAY_THICK_LINE:

                        gfx_set_color(
                            gfx,
                            DARK_GRAY,
                            gfx->color_bg
                        );

                        gfx_draw_thick_line(
                            gfx,
                            prev_sx,
                            prev_sy,
                            sx,
                            sy,
                            3
                        );

                        break;


                    case PURRGO_STYLE_DARK_GRAY_SEMITHICK_LINE:

                        gfx_set_color(
                            gfx,
                            DARK_GRAY,
                            gfx->color_bg
                        );

                        gfx_draw_thick_line(
                            gfx,
                            prev_sx,
                            prev_sy,
                            sx,
                            sy,
                            2
                        );

                        break;


                    case PURRGO_STYLE_DARK_GRAY_LINE:

                        gfx_set_color(
                            gfx,
                            DARK_GRAY,
                            gfx->color_bg
                        );

                        gfx_draw_line(
                            gfx,
                            prev_sx,
                            prev_sy,
                            sx,
                            sy
                        );

                        break;


                    case PURRGO_STYLE_DARK_GRAY_DASHED_LINE:

                        gfx_set_color(
                            gfx,
                            DARK_GRAY,
                            gfx->color_bg
                        );

                        gfx_draw_dashed_line(
                            gfx,
                            prev_sx,
                            prev_sy,
                            sx,
                            sy
                        );

                        break;


                    case PURRGO_STYLE_DARK_GRAY_DOTTED_LINE:

                        gfx_set_color(
                            gfx,
                            DARK_GRAY,
                            gfx->color_bg
                        );

                        gfx_draw_dotted_line(
                            gfx,
                            prev_sx,
                            prev_sy,
                            sx,
                            sy
                        );

                        break;


                    case PURRGO_STYLE_RAILWAY_LINE:

                        gfx_draw_railway_line(
                            gfx,
                            prev_sx,
                            prev_sy,
                            sx,
                            sy
                        );

                        break;


                    default:
                        break;
                }


                /*
                 * Восстанавливаем исходный цвет foreground.
                 */
                gfx_set_color(
                    gfx,
                    prev_fg,
                    gfx->color_bg
                );


                if (diag != NULL) {
                    diag->lines_drawn++;
                }
            }
        }


        prev_sx = sx;
        prev_sy = sy;
    }


    /*
     * Проверяем, была ли полностью прочитана геометрия.
     */
    if (iter.points_read < iter.num_points) {

        if (is_polygon_layer && diag != NULL) {
            diag->polygons_skipped++;
        }

        return;
    }


    if (is_polygon_layer) {

        uint32_t part_count = iter.num_parts;


        if (part_count == 0) {

            PURRGO_LOG(
                "MAP: polygon geometry has no parts points=%ld\n",
                (long)iter.num_points
            );

            if (diag != NULL) {
                diag->polygons_skipped++;
            }

            return;
        }


        if (iter.num_points > UINT16_MAX ||
            part_count > UINT16_MAX) {

            PURRGO_LOG(
                "MAP: polygon geometry exceeds uint16_t limit "
                "points=%ld parts=%ld\n",
                (long)iter.num_points,
                (long)part_count
            );

            if (diag != NULL) {
                diag->polygons_skipped++;
            }

            return;
        }


        gfx_color_t prev_fg = gfx->color_fg;


        if (style == PURRGO_STYLE_LIGHT_GRAY_FILL) {

            gfx_set_color(
                gfx,
                LIGHT_GRAY,
                gfx->color_bg
            );

        } else if (style == PURRGO_STYLE_DARK_GRAY_FILL) {

            gfx_set_color(
                gfx,
                DARK_GRAY,
                gfx->color_bg
            );
        }


        gfx_fill_compound_polygon(
            gfx,
            screen_points,
            (uint16_t)iter.num_points,
            (uint32_t *)iter.parts,
            (uint16_t)part_count
        );


        /*
         * Восстанавливаем исходный foreground.
         */
        gfx_set_color(
            gfx,
            prev_fg,
            gfx->color_bg
        );


        if (diag != NULL) {
            diag->polygons_filled++;
        }
    }
}