#include "purrgo/track_renderer.h"
#include "purrgo/track_logger.h"
#include "map_projection.h"
#include "purrgo/gfx_line.h"

/*
 * Трек: чёрная линия 1 px с белой обводкой (по 1 px с каждой стороны).
 * Обводка отличает трек от ручья (чёрная линия 1 px без обводки) и
 * от маршрута (чёрная линия 3 px с белой осевой линией).
 */
#define TRACK_CASING_PX 3

/*
 * Один проход по всему треку. Сначала белая обводка всех отрезков, потом
 * чёрная линия поверх: иначе обводка следующего отрезка затирала бы
 * чёрную линию в стыках.
 */
static void track_render_pass(
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *vp,
    size_t num_points,
    bool casing)
{
    bool has_prev = false;
    int32_t prev_sx = 0, prev_sy = 0;

    for (size_t i = 0; i < num_points; i++) {
        track_point_t point;
        if (purrgo_logger_get_track_point(i, &point)) {
            int32_t sx, sy;
            project_to_screen_i32(point.lon_1e7, point.lat_1e7, camera, vp, &sx, &sy);

            if (has_prev) {
                /* Далёкий конец отрезка отсекается с сохранением наклона (не насыщается в int16). */
                int32_t x0 = prev_sx, y0 = prev_sy, x1 = sx, y1 = sy;

                if (map_clip_segment_to_safe(&x0, &y0, &x1, &y1)) {
                    if (casing) {
                        gfx_draw_thick_line(gfx, (int16_t)x0, (int16_t)y0,
                                            (int16_t)x1, (int16_t)y1, TRACK_CASING_PX);
                    } else {
                        gfx_draw_line(gfx, (int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1);
                    }
                }
            }
            prev_sx = sx;
            prev_sy = sy;
            has_prev = true;
        }
    }
}

void purrgo_track_render(
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *vp,
    const char *gpx_filepath)
{
    if (!gfx || !camera || !vp || !gpx_filepath) return;

    size_t num_points = purrgo_logger_get_track_point_count();

    if (num_points == 0) {
        return;
    }

    gfx_set_color(gfx, WHITE, gfx->color_bg);
    track_render_pass(gfx, camera, vp, num_points, true);

    gfx_set_color(gfx, BLACK, gfx->color_bg);
    track_render_pass(gfx, camera, vp, num_points, false);
}

/* Объединение прямоугольников (rect_valid - есть ли в acc уже что-то). */
static void rect_union(purrgo_rect_t *acc, bool *acc_valid, const purrgo_rect_t *r)
{
    if (!*acc_valid) {
        *acc = *r;
        *acc_valid = true;
        return;
    }

    const int16_t x1 = (int16_t)((acc->x + acc->w > r->x + r->w) ? (acc->x + acc->w) : (r->x + r->w));
    const int16_t y1 = (int16_t)((acc->y + acc->h > r->y + r->h) ? (acc->y + acc->h) : (r->y + r->h));

    if (r->x < acc->x) { acc->x = r->x; }
    if (r->y < acc->y) { acc->y = r->y; }
    acc->w = (int16_t)(x1 - acc->x);
    acc->h = (int16_t)(y1 - acc->y);
}

/*
 * Рисует один отрезок трека (по точкам p0 -> p1) с отсечением по окну карты.
 * true - отрезок попал в окно; out_rect - его рамка (с запасом 1 px, внутри окна).
 */
static bool draw_segment(
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *vp,
    const track_point_t *p0,
    const track_point_t *p1,
    bool casing,
    purrgo_rect_t *out_rect)
{
    int32_t x0, y0, x1, y1;
    project_to_screen_i32(p0->lon_1e7, p0->lat_1e7, camera, vp, &x0, &y0);
    project_to_screen_i32(p1->lon_1e7, p1->lat_1e7, camera, vp, &x1, &y1);

    /* Далёкий конец отрезка отсекается с сохранением наклона; отрезок целиком вне области не рисуется. */
    if (!map_clip_segment_to_safe(&x0, &y0, &x1, &y1)) {
        return false;
    }

    int16_t min_x = (int16_t)((x0 < x1) ? x0 : x1);
    int16_t max_x = (int16_t)((x0 > x1) ? x0 : x1);
    int16_t min_y = (int16_t)((y0 < y1) ? y0 : y1);
    int16_t max_y = (int16_t)((y0 > y1) ? y0 : y1);

    min_x -= 1;
    max_x += 1;
    min_y -= 1;
    max_y += 1;

    if (max_x < vp->offset_x || min_x >= vp->offset_x + vp->width ||
        max_y < vp->offset_y || min_y >= vp->offset_y + vp->height) {
        return false;
    }

    if (casing) {
        gfx_draw_thick_line(gfx, (int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1,
                            TRACK_CASING_PX);
    } else {
        gfx_draw_line(gfx, (int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1);
    }

    if (min_x < vp->offset_x) min_x = vp->offset_x;
    if (max_x >= vp->offset_x + vp->width) max_x = (int16_t)(vp->offset_x + vp->width - 1);
    if (min_y < vp->offset_y) min_y = vp->offset_y;
    if (max_y >= vp->offset_y + vp->height) max_y = (int16_t)(vp->offset_y + vp->height - 1);

    out_rect->x = min_x;
    out_rect->y = min_y;
    out_rect->w = (int16_t)(max_x - min_x + 1);
    out_rect->h = (int16_t)(max_y - min_y + 1);
    return out_rect->w > 0 && out_rect->h > 0;
}

bool purrgo_track_render_new_segments(
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *vp,
    uint32_t *drawn_serial,
    purrgo_rect_t *out_rect)
{
    if (!gfx || !camera || !vp || !drawn_serial || !out_rect) return false;

    const uint32_t serial = purrgo_logger_get_track_serial();
    const size_t count = purrgo_logger_get_track_point_count();
    const uint32_t added = (uint32_t)(serial - *drawn_serial);

    *drawn_serial = serial;
    if (added == 0U || count < 2U) {
        return false;
    }

    /* Новые точки - последние `added` в буфере (старые могли быть вытеснены). */
    const size_t first_new = (added < count) ? (count - added) : 0U;

    /* Рисуем только внутри окна карты: рамки строк статуса не трогаем. */
    const int16_t old_clip_x = gfx->clip_x;
    const int16_t old_clip_y = gfx->clip_y;
    const int16_t old_clip_w = gfx->clip_w;
    const int16_t old_clip_h = gfx->clip_h;
    gfx_set_clip(gfx, vp->offset_x, vp->offset_y, (int16_t)vp->width, (int16_t)vp->height);
    bool any = false;

    /* Отрезок до первой новой точки связывает её с уже нарисованной. */
    const size_t i0 = (first_new > 0U) ? first_new : 1U;

    /*
     * Обводка нового отрезка затирает конец предыдущей чёрной линии у стыка,
     * поэтому чёрная линия перерисовывается на один отрезок глубже (i0 - 1,
     * уже нарисованный, без своей обводки). Сначала белая обводка новых
     * отрезков, потом чёрная линия.
     */
    const size_t black_from = (i0 >= 2U) ? (i0 - 1U) : i0;

    for (int pass = 0; pass < 2; pass++) {
        const bool casing = (pass == 0);
        const size_t from = casing ? i0 : black_from;
        track_point_t prev;

        gfx_set_color(gfx, casing ? WHITE : BLACK, gfx->color_bg);

        if (!purrgo_logger_get_track_point(from - 1U, &prev)) {
            continue;
        }
        for (size_t i = from; i < count; i++) {
            track_point_t cur;
            purrgo_rect_t r;

            if (!purrgo_logger_get_track_point(i, &cur)) {
                break;
            }
            if (draw_segment(gfx, camera, vp, &prev, &cur, casing, &r)) {
                rect_union(out_rect, &any, &r);
            }
            prev = cur;
        }
    }

    gfx_set_clip(gfx, old_clip_x, old_clip_y, old_clip_w, old_clip_h);
    return any;
}
