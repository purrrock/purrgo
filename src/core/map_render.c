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
 * Буфер точек полигона (int32: вершины далёких полигонов не насыщаются в int16,
 * см. project_to_screen_i32()). 512 точек * 8 байт = 4 КБ.
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


/*
 * Отрезок буфера точек на экран: отсечение по безопасному квадрату (как у
 * обычных линий), толщина 1 - gfx_draw_line, больше - gfx_draw_thick_line.
 */
static void map_draw_buffer_segment(
    gfx_context_t *gfx,
    const gfx_point_t *a,
    const gfx_point_t *b,
    int16_t width
)
{
    int32_t x0 = a->x;
    int32_t y0 = a->y;
    int32_t x1 = b->x;
    int32_t y1 = b->y;

    if (!map_clip_segment_to_safe(&x0, &y0, &x1, &y1)) {
        return;
    }

    if (width <= 1) {
        gfx_draw_line(gfx, (int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1);
    } else {
        gfx_draw_thick_line(
            gfx, (int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1, width
        );
    }
}


/*
 * Диапазон точек одной части [first, end) из буфера: ломаная
 * (close_ring == false) или контур кольца (close_ring == true, замыкающий
 * отрезок добавляется, если кольцо не замкнуто явно).
 */
static void map_draw_buffer_part(
    gfx_context_t *gfx,
    const gfx_point_t *pts,
    uint32_t first,
    uint32_t end,
    bool close_ring,
    int16_t width
)
{
    if (end < first + 2) {
        return;
    }

    for (uint32_t i = first; i + 1 < end; i++) {
        map_draw_buffer_segment(gfx, &pts[i], &pts[i + 1], width);
    }

    if (close_ring &&
        (pts[first].x != pts[end - 1].x ||
         pts[first].y != pts[end - 1].y)) {
        map_draw_buffer_segment(gfx, &pts[end - 1], &pts[first], width);
    }
}


/*
 * Все части объекта одним цветом и одной толщиной. Таблица частей берётся из
 * итератора; без частей (num_parts == 0) весь буфер - одна часть.
 */
static void map_draw_buffer_parts(
    gfx_context_t *gfx,
    const gfx_point_t *pts,
    uint32_t num_points,
    const uint32_t *parts,
    uint32_t num_parts,
    bool close_ring,
    gfx_color_t color,
    int16_t width
)
{
    gfx_set_color(gfx, color, gfx->color_bg);

    if (num_parts == 0) {
        map_draw_buffer_part(gfx, pts, 0, num_points, close_ring, width);
        return;
    }

    for (uint32_t i = 0; i < num_parts; i++) {
        uint32_t first = parts[i];
        uint32_t end = (i + 1 < num_parts) ? parts[i + 1] : num_points;

        if (first >= num_points) {
            continue;
        }
        if (end > num_points) {
            end = num_points;
        }

        map_draw_buffer_part(gfx, pts, first, end, close_ring, width);
    }
}


/*
 * Штриховка полигонов слоя water (без обводки контура).
 *
 * Болото: горизонтальные штрихи 4 px через 4 px, строки через 4 px, каждая
 * следующая строка сдвинута на полпериода (шахматный порядок).
 * Ледник: диагональные линии 45 градусов, шаг 6 px по горизонтали.
 */
static const gfx_hatch_t s_wetland_hatch = {
    .row_period  = 4,
    .dash_len    = 4,
    .dash_period = 8,
    .row_shift   = 4
};

/*
 * Пляж, песок и подобное: одиночные точки с шагом 4 px по x и 3 px по y,
 * нечётные строки сдвинуты на 2 px (шахматный порядок).
 */
static const gfx_hatch_t s_sand_hatch = {
    .row_period  = 3,
    .dash_len    = 1,
    .dash_period = 4,
    .row_shift   = 2
};

static const gfx_hatch_t s_glacier_hatch = {
    .row_period  = 1,
    .dash_len    = 1,
    .dash_period = 6,
    .row_shift   = 1
};


void map_render_feature(
    purrgo_file_t *mlp_file,
    uint32_t v1_offset,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    gfx_context_t *gfx,
    bool is_polygon_layer,
    purrgo_map_style_t style
)
{
    if (mlp_file == NULL ||
        cam == NULL ||
        vp == NULL ||
        gfx == NULL) {

        return;
    }

    map_mlp_iter_t iter;

    if (!map_mlp_iter_init(&iter, mlp_file, v1_offset)) {
        return;
    }

    /*
     * Для полигонов используем общий статический буфер.
     * Для линий он не нужен.
     */
    /*
     * Линии воды (чёрное ядро с тёмно-серой обводкой) рисуются в два прохода:
     * сначала обводка всех звеньев, затем ядро. Для этого точки объекта
     * собираются в тот же буфер (объект до PURRGO_MAP_MAX_POINTS точек
     * гарантирует map_mlp_iter_init()).
     */
    const bool is_water_line =
        !is_polygon_layer &&
        (style == PURRGO_STYLE_WATER_THICK_LINE ||
         style == PURRGO_STYLE_WATER_LINE);

    gfx_point_t *screen_points =
        (is_polygon_layer || is_water_line) ? s_polygon_buffer : NULL;

    int32_t prev_sx = 0;
    int32_t prev_sy = 0;

    int32_t raw_x;
    int32_t raw_y;
    bool is_new_part;


    while (map_mlp_iter_next(
        &iter,
        &raw_x,
        &raw_y,
        &is_new_part)) {

        uint32_t point_index = iter.points_read - 1;

        int32_t sx;
        int32_t sy;

        project_to_screen_i32(
            raw_x,
            raw_y,
            cam,
            vp,
            &sx,
            &sy
        );


        if (screen_points != NULL) {

            screen_points[point_index].x = sx;
            screen_points[point_index].y = sy;
        }


        if (!is_polygon_layer && !is_water_line) {

            uint32_t current_part_offset =
                (iter.num_parts > 0)
                    ? iter.parts[iter.current_part_idx]
                    : 0;


            if (point_index > current_part_offset) {

                /*
                 * Отрезок с концом далеко за экраном отсекается по
                 * +-PURRGO_SAFE_COORD с сохранением наклона (раньше конец
                 * насыщался в int16, и наклон менялся). Отрезок внутри этой
                 * области рисуется как есть.
                 */
                int32_t seg_x0 = prev_sx;
                int32_t seg_y0 = prev_sy;
                int32_t seg_x1 = sx;
                int32_t seg_y1 = sy;

                if (!map_clip_segment_to_safe(
                        &seg_x0, &seg_y0, &seg_x1, &seg_y1)) {

                    prev_sx = sx;
                    prev_sy = sy;
                    continue;
                }

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
                            (int16_t)seg_x0,
                            (int16_t)seg_y0,
                            (int16_t)seg_x1,
                            (int16_t)seg_y1,
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
                            (int16_t)seg_x0,
                            (int16_t)seg_y0,
                            (int16_t)seg_x1,
                            (int16_t)seg_y1,
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
                            (int16_t)seg_x0,
                            (int16_t)seg_y0,
                            (int16_t)seg_x1,
                            (int16_t)seg_y1
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
                            (int16_t)seg_x0,
                            (int16_t)seg_y0,
                            (int16_t)seg_x1,
                            (int16_t)seg_y1
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
                            (int16_t)seg_x0,
                            (int16_t)seg_y0,
                            (int16_t)seg_x1,
                            (int16_t)seg_y1
                        );

                        break;


                    case PURRGO_STYLE_RAILWAY_LINE:

                        gfx_draw_railway_line(
                            gfx,
                            (int16_t)seg_x0,
                            (int16_t)seg_y0,
                            (int16_t)seg_x1,
                            (int16_t)seg_y1
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
            }
        }


        prev_sx = sx;
        prev_sy = sy;
    }


    /*
     * Проверяем, была ли полностью прочитана геометрия.
     */
    if (iter.points_read < iter.num_points) {
        return;
    }


    if (is_water_line) {

        const bool thick = (style == PURRGO_STYLE_WATER_THICK_LINE);
        const gfx_color_t prev_fg_line = gfx->color_fg;

        /* Проход 1: тёмно-серая обводка. Проход 2: чёрное ядро. */
        map_draw_buffer_parts(
            gfx, screen_points, iter.num_points,
            iter.parts, iter.num_parts, false,
            DARK_GRAY, thick ? 5 : 3
        );
        map_draw_buffer_parts(
            gfx, screen_points, iter.num_points,
            iter.parts, iter.num_parts, false,
            BLACK, thick ? 3 : 1
        );

        gfx_set_color(gfx, prev_fg_line, gfx->color_bg);

        return;
    }


    if (is_polygon_layer) {

        uint32_t part_count = iter.num_parts;


        if (part_count == 0) {

            PURRGO_LOG(
                "MAP: polygon geometry has no parts points=%ld\n\r",
                (long)iter.num_points
            );

            return;
        }


        if (iter.num_points > UINT16_MAX ||
            part_count > UINT16_MAX) {

            PURRGO_LOG(
                "MAP: polygon geometry exceeds uint16_t limit "
                "points=%ld parts=%ld\n\r",
                (long)iter.num_points,
                (long)part_count
            );

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

        } else if (style == PURRGO_STYLE_WATER_FILL) {

            gfx_set_color(
                gfx,
                BLACK,
                gfx->color_bg
            );

        } else if (style == PURRGO_STYLE_WETLAND_HATCH) {

            gfx_set_color(
                gfx,
                BLACK,
                gfx->color_bg
            );

        } else if (style == PURRGO_STYLE_GLACIER_HATCH ||
                   style == PURRGO_STYLE_SAND_DOTS) {

            gfx_set_color(
                gfx,
                DARK_GRAY,
                gfx->color_bg
            );
        }


        if (style == PURRGO_STYLE_WETLAND_HATCH) {

            gfx_fill_compound_polygon_hatched(
                gfx,
                screen_points,
                (uint16_t)iter.num_points,
                (uint32_t *)iter.parts,
                (uint16_t)part_count,
                &s_wetland_hatch
            );

        } else if (style == PURRGO_STYLE_GLACIER_HATCH) {

            gfx_fill_compound_polygon_hatched(
                gfx,
                screen_points,
                (uint16_t)iter.num_points,
                (uint32_t *)iter.parts,
                (uint16_t)part_count,
                &s_glacier_hatch
            );

        } else if (style == PURRGO_STYLE_SAND_DOTS) {

            gfx_fill_compound_polygon_hatched(
                gfx,
                screen_points,
                (uint16_t)iter.num_points,
                (uint32_t *)iter.parts,
                (uint16_t)part_count,
                &s_sand_hatch
            );

        } else {

            gfx_fill_compound_polygon(
                gfx,
                screen_points,
                (uint16_t)iter.num_points,
                (uint32_t *)iter.parts,
                (uint16_t)part_count
            );
        }


        /*
         * Вода: тёмно-серая обводка поверх чёрной заливки. Контур строится
         * по тем же точкам из буфера, отрезки отсекаются по безопасному
         * квадрату (вершины далёких полигонов выходят за int16).
         */
        if (style == PURRGO_STYLE_WATER_FILL) {

            map_draw_buffer_parts(
                gfx, screen_points, iter.num_points,
                iter.parts, iter.num_parts, true,
                DARK_GRAY, 1
            );
        }


        /*
         * Восстанавливаем исходный foreground.
         */
        gfx_set_color(
            gfx,
            prev_fg,
            gfx->color_bg
        );
    }
}