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
 * Статический буфер для рендеринга полигонов.
 *
 * Использование статического буфера позволяет избежать malloc/free
 * и делает поведение предсказуемым для STM32.
 */
static gfx_point_t s_polygon_buffer[PURRGO_MAP_MAX_POINTS];


/*
 * Диагностические счётчики map subsystem.
 *
 * Поток обработки:
 *
 *     IDX -> SQT -> NAV -> DATA -> AABB -> MLP -> projection -> GFX
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
 * AABB Intersection with Antimeridian support
 */
static inline bool bbox_intersects_camera(
    int32_t xmin, int32_t ymin, int32_t xmax, int32_t ymax,
    const purrgo_bbox_t *cam
) {
    if (ymax < cam->min_y || ymin > cam->max_y) {
        return false;
    }

    if (cam->min_x <= cam->max_x) {
        // Ordinary camera
        return xmax >= cam->min_x && xmin <= cam->max_x;
    } else {
        // Antimeridian crossing
        // Bbox must intersect [-1.8e9, cam->max_x] OR [cam->min_x, +1.8e9]
        return xmin <= cam->max_x || xmax >= cam->min_x;
    }
}

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

/*
 * Безопасное преобразование float BBox координат в int32_t.
 * Применяется запас в 256 единиц (2.5-5 градуса) для компенсации
 * потери точности мантиссы IEEE-754 24-bit при 180 градусах
 * и усечения к нулю при кастинге float -> int.
 * Также добавлена защита от UB при конвертации очень больших (запредельных) float в int32_t.
 */
static inline int32_t float_bbox_min(float f_val) {
    if (f_val <= -214.7483f) return -2147483647;
    if (f_val >=  214.7483f) return  2147483647 - 256;
    return (int32_t)(f_val * 10000000.0f) - 256;
}

static inline int32_t float_bbox_max(float f_val) {
    if (f_val <= -214.7483f) return -2147483647 + 256;
    if (f_val >=  214.7483f) return  2147483647;
    return (int32_t)(f_val * 10000000.0f) + 256;
}


/* -------------------------------------------------------------------------- */
/* Projection                                                                 */
/* -------------------------------------------------------------------------- */

static inline int64_t camera_span_x(const purrgo_bbox_t *cam) {
    if (cam->min_x <= cam->max_x) {
        return (int64_t)cam->max_x - (int64_t)cam->min_x;
    } else {
        return ((int64_t)cam->max_x + 3600000000LL) - (int64_t)cam->min_x;
    }
}

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
 * Для промежуточного умножения и вычитания используется int64_t.
 *
 * Важно:
 * приведение к int64_t выполняется ДО вычитания координат.
 * Иначе выражение вида
 *
 *     lon - cam->min_x
 *
 * сначала вычислялось бы в int32_t и могло бы переполниться.
 */
static void project_to_screen(
    int32_t lon,
    int32_t lat,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    int16_t *sx,
    int16_t *sy
) {
    int64_t dx_raw = (int64_t)lon - (int64_t)cam->min_x;

    // Normalize wrapped longitude difference for antimeridian crossing
    if (cam->min_x > cam->max_x && dx_raw < 0) {
        dx_raw += 3600000000LL;
    }

    int64_t dx = dx_raw * (int64_t)vp->width;

    int64_t w = camera_span_x(cam);

    int64_t projected_x =
        (w > 0)
            ? (dx / w)
            : 0;

    projected_x +=
        (int64_t)vp->offset_x;


    int64_t dy =
        (
            (int64_t)lat -
            (int64_t)cam->min_y
        ) *
        (int64_t)vp->height;

    int64_t h =
        (int64_t)cam->max_y -
        (int64_t)cam->min_y;

    int64_t projected_y =
        (h > 0)
            ? (
                (int64_t)vp->height -
                (dy / h)
            )
            : 0;

    projected_y +=
        (int64_t)vp->offset_y;


    /*
     * gfx_point_t использует int16_t.
     *
     * Camera/viewport в нормальном режиме проекта дают координаты,
     * помещающиеся в int16_t. Здесь преобразование выполняется
     * только после всей арифметики в int64_t.
     * Добавлен clamping для защиты от переполнения (заворачивания координат)
     * при отрисовке очень длинных полилиний или сильно приближенной камеры.
     */
    if (projected_x < -32768) projected_x = -32768;
    if (projected_x >  32767) projected_x =  32767;
    if (projected_y < -32768) projected_y = -32768;
    if (projected_y >  32767) projected_y =  32767;

    *sx = (int16_t)projected_x;
    *sy = (int16_t)projected_y;
}


/* -------------------------------------------------------------------------- */
/* Polygon helpers                                                            */
/* -------------------------------------------------------------------------- */

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
 *
 * Важно:
 *
 *     parts[] не содержит конечный индекс последнего ring.
 *
 * Поэтому конец каждого ring вычисляется как:
 *
 *     parts[i + 1]
 *
 * либо:
 *
 *     num_points
 *
 * для последнего ring.
 */
static bool validate_parts(
    const uint32_t *parts,
    uint32_t num_parts,
    uint32_t num_points
) {
    if (num_parts == 0) {
        /*
         * Geometry без parts допускается для line renderer.
         *
         * Polygon renderer отдельно требует хотя бы один part,
         * поскольку без него невозможно определить границы ring.
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
        /*
         * Каждый start index должен указывать на существующую
         * точку geometry.
         */
        if (parts[i] >= num_points) {
            return false;
        }

        /*
         * Ring boundaries должны идти строго по возрастанию.
         */
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
 *         +-- ring 0
 *         +-- ring 1
 *         +-- ...
 *
 * Все rings передаются одновременно в
 * gfx_fill_compound_polygon().
 *
 * Renderer использует even-odd правило, поэтому внутренние rings
 * автоматически образуют holes.
 *
 * Geometry читается полностью потому, что polygon renderer требует
 * массив projected points.
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
            32u + v1_offset
        )) {
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
     *
     * Всего 24 байта.
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

        if (is_polygon_layer && diag != NULL) {
            diag->polygons_skipped++;
        }

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
            if (is_polygon_layer && diag != NULL) {
                diag->polygons_skipped++;
            }

            return;
        }


        for (
            uint32_t i = 0;
            i < (uint32_t)num_parts;
            i++
        ) {
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

        if (is_polygon_layer && diag != NULL) {
            diag->polygons_skipped++;
        }

        return;
    }


    /*
     * Для polygon rendering используется общий статический буфер.
     *
     * num_points уже ограничен PURRGO_MAP_MAX_POINTS выше, поэтому
     * дополнительной проверки размера самого буфера здесь не требуется.
     */
    gfx_point_t *screen_points = NULL;

    if (is_polygon_layer) {
        screen_points = s_polygon_buffer;
    }


    /*
     * Для line rendering сохраняем потоковый подход:
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
            if (is_polygon_layer && diag != NULL) {
                diag->polygons_skipped++;
            }

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
         */
        int32_t raw_x =
            unpack_i32_le(&pt_buf[0]);

        int32_t raw_y =
            unpack_i32_le(&pt_buf[4]);


        /*
         * Для реальных координат MLP текущего диапазона
         * умножение на 10 помещается в int32_t.
         */
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
         * parts[] содержит start index каждого ring/part.
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
         * Для line layer продолжаем потоковый рендеринг.
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


    /* ---------------------------------------------------------------------- */
    /* Polygon rendering                                                      */
    /* ---------------------------------------------------------------------- */

    if (is_polygon_layer) {
        uint32_t part_count =
            (uint32_t)num_parts;


        /*
         * Для polygon geometry parts[] обязателен.
         *
         * Без parts невозможно определить границы rings,
         * поэтому compound polygon построить нельзя.
         */
        if (part_count == 0) {
            PURRGO_LOG(
                "MAP: polygon geometry has no parts "
                "points=%ld\n",
                (long)num_points
            );

            if (diag != NULL) {
                diag->polygons_skipped++;
            }

            return;
        }


        /*
         * Проверка контракта gfx_fill_compound_polygon().
         *
         * На текущем этапе выше уже существует более жёсткое
         * ограничение PURRGO_MAP_MAX_POINTS=2048 и
         * PURRGO_MAP_MAX_PARTS=32, но этот check оставляет
         * явную границу API.
         */
        if (
            num_points > UINT16_MAX ||
            part_count > UINT16_MAX
        ) {
            PURRGO_LOG(
                "MAP: polygon geometry exceeds uint16_t limit "
                "points=%ld parts=%ld\n",
                (long)num_points,
                (long)part_count
            );

            if (diag != NULL) {
                diag->polygons_skipped++;
            }

            return;
        }


        /*
         * Передаём все rings одновременно.
         *
         * gfx_fill_compound_polygon() использует even-odd rule:
         *
         *     outer ring + inner ring
         *
         * даёт заполненный outer и пустой inner.
         *
         * Направление обхода ring здесь намеренно не проверяется:
         * even-odd алгоритм не зависит от CW/CCW winding.
         */
        gfx_fill_compound_polygon(
            gfx,
            screen_points,
            (uint16_t)num_points,
            parts,
            (uint16_t)part_count
        );


        /*
         * Здесь geometry успешно прошла все проверки и была
         * передана renderer.
         *
         * Важно: gfx_fill_compound_polygon() имеет void API и не
         * сообщает, произошёл ли внутренний fail-fast из-за
         * GFX_MAX_POLYGON_NODES. Поэтому этот счётчик означает
         * "polygon accepted by map renderer", а не гарантированно
         * полностью отображённый polygon.
         */
        if (diag != NULL) {
            diag->polygons_filled++;
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
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    gfx_context_t *gfx,
    bool is_polygon_layer,
    map_diag_t *diag
) {
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
        float f_ymin =
            unpack_float_le(&node_buf[4]);

        float f_ymax =
            unpack_float_le(&node_buf[12]);

        /*
         * PurrGo internal coordinate representation:
         *
         *     degrees * 10^7
         */
        int32_t ymin = float_bbox_min(f_ymin);
        int32_t ymax = float_bbox_max(f_ymax);

        /*
         * AABB intersection (Y-axis fast cull).
         * Сначала проверяем Y, чтобы пропустить распаковку X.
         */
        bool passes = false;

        float f_xmin = 0.0f;
        float f_xmax = 0.0f;
        int32_t xmin = 0;
        int32_t xmax = 0;

        if (!(ymax < cam->min_y || ymin > cam->max_y)) {
            f_xmin = unpack_float_le(&node_buf[0]);
            f_xmax = unpack_float_le(&node_buf[8]);

            xmin = float_bbox_min(f_xmin);
            xmax = float_bbox_max(f_xmax);

            if (cam->min_x <= cam->max_x) {
                passes = (xmax >= cam->min_x && xmin <= cam->max_x);
            } else {
                passes = (xmin <= cam->max_x || xmax >= cam->min_x);
            }
        }

        if (diag != NULL) {
            if (passes) {
                diag->data_passed++;
            } else {
                diag->data_culled++;
            }

            if (diag->nodes_logged < 10) {
                /* Распаковываем X только для логирования, если они еще не распакованы */
                if (ymax < cam->min_y || ymin > cam->max_y) {
                    f_xmin = unpack_float_le(&node_buf[0]);
                    f_xmax = unpack_float_le(&node_buf[8]);

                    xmin = float_bbox_min(f_xmin);
                    xmax = float_bbox_max(f_xmax);
                }

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


    float f_c_ymin =
        unpack_float_le(&node_buf[8]);

    float f_c_ymax =
        unpack_float_le(&node_buf[16]);

    int32_t c_ymin = float_bbox_min(f_c_ymin);
    int32_t c_ymax = float_bbox_max(f_c_ymax);

    bool passes = false;

    float f_c_xmin = 0.0f;
    float f_c_xmax = 0.0f;
    int32_t c_xmin = 0;
    int32_t c_xmax = 0;

    if (!(c_ymax < cam->min_y || c_ymin > cam->max_y)) {
        f_c_xmin = unpack_float_le(&node_buf[4]);
        f_c_xmax = unpack_float_le(&node_buf[12]);

        c_xmin = float_bbox_min(f_c_xmin);
        c_xmax = float_bbox_max(f_c_xmax);

        if (cam->min_x <= cam->max_x) {
            passes = (c_xmax >= cam->min_x && c_xmin <= cam->max_x);
        } else {
            passes = (c_xmin <= cam->max_x || c_xmax >= cam->min_x);
        }
    }


    if (
        diag != NULL &&
        diag->nodes_logged < 10
    ) {
        if (c_ymax < cam->min_y || c_ymin > cam->max_y) {
            f_c_xmin = unpack_float_le(&node_buf[4]);
            f_c_xmax = unpack_float_le(&node_buf[12]);

            c_xmin = float_bbox_min(f_c_xmin);
            c_xmax = float_bbox_max(f_c_xmax);
        }

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
     */
    if (!passes) {
        if (v3_jump > 8) {
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


        // Временный отказ от LOD (Z-Culling).
        // Читаем только первую SQT секцию (LOD 0).
        if (diag.sqt_blocks > 0) {
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