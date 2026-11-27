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

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} purrgo_rect_t;

/**
 * @brief Дорисовывает отрезки трека, добавленные после предыдущего вызова.
 *
 * Для инкрементальной отрисовки активного трека без полной перерисовки карты.
 * Рисует ВСЕ новые отрезки (а не только последний): между двумя обновлениями
 * экрана точек может добавиться несколько. Первый новый отрезок соединяет
 * первую новую точку с последней уже нарисованной. Рисование ограничено
 * окном карты vp.
 *
 * @param gfx          Графический контекст.
 * @param camera       Текущий географический bbox камеры.
 * @param vp           Окно карты на экране.
 * @param drawn_serial Номер последней нарисованной точки (purrgo_logger_get_track_serial());
 *                     после вызова равен текущему номеру. После полной отрисовки
 *                     трека (purrgo_track_render) вызывающий задаёт его сам.
 * @param out_rect     Объединённая рамка нарисованных отрезков (внутри окна карты).
 * @return true, если что-то нарисовано (out_rect заполнен).
 */
bool purrgo_track_render_new_segments(
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *vp,
    uint32_t *drawn_serial,
    purrgo_rect_t *out_rect
);

/**
 * @brief Рамка хвоста трека (без рисования): отрезка от последней точки трека до позиции
 *        (lat_1e7, lon_1e7), с запасом на обводку, внутри окна карты.
 *
 * @return true, если в RAM-треке есть точка и отрезок попадает в окно (out_rect заполнен).
 */
bool purrgo_track_tail_rect(
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *vp,
    int32_t lat_1e7,
    int32_t lon_1e7,
    purrgo_rect_t *out_rect
);

/**
 * @brief Рисует "хвост" трека: отрезок от последней записанной точки до текущей позиции
 *        (маркёра GNSS), тем же стилем, что и сам трек.
 *
 * Запись точек идёт по фильтру (отклонение, шаг, таймаут), поэтому конец записанного
 * трека обычно отстаёт от маркёра. Хвост - временное изображение: его не хранят, а
 * стирают возвратом сохранённого фона и рисуют заново при каждом движении маркёра.
 * Рисование ограничено окном карты vp.
 *
 * @return true, если что-то нарисовано (out_rect - рамка, как у purrgo_track_tail_rect()).
 */
bool purrgo_track_render_tail(
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *vp,
    int32_t lat_1e7,
    int32_t lon_1e7,
    purrgo_rect_t *out_rect
);

#endif // PURRGO_TRACK_RENDERER_H
