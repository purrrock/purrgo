#ifndef PURRGO_MAP_PROJECTION_H
#define PURRGO_MAP_PROJECTION_H

#include <stdbool.h>
#include <stdint.h>
#include "purrgo/map.h"

int64_t camera_span_x(const purrgo_bbox_t *cam);

/*
 * Проекция точки на экран с насыщением результата в int16. Годится для точек,
 * которые рисуются по одной (маркер, POI, иконки): далеко за экраном точка всё
 * равно не видна. Для вершин рёбер (полигоны, линии) использовать
 * project_to_screen_i32(): при насыщении менялся наклон ребра.
 */
void project_to_screen(
    int32_t lon,
    int32_t lat,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    int16_t *sx,
    int16_t *sy
);

/* Предел координат project_to_screen_i32() (2^29): разности двух координат помещаются в int32. */
#define PURRGO_PROJ_LIMIT ((int64_t)1 << 29)

/*
 * Безопасная область для int16-примитивов рисования линий: внутри
 * +-PURRGO_SAFE_COORD произведения в clip_line() помещаются в int32.
 */
#define PURRGO_SAFE_COORD 16384

/* Проекция без насыщения в int16: результат ограничен только +-PURRGO_PROJ_LIMIT. */
void project_to_screen_i32(
    int32_t lon,
    int32_t lat,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    int32_t *sx,
    int32_t *sy
);

/*
 * Отсечение отрезка по квадрату +-PURRGO_SAFE_COORD с сохранением наклона.
 * true - отрезок (возможно, укороченный) надо рисовать; false - он целиком вне
 * квадрата. Отрезок внутри квадрата не меняется.
 */
bool map_clip_segment_to_safe(
    int32_t *x0,
    int32_t *y0,
    int32_t *x1,
    int32_t *y1
);

#endif // PURRGO_MAP_PROJECTION_H
