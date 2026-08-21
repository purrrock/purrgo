// file: src/core/map.c

#include "purrgo/map.h"
#include "purrgo/logger.h"
#include "purrgo/gfx_line.h"
#include "purrgo/gfx_polygon.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define PURRGO_MAP_MAX_PARTS 32

/*
 * Текущий parser Python использует 2048 как защитное ограничение
 * количества точек одной MLP geometry.
 *
 * Это НЕ ограничение бинарного формата MLP.
 * Это только защитный предел данной реализации parser/render pipeline.
 *
 * Для текущего этапа он нужен потому, что polygon renderer должен
 * временно разместить projected points в памяти.
 */
#define PURRGO_MAP_MAX_POINTS 2048

/*
 * Статический буфер для рендеринга полигонов, чтобы избежать malloc.
 * 2048 точек достаточно для большинства объектов landuse на дисплее 128x296.
 */
static gfx_point_t s_polygon_buffer[PURRGO_MAP_MAX_POINTS];


/*
 * Диагностические счётчики map subsystem.
 *
 * Поток обработки:
 *
 * IDX -> SQT -> NAV -> DATA -> AABB -> MLP -> projection -> GFX
 *
 * Для polygon layer дополнительно считаются:
 *
 *   polygons_filled
 *   polygons_skipped
 *
 * polygons_skipped может включать внутренние кольца (holes), которые
 * на текущем этапе намеренно не заполняются.
 */
typedef struct {
    uint32_t sqt_blocks;
    uint32_t nav_visited;
    uint32_t data_visited;
    uint32_t data_passed;
    uint32_t data_culled;

    uint32_t lines_drawn;

    uint32_t polygons_filled;
    uint32_t polygons_skipped;

    /*
     * Ограничивает количество подробных диагностических сообщений
     * о DATA/NAV nodes.
     */
    uint32_t nodes_logged;
} map_diag_t;


/* -------------------------------------------------------------------------- */
/* Little-endian helpers                                                      */
/* -------------------------------------------------------------------------- */

/*
 * Безопасное чтение Little-Endian int32_t.
 */
static inline int32_t unpack_i32_le(const uint8_t *buf)
{
    return (int32_t)(
        (uint32_t)buf[0] |
        ((uint32_t)buf[1] << 8) |
        ((uint32_t)buf[2] << 16) |
        ((uint32_t)buf[3] << 24)
    );
}


/*
 * Безопасное чтение Little-Endian uint32_t.
 */
static inline uint32_t unpack_u32_le(const uint8_t *buf)
{
    return
        (uint32_t)buf[0] |
        ((uint32_t)buf[1] << 8) |
        ((uint32_t)buf[2] << 16) |
        ((uint32_t)buf[3] << 24);
}


/*
 * Чтение IEEE-754 float из четырёх байт Little-Endian.
 *
 * IDX хранит BBox NAV/DATA node как float.
 * Здесь выполняется только побитовое восстановление float.
 */
static inline float unpack_float_le(const uint8_t *buf)
{
    union {
        uint32_t i;
        float f;
    } u;

    u.i = unpack_u32_le(buf);

    return u.f;
}


/* -------------------------------------------------------------------------- */
/* Projection                                                                 */
/* -------------------------------------------------------------------------- */

/*
 * Преобразование координат карты в координаты framebuffer.
 *
 * PurrGo internal coordinate representation:
 *
 *     degrees * 10^7
 *
 * MLP source coordinate representation:
 *
 *     degrees * 10^6
 *
 * Поэтому parse_geometry_mlp() передаёт сюда:
 *
 *     raw_mlp_coordinate * 10
 *
 * Ось Y framebuffer инвертируется:
 *
 *     map min_y -> нижняя граница viewport
 *     map max_y -> верхняя граница viewport
 *
 * Для промежуточного умножения используется int64_t.
 */
static void project_to_screen(
    int32_t lon,
    int32_t lat,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    int16_t *sx,
    int16_t *sy
) {
    int64_t dx =
        (int64_t)(lon - cam->min_x) *
        (int64_t)vp->width;

    int64_t w =
        (int64_t)(cam->max_x - cam->min_x);

    *sx =
        (int16_t)(
            w > 0
                ? (dx / w)
                : 0
        ) +
        vp->offset_x;


    int64_t dy =
        (int64_t)(lat - cam->min_y) *
        (int64_t)vp->height;

    int64_t h =
        (int64_t)(cam->max_y - cam->min_y);

    *sy =
        (int16_t)(
            (int64_t)vp->height -
            (
                h > 0
                    ? (dy / h)
                    : 0
            )
        ) +
        vp->offset_y;
}


/* -------------------------------------------------------------------------- */
/* Polygon helpers                                                            */
/* -------------------------------------------------------------------------- */

/*
 * Вычисляет ориентированную площадь кольца в экранных координатах.
 *
 * Формула:
 *
 *     area2 = sum(x_i * y_(i+1) - x_(i+1) * y_i)
 *
 * В экранной системе PurrGo ось Y направлена вниз.
 *
 * Поэтому направление CW/CCW относительно географической системы
 * меняется на противоположное относительно screen coordinates.
 *
 * Для определения topology мы делаем вычисление в screen coordinates
 * с учётом этого факта.
 *
 * Возвращаемое значение:
 *
 *     > 0  -> географически CW
 *     < 0  -> географически CCW
 *      0  -> вырожденное кольцо
 *
 * Используется int64_t. Для координат int16_t и размеров дисплея
 * этого более чем достаточно.
 */
static int64_t polygon_ring_area2(
    const gfx_point_t *points,
    uint32_t count
) {
    if (points == NULL || count < 3) {
        return 0;
    }

    int64_t area2 = 0;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t j =
            (i + 1 < count)
                ? i + 1
                : 0;

        area2 +=
            (int64_t)points[i].x * points[j].y -
            (int64_t)points[j].x * points[i].y;
    }

    /*
     * Screen Y направлен вниз.
     *
     * В математической системе:
     *
     *     positive area = CCW
     *
     * В screen coordinates знак меняется.
     *
     * Поэтому:
     *
     *     screen area < 0 -> geographic CW
     *     screen area > 0 -> geographic CCW
     */
    return -area2;
}


/*
 * Проверка, является ли кольцо внешним.
 *
 * Согласно текущему описанию MLP:
 *
 *     Outer = CW
 *     Inner = CCW
 *
 * Поэтому:
 *
 *     area2 > 0 -> outer
 *     area2 < 0 -> hole
 */
static bool polygon_ring_is_outer(
    const gfx_point_t *points,
    uint32_t count
) {
    int64_t area2 =
        polygon_ring_area2(points, count);

    return area2 > 0;
}


/*
 * Проверка, что parts[] действительно содержит корректные start indices.
 *
 * Формат:
 *
 *     parts[0] = начало первого ring
 *     parts[1] = начало второго ring
 *     ...
 *
 * Для корректной geometry:
 *
 *     parts[0] == 0
 *     parts[i] < parts[i + 1]
 *     parts[last] < num_points
 */
static bool validate_parts(
    const uint32_t *parts,
    uint32_t num_parts,
    uint32_t num_points
) {
    if (num_parts == 0) {
        /*
         * Geometry без parts допускается только как обычная
         * последовательность точек для line renderer.
         */
        return true;
    }

    if (parts == NULL || num_points == 0) {
        return false;
    }

    if (parts[0] != 0) {
        return false;
    }

    for (uint32_t i = 0; i < num_parts; i++) {
        if (parts[i] >= num_points) {
            return false;
        }

        if (i > 0 && parts[i] <= parts[i - 1]) {
            return false;
        }
    }

    return true;
}


/* -------------------------------------------------------------------------- */
/* MLP geometry rendering                                                     */
/* -------------------------------------------------------------------------- */

/*
 * Рендерит одну MLP geometry.
 *
 * Для line layer:
 *
 *     geometry
 *         |
 *         +-- part 0 -> polyline
 *         +-- part 1 -> polyline
 *         +-- ...
 *
 * Для polygon layer:
 *
 *     geometry
 *         |
 *         +-- outer ring -> gfx_fill_polygon()
 *         +-- inner ring -> пока пропускается
 *
 * Почему geometry читается полностью:
 *
 * gfx_fill_polygon() принимает массив gfx_point_t.
 *
 * Поэтому потоковый алгоритм, который был достаточен для линий,
 * недостаточен для polygon fill.
 */
static void parse_geometry_mlp(
    purrgo_fs_t *mlp_fs,
    uint32_t v1_offset,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    gfx_context_t *gfx,
    bool is_polygon_layer,
    map_diag_t *diag
) {
    if (
        mlp_fs == NULL ||
        mlp_fs->read == NULL ||
        mlp_fs->seek == NULL ||
        cam == NULL ||
        vp == NULL ||
        gfx == NULL
    ) {
        return;
    }


    /*
     * Data Node v1 указывает на начало geometry body,
     * а не на 8-byte local geometry header.
     *
     * Поэтому:
     *
     *     file_offset = 32 + v1_offset
     *
     * 32 байта — YZL header.
     */
    if (!mlp_fs->seek(
            mlp_fs->handle,
            32u + v1_offset)) {

        return;
    }


    /*
     * Geometry body fixed header:
     *
     *   0x00 minx
     *   0x04 miny
     *   0x08 maxx
     *   0x0C maxy
     *   0x10 num_parts
     *   0x14 num_points
     */
    uint8_t head[24];

    if (
        mlp_fs->read(
            mlp_fs->handle,
            head,
            sizeof(head)
        ) != sizeof(head)
    ) {
        return;
    }


    int32_t num_parts =
        unpack_i32_le(&head[16]);

    int32_t num_points =
        unpack_i32_le(&head[20]);


    /*
     * Проверка структурных полей.
     *
     * num_parts может быть 0 для обычной line geometry.
     */
    if (
        num_parts < 0 ||
        num_parts > PURRGO_MAP_MAX_PARTS ||
        num_points <= 0 ||
        num_points > PURRGO_MAP_MAX_POINTS
    ) {
        PURRGO_LOG(
            "MAP: invalid MLP geometry "
            "parts=%ld points=%ld\n",
            (long)num_parts,
            (long)num_points
        );

        return;
    }


    uint32_t parts[PURRGO_MAP_MAX_PARTS] = {0};


    /*
     * parts[] располагается непосредственно после
     * фиксированных 24 байт geometry header.
     */
    if (num_parts > 0) {
        uint32_t bytes_to_read =
            (uint32_t)num_parts * 4u;

        uint8_t part_buf[
            PURRGO_MAP_MAX_PARTS * 4
        ];


        if (
            mlp_fs->read(
                mlp_fs->handle,
                part_buf,
                bytes_to_read
            ) != bytes_to_read
        ) {
            return;
        }


        for (uint32_t i = 0;
             i < (uint32_t)num_parts;
             i++) {

            parts[i] =
                unpack_u32_le(
                    &part_buf[i * 4u]
                );
        }
    }


    /*
     * Проверяем parts[] до чтения point array.
     */
    if (
        !validate_parts(
            parts,
            (uint32_t)num_parts,
            (uint32_t)num_points
        )
    ) {
        PURRGO_LOG(
            "MAP: invalid MLP parts "
            "parts=%ld points=%ld\n",
            (long)num_parts,
            (long)num_points
        );

        return;
    }


    /*
     * Для polygon rendering необходимо иметь весь массив
     * projected screen points.
     *
     * gfx_point_t:
     *
     *     int16_t x
     *     int16_t y
     *
     * Поэтому размер временного буфера:
     *
     *     num_points * sizeof(gfx_point_t)
     *
     * Буфер выделяется ровно под текущую geometry.
     *
     * Важно:
     * это временное решение Stage 2.
     * Для STM32 в дальнейшем можно заменить его на
     * специализированный streaming/ring-buffer renderer.
     */
    gfx_point_t *screen_points = NULL;

    if (is_polygon_layer) {
        if ((size_t)num_points > PURRGO_MAP_MAX_POINTS) {
            PURRGO_LOG(
                "MAP: polygon points %ld exceed static buffer size %d, skipping\n",
                (long)num_points,
                PURRGO_MAP_MAX_POINTS
            );
            if (diag != NULL) {
                diag->polygons_skipped++;
            }
            return;
        }

        screen_points = s_polygon_buffer;
    }


    /*
     * Для line rendering сохраняем старый streaming-подход:
     *
     * предыдущая точка -> текущая точка.
     */
    int16_t prev_sx = 0;
    int16_t prev_sy = 0;

    uint32_t current_part_idx = 0;

    uint32_t next_part_start =
        (num_parts > 1)
            ? parts[1]
            : (uint32_t)num_points;


    /*
     * Читаем все точки geometry.
     */
    for (
        uint32_t i = 0;
        i < (uint32_t)num_points;
        i++
    ) {
        uint8_t pt_buf[8];


        if (
            mlp_fs->read(
                mlp_fs->handle,
                pt_buf,
                sizeof(pt_buf)
            ) != sizeof(pt_buf)
        ) {
            /*
             * При ошибке чтения geometry нельзя использовать
             * частично заполненный polygon.
             */

            return;
        }


        /*
         * MLP:
         *
         *     X = longitude * 10^6
         *     Y = latitude  * 10^6
         *
         * PurrGo internal:
         *
         *     longitude * 10^7
         *     latitude  * 10^7
         *
         * Поэтому умножаем исходные MLP coordinates на 10.
         *
         * Это существующая и документированная конверсия проекта.
         */
        int32_t raw_x =
            unpack_i32_le(&pt_buf[0]);

        int32_t raw_y =
            unpack_i32_le(&pt_buf[4]);


        int32_t norm_x =
            raw_x * 10;

        int32_t norm_y =
            raw_y * 10;


        int16_t sx;
        int16_t sy;


        project_to_screen(
            norm_x,
            norm_y,
            cam,
            vp,
            &sx,
            &sy
        );


        /*
         * Polygon renderer сохраняет все projected points.
         */
        if (is_polygon_layer) {
            screen_points[i].x = sx;
            screen_points[i].y = sy;
        }


        /*
         * Определяем начало следующего part.
         *
         * parts[] содержит start index.
         */
        if (
            num_parts > 0 &&
            i == next_part_start
        ) {
            current_part_idx++;

            next_part_start =
                (
                    current_part_idx + 1 <
                    (uint32_t)num_parts
                )
                    ? parts[current_part_idx + 1]
                    : (uint32_t)num_points;
        }


        /*
         * Для line layer продолжаем старое потоковое
         * поведение.
         */
        if (!is_polygon_layer) {
            uint32_t current_part_offset =
                (num_parts > 0)
                    ? parts[current_part_idx]
                    : 0;


            /*
             * Первая точка каждого part не соединяется
             * с предыдущим part.
             */
            if (i > current_part_offset) {
                if (
                    diag != NULL &&
                    diag->lines_drawn == 0
                ) {
                    PURRGO_LOG(
                        "MAP: FIRST LINE "
                        "screen=(%d,%d)->(%d,%d) "
                        "viewport=(%d,%d,%u,%u)\n",
                        (int)prev_sx,
                        (int)prev_sy,
                        (int)sx,
                        (int)sy,
                        (int)vp->offset_x,
                        (int)vp->offset_y,
                        (unsigned)vp->width,
                        (unsigned)vp->height
                    );
                }


                gfx_draw_line(
                    gfx,
                    prev_sx,
                    prev_sy,
                    sx,
                    sy
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
     * Polygon rendering.
     */
    if (is_polygon_layer) {
        uint32_t part_count =
            (uint32_t)num_parts;


        /*
         * Формат MLP содержит parts[].
         *
         * Для polygon geometry без parts[] нет способа
         * определить границы нескольких колец.
         *
         * Поэтому такой объект не является корректным
         * multipart polygon.
         */
        if (part_count == 0) {
            PURRGO_LOG(
                "MAP: polygon geometry has no parts "
                "points=%ld\n",
                (long)num_points
            );


            return;
        }


        for (
            uint32_t part_idx = 0;
            part_idx < part_count;
            part_idx++
        ) {
            uint32_t start =
                parts[part_idx];


            uint32_t end =
                (
                    part_idx + 1 < part_count
                )
                    ? parts[part_idx + 1]
                    : (uint32_t)num_points;


            /*
             * parts[] задаёт start indices.
             *
             * Поэтому:
             *
             *     [start, end)
             *
             * является текущим ring.
             */
            if (
                start >= end ||
                end > (uint32_t)num_points
            ) {
                if (diag != NULL) {
                    diag->polygons_skipped++;
                }

                continue;
            }


            uint32_t ring_count =
                end - start;


            /*
             * Для заполнения нужен настоящий polygon:
             * минимум 3 точки.
             *
             * Обычно кольцо замкнуто, то есть первая и последняя
             * точки совпадают. gfx_fill_polygon() умеет замыкать
             * контур самостоятельно, поэтому дублированная
             * последняя точка допустима.
             */
            if (ring_count < 3) {
                if (diag != NULL) {
                    diag->polygons_skipped++;
                }

                continue;
            }


            gfx_point_t *ring_points =
                &screen_points[start];


            /*
             * Согласно формату:
             *
             *     Outer = CW
             *     Inner = CCW
             *
             * На текущем этапе внутренние кольца не передаются
             * в gfx_fill_polygon(), потому что тот использует
             * независимое заполнение одного кольца и не поддерживает
             * compound polygon / hole subtraction.
             *
             * Таким образом мы не получаем неправильное поведение,
             * при котором hole становится заполненной областью.
             */
            if (
                !polygon_ring_is_outer(
                    ring_points,
                    ring_count
                )
            ) {
                if (diag != NULL) {
                    diag->polygons_skipped++;
                }

                continue;
            }


            /*
             * Заполняем внешний контур.
             *
             * gfx_fill_polygon() использует текущий
             * graphics context.
             */
            gfx_fill_polygon(
                gfx,
                ring_points,
                (uint16_t)ring_count
            );


            /*
             * После заливки повторно рисуем контур.
             *
             * Это даёт чёткую границу polygon и соответствует
             * поведению обычного map renderer.
             */
            // gfx_draw_polygon(
            //    gfx,
            //    ring_points,
            //    (uint16_t)ring_count
            //);


            if (diag != NULL) {
                diag->polygons_filled++;
            }
        }
    }


}


/* -------------------------------------------------------------------------- */
/* IDX node parser                                                            */
/* -------------------------------------------------------------------------- */

/*
 * Рекурсивный обход SQT R-tree.
 *
 * is_nav_node:
 *
 *     true  -> текущий node является Navigation Node
 *     false -> текущий node является Data Node
 */
static void parse_node(
    purrgo_fs_t *idx_fs,
    uint32_t *current_idx_offset,
    purrgo_fs_t *mlp_fs,
    bool is_nav_node,
    uint32_t level,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    gfx_context_t *gfx,
    bool is_polygon_layer,
    map_diag_t *diag
) {
    (void)level;


    uint8_t node_buf[28];


    if (
        idx_fs->read(
            idx_fs->handle,
            node_buf,
            sizeof(node_buf)
        ) != sizeof(node_buf)
    ) {
        return;
    }


    *current_idx_offset += 28;


    /* ---------------------------------------------------------------------- */
    /* DATA NODE                                                              */
    /* ---------------------------------------------------------------------- */

    if (!is_nav_node) {
        if (diag != NULL) {
            diag->data_visited++;
        }


        /*
         * DATA BBox:
         *
         *     +0x00 xmin
         *     +0x04 ymin
         *     +0x08 xmax
         *     +0x0C ymax
         *
         * В исходном IDX эти значения являются float.
         */
        float f_xmin =
            unpack_float_le(&node_buf[0]);

        float f_ymin =
            unpack_float_le(&node_buf[4]);

        float f_xmax =
            unpack_float_le(&node_buf[8]);

        float f_ymax =
            unpack_float_le(&node_buf[12]);


        /*
         * PurrGo internal coordinate representation:
         *
         *     degrees * 10^7
         */
        int32_t xmin =
            (int32_t)(
                f_xmin * 10000000.0f
            );

        int32_t ymin =
            (int32_t)(
                f_ymin * 10000000.0f
            );

        int32_t xmax =
            (int32_t)(
                f_xmax * 10000000.0f
            );

        int32_t ymax =
            (int32_t)(
                f_ymax * 10000000.0f
            );


        /*
         * AABB intersection.
         *
         * Объект видим, если его BBox пересекает camera.
         */
        bool passes =
            xmax >= cam->min_x &&
            xmin <= cam->max_x &&
            ymax >= cam->min_y &&
            ymin <= cam->max_y;


        if (diag != NULL) {
            if (passes) {
                diag->data_passed++;
            } else {
                diag->data_culled++;
            }


            if (diag->nodes_logged < 10) {
                PURRGO_LOG(
                    "MAP: DATA "
                    "raw=(%08x,%08x,%08x,%08x) "
                    "flt=(%f,%f,%f,%f) "
                    "int=(%d,%d,%d,%d) %s\n",

                    unpack_u32_le(&node_buf[0]),
                    unpack_u32_le(&node_buf[4]),
                    unpack_u32_le(&node_buf[8]),
                    unpack_u32_le(&node_buf[12]),

                    f_xmin,
                    f_ymin,
                    f_xmax,
                    f_ymax,

                    xmin,
                    ymin,
                    xmax,
                    ymax,

                    passes
                        ? "PASS"
                        : "CULL"
                );


                diag->nodes_logged++;
            }
        }


        if (passes) {
            /*
             * DATA node v1:
             *
             *     +0x14
             *
             * Указывает на MLP geometry body.
             */
            uint32_t v1 =
                unpack_u32_le(&node_buf[20]);


            if (v1 > 0) {
                parse_geometry_mlp(
                    mlp_fs,
                    v1,
                    cam,
                    vp,
                    gfx,
                    is_polygon_layer,
                    diag
                );
            }
        }


        return;
    }


    /* ---------------------------------------------------------------------- */
    /* NAVIGATION NODE                                                        */
    /* ---------------------------------------------------------------------- */

    if (diag != NULL) {
        diag->nav_visited++;
    }


    /*
     * NAV node:
     *
     *     +0x00 v3_jump
     *     +0x04 xmin
     *     +0x08 ymin
     *     +0x0C xmax
     *     +0x10 ymax
     *     +0x14 nav_level
     *     +0x18 obj_count
     */
    uint32_t v3_jump =
        unpack_u32_le(&node_buf[0]);


    float f_c_xmin =
        unpack_float_le(&node_buf[4]);

    float f_c_ymin =
        unpack_float_le(&node_buf[8]);

    float f_c_xmax =
        unpack_float_le(&node_buf[12]);

    float f_c_ymax =
        unpack_float_le(&node_buf[16]);


    int32_t c_xmin =
        (int32_t)(
            f_c_xmin * 10000000.0f
        );

    int32_t c_ymin =
        (int32_t)(
            f_c_ymin * 10000000.0f
        );

    int32_t c_xmax =
        (int32_t)(
            f_c_xmax * 10000000.0f
        );

    int32_t c_ymax =
        (int32_t)(
            f_c_ymax * 10000000.0f
        );


    if (
        diag != NULL &&
        diag->nodes_logged < 10
    ) {
        PURRGO_LOG(
            "MAP: NAV "
            "raw=(%08x,%08x,%08x,%08x) "
            "flt=(%f,%f,%f,%f) "
            "int=(%d,%d,%d,%d)\n",

            unpack_u32_le(&node_buf[4]),
            unpack_u32_le(&node_buf[8]),
            unpack_u32_le(&node_buf[12]),
            unpack_u32_le(&node_buf[16]),

            f_c_xmin,
            f_c_ymin,
            f_c_xmax,
            f_c_ymax,

            c_xmin,
            c_ymin,
            c_xmax,
            c_ymax
        );


        diag->nodes_logged++;
    }


    uint32_t nav_level =
        unpack_u32_le(&node_buf[20]);


    uint32_t obj_count =
        unpack_u32_le(&node_buf[24]);


    /*
     * Если BBox NAV node не пересекается с camera,
     * можно пропустить всё его поддерево.
     *
     * v3_jump содержит размер перехода с учётом уже
     * считанных 8 байт node header.
     *
     * Поэтому используется:
     *
     *     v3_jump - 8
     *
     * Это существующая семантика текущего parser.
     */
    if (
        c_xmax < cam->min_x ||
        c_xmin > cam->max_x ||
        c_ymax < cam->min_y ||
        c_ymin > cam->max_y
    ) {
        if (v3_jump >= 8) {
            uint32_t jump_amount =
                v3_jump - 8;


            if (
                idx_fs->seek(
                    idx_fs->handle,
                    *current_idx_offset +
                    jump_amount
                )
            ) {
                *current_idx_offset +=
                    jump_amount;
            }
        }


        return;
    }


    /*
     * Если nav_level > 0, непосредственные дети являются
     * Navigation Nodes.
     *
     * Если nav_level == 0, непосредственные дети являются
     * Data Nodes.
     */
    bool child_is_nav =
        (nav_level > 0);


    uint32_t child_level =
        (nav_level > 0)
            ? (nav_level - 1)
            : 0;


    for (
        uint32_t i = 0;
        i < obj_count;
        i++
    ) {
        parse_node(
            idx_fs,
            current_idx_offset,
            mlp_fs,
            child_is_nav,
            child_level,
            cam,
            vp,
            gfx,
            is_polygon_layer,
            diag
        );
    }
}


/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

void purrgo_map_render_layer(
    purrgo_fs_t *idx_fs,
    purrgo_fs_t *mlp_fs,
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *viewport,
    bool is_polygon_layer
) {
    if (
        idx_fs == NULL ||
        mlp_fs == NULL ||
        gfx == NULL ||
        camera == NULL ||
        viewport == NULL
    ) {
        return;
    }


    PURRGO_LOG(
        "MAP: IDX opened\n"
    );


    PURRGO_LOG(
        "MAP: CAMERA "
        "min=(%ld,%ld) "
        "max=(%ld,%ld)\n",

        (long)camera->min_x,
        (long)camera->min_y,
        (long)camera->max_x,
        (long)camera->max_y
    );


    PURRGO_LOG(
        "MAP: VIEWPORT "
        "offset=(%d,%d) "
        "size=(%u,%u)\n",

        (int)viewport->offset_x,
        (int)viewport->offset_y,
        (unsigned)viewport->width,
        (unsigned)viewport->height
    );


    PURRGO_LOG(
        "MAP: GEOMETRY TYPE=%s\n",
        is_polygon_layer
            ? "POLYGON"
            : "LINE"
    );


    map_diag_t diag = {0};


    uint32_t current_idx_offset = 0;


    /*
     * YZL header занимает первые 32 байта IDX.
     */
    uint8_t yzl_header[32];


    if (
        idx_fs->read(
            idx_fs->handle,
            yzl_header,
            sizeof(yzl_header)
        ) != sizeof(yzl_header)
    ) {
        PURRGO_LOG(
            "MAP: ERROR reading YZL header\n"
        );

        return;
    }


    current_idx_offset += 32;


    /*
     * Проверяем magic.
     */
    if (
        yzl_header[0] != 'Y' ||
        yzl_header[1] != 'Z' ||
        yzl_header[2] != 'L'
    ) {
        PURRGO_LOG(
            "MAP: ERROR invalid YZL header\n"
        );

        return;
    }


    /*
     * Последовательно читаем SQT blocks
     * до EOF или invalid block.
     */
    while (true) {
        uint8_t sqt_header[16];


        if (
            idx_fs->read(
                idx_fs->handle,
                sqt_header,
                sizeof(sqt_header)
            ) != sizeof(sqt_header)
        ) {
            break;
        }


        current_idx_offset += 16;


        /*
         * SQT header:
         *
         *     0x00 'S'
         *     0x01 'Q'
         *     0x02 'T'
         *     0x03 0x01
         */
        if (
            sqt_header[0] != 'S' ||
            sqt_header[1] != 'Q' ||
            sqt_header[2] != 'T' ||
            sqt_header[3] != 0x01
        ) {
            break;
        }


        diag.sqt_blocks++;


        /*
         * SQT:
         *
         *     +0x08 mode
         *     +0x0C count
         */
        uint32_t mode =
            unpack_u32_le(
                &sqt_header[8]
            );


        uint32_t count =
            unpack_u32_le(
                &sqt_header[12]
            );


        if (count == 0) {
            continue;
        }


        /*
         * Root node type:
         *
         *     mode > 0 -> Navigation Node
         *     mode == 0 -> Data Node
         */
        bool is_nav =
            (mode > 0);


        uint32_t level =
            (mode > 0)
                ? (mode - 1)
                : 0;


        for (
            uint32_t i = 0;
            i < count;
            i++
        ) {
            parse_node(
                idx_fs,
                &current_idx_offset,
                mlp_fs,
                is_nav,
                level,
                camera,
                viewport,
                gfx,
                is_polygon_layer,
                &diag
            );
        }
    }


    PURRGO_LOG(
        "MAP: "
        "SQT=%u "
        "NAV=%u "
        "DATA=%u "
        "PASS=%u "
        "CULL=%u "
        "LINES=%u "
        "POLYGONS=%u "
        "SKIPPED=%u\n",

        diag.sqt_blocks,
        diag.nav_visited,
        diag.data_visited,
        diag.data_passed,
        diag.data_culled,
        diag.lines_drawn,
        diag.polygons_filled,
        diag.polygons_skipped
    );
}
