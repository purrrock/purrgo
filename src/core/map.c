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
 * Защитный предел реализации renderer.
 *
 * Это НЕ ограничение бинарного формата MLP.
 * MLP использует uint32/int32 для количества parts/points.
 *
 * Ограничение необходимо потому, что polygon renderer требует
 * одновременно хранить все projected points geometry.
 */
#define PURRGO_MAP_MAX_POINTS 512

/*
 * Размер chunk при чтении MLP point array.
 *
 * Одна MLP point занимает:
 *
 *     int32 X + int32 Y = 8 bytes
 *
 * Поэтому 64-byte chunk содержит максимум 8 точек.
 *
 * Chunked reading позволяет не делать отдельный filesystem read()
 * для каждой точки и при этом не требует большого временного буфера.
 */
#define PURRGO_MAP_READ_CHUNK_SIZE 64

/*
 * Количество точек, помещающихся в один MLP read chunk.
 */
#define PURRGO_MAP_POINTS_PER_CHUNK \
    (PURRGO_MAP_READ_CHUNK_SIZE / 8)


/*
 * Статический буфер для polygon rendering.
 *
 * malloc/free здесь намеренно не используется.
 */
static gfx_point_t s_polygon_buffer[PURRGO_MAP_MAX_POINTS];


/*
 * Диагностические счётчики map subsystem.
 *
 * Поток обработки:
 *
 *     IDX
 *       |
 *       v
 *     SQT
 *       |
 *       v
 *     NAV
 *       |
 *       v
 *     DATA
 *       |
 *       v
 *     AABB culling
 *       |
 *       v
 *     MLP
 *       |
 *       v
 *     projection
 *       |
 *       v
 *     GFX
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
 *
 * Нельзя напрямую делать cast uint8_t* -> int32_t*:
 *
 * - возможны проблемы с alignment;
 * - архитектура может иметь другой byte order.
 *
 * Поэтому поле собирается побайтно.
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


/* -------------------------------------------------------------------------- */
/* AABB                                                                      */
/* -------------------------------------------------------------------------- */

/*
 * Проверка пересечения BBox geometry/node с камерой.
 *
 * Координаты PurrGO:
 *
 *     degrees * 10^7
 *
 * Поддерживается переход через antimeridian.
 */
static inline bool bbox_intersects_camera(
    int32_t xmin,
    int32_t ymin,
    int32_t xmax,
    int32_t ymax,
    const purrgo_bbox_t *cam
) {
    /*
     * Сначала проверяем latitude.
     */
    if (ymax < cam->min_y || ymin > cam->max_y) {
        return false;
    }

    /*
     * Обычный BBox:
     *
     *     min_x <= max_x
     */
    if (cam->min_x <= cam->max_x) {
        return (
            xmax >= cam->min_x &&
            xmin <= cam->max_x
        );
    }

    /*
     * Camera BBox пересекает antimeridian.
     *
     * Например:
     *
     *     min_x = +179°
     *     max_x = -179°
     *
     * Видимая область состоит из двух частей:
     *
     *     [+179°, +180°]
     *     [-180°, -179°]
     */
    return (
        xmin <= cam->max_x ||
        xmax >= cam->min_x
    );
}


/* -------------------------------------------------------------------------- */
/* Projection                                                                 */
/* -------------------------------------------------------------------------- */

/*
 * Размер camera longitude span.
 *
 * Для обычной камеры:
 *
 *     max_x - min_x
 *
 * Для camera BBox через antimeridian:
 *
 *     (max_x + 360°) - min_x
 *
 * Все координаты находятся в формате degrees * 10^7.
 */
static inline int64_t camera_span_x(
    const purrgo_bbox_t *cam
) {
    if (cam->min_x <= cam->max_x) {
        return (
            (int64_t)cam->max_x -
            (int64_t)cam->min_x
        );
    }

    return (
        (int64_t)cam->max_x +
        3600000000LL
    ) - (int64_t)cam->min_x;
}


/*
 * Преобразование координат карты в framebuffer coordinates.
 *
 * Формат координат V2:
 *
 *     longitude = int32 / 10^7
 *     latitude  = int32 / 10^7
 *
 * Внутри PurrGO используется тот же integer representation:
 *
 *     degrees * 10^7
 *
 * Вся промежуточная арифметика выполняется в int64_t.
 */
static void project_to_screen(
    int32_t lon,
    int32_t lat,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    int16_t *sx,
    int16_t *sy
) {
    /*
     * Longitude displacement относительно левой границы камеры.
     *
     * Приведение к int64_t происходит ДО вычитания.
     */
    int64_t dx_raw =
        (int64_t)lon -
        (int64_t)cam->min_x;


    /*
     * Нормализация longitude при переходе через antimeridian.
     */
    if (
        cam->min_x > cam->max_x &&
        dx_raw < 0
    ) {
        dx_raw += 3600000000LL;
    }


    /*
     * Масштабирование longitude в pixel coordinates.
     */
    int64_t dx =
        dx_raw *
        (int64_t)vp->width;


    int64_t width =
        camera_span_x(cam);


    int64_t projected_x =
        (width > 0)
            ? (dx / width)
            : 0;


    projected_x +=
        (int64_t)vp->offset_x;


    /*
     * Latitude.
     *
     * framebuffer Y направлен вниз.
     *
     * Поэтому:
     *
     *     min_y -> нижняя часть viewport
     *     max_y -> верхняя часть viewport
     */
    int64_t dy =
        (
            (int64_t)lat -
            (int64_t)cam->min_y
        ) *
        (int64_t)vp->height;


    int64_t height =
        (int64_t)cam->max_y -
        (int64_t)cam->min_y;


    int64_t projected_y =
        (height > 0)
            ? (
                (int64_t)vp->height -
                (dy / height)
            )
            : 0;


    projected_y +=
        (int64_t)vp->offset_y;


    /*
     * gfx_point_t использует int16_t.
     *
     * Clamping выполняется после всей арифметики.
     */
    if (projected_x < -32768) {
        projected_x = -32768;
    }

    if (projected_x > 32767) {
        projected_x = 32767;
    }

    if (projected_y < -32768) {
        projected_y = -32768;
    }

    if (projected_y > 32767) {
        projected_y = 32767;
    }


    *sx = (int16_t)projected_x;
    *sy = (int16_t)projected_y;
}


/* -------------------------------------------------------------------------- */
/* Polygon helpers                                                            */
/* -------------------------------------------------------------------------- */

/*
 * Проверка массива parts[].
 *
 * MLP V2:
 *
 *     parts[0] = start point первого part/ring
 *     parts[1] = start point второго part/ring
 *     ...
 *
 * Конечный индекс последнего part отдельно не хранится.
 *
 * Для последнего part:
 *
 *     end = num_points
 */
static bool validate_parts(
    const uint32_t *parts,
    uint32_t num_parts,
    uint32_t num_points
) {
    /*
     * Для line geometry parts могут отсутствовать.
     */
    if (num_parts == 0) {
        return true;
    }


    if (
        parts == NULL ||
        num_points == 0
    ) {
        return false;
    }


    /*
     * Первый part обязан начинаться с первой точки.
     */
    if (parts[0] != 0) {
        return false;
    }


    for (
        uint32_t i = 0;
        i < num_parts;
        i++
    ) {
        /*
         * Start index должен указывать
         * на существующую точку.
         */
        if (parts[i] >= num_points) {
            return false;
        }


        /*
         * Start indices должны строго возрастать.
         */
        if (
            i > 0 &&
            parts[i] <= parts[i - 1]
        ) {
            return false;
        }
    }


    return true;
}


/* -------------------------------------------------------------------------- */
/* MLP geometry                                                               */
/* -------------------------------------------------------------------------- */

/*
 * Чтение и rendering одной MLP geometry.
 *
 * MLP V2 geometry body:
 *
 *     +0x00 int32 minx
 *     +0x04 int32 miny
 *     +0x08 int32 maxx
 *     +0x0C int32 maxy
 *     +0x10 int32 num_parts
 *     +0x14 int32 num_points
 *
 *     parts[num_parts]      uint32
 *     points[num_points]    int32 X + int32 Y
 *
 * Все координаты:
 *
 *     degrees * 10^7
 *
 * Важный контракт IDX:
 *
 *     v1 -> непосредственно начало geometry body
 *
 * Поэтому local 8-byte MLP header пропускается самим v1.
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
     * YZL global header:
     *
     *     32 bytes
     *
     * v1_offset является offset относительно конца
     * этого header.
     */
    uint32_t absolute_body_offset =
        32u + v1_offset;


    if (
        !mlp_fs->seek(
            mlp_fs->handle,
            absolute_body_offset
        )
    ) {
        return;
    }


    /*
     * Fixed MLP geometry body header:
     *
     *     4 * int32 BBox = 16 bytes
     *     int32 num_parts = 4 bytes
     *     int32 num_points = 4 bytes
     *
     * Всего 24 bytes.
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


    /*
     * BBox geometry пока не используется отдельно:
     * DATA node уже выполнил AABB culling.
     *
     * Читаем только структурные поля, необходимые parser.
     */
    int32_t num_parts =
        unpack_i32_le(&head[16]);


    int32_t num_points =
        unpack_i32_le(&head[20]);


    /*
     * Защитные проверки реализации.
     *
     * Это не ограничения бинарного формата.
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


        if (
            is_polygon_layer &&
            diag != NULL
        ) {
            diag->polygons_skipped++;
        }


        return;
    }


    /* ---------------------------------------------------------------------- */
    /* parts[]                                                                 */
    /* ---------------------------------------------------------------------- */

    uint32_t parts[PURRGO_MAP_MAX_PARTS] = {0};


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
            if (
                is_polygon_layer &&
                diag != NULL
            ) {
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


        if (
            is_polygon_layer &&
            diag != NULL
        ) {
            diag->polygons_skipped++;
        }


        return;
    }


    /*
     * Polygon renderer требует весь массив projected points.
     *
     * Line renderer работает потоково и буфер ему не нужен.
     */
    gfx_point_t *screen_points =
        is_polygon_layer
            ? s_polygon_buffer
            : NULL;


    /* ---------------------------------------------------------------------- */
    /* Point array                                                             */
    /* ---------------------------------------------------------------------- */

    int16_t prev_sx = 0;
    int16_t prev_sy = 0;


    uint32_t current_part_idx = 0;


    /*
     * Первый part начинается с point 0.
     *
     * Если parts > 1, parts[1] является первой точкой
     * следующего part.
     */
    uint32_t next_part_start =
        (num_parts > 1)
            ? parts[1]
            : (uint32_t)num_points;


    /*
     * Chunk buffer:
     *
     *     64 bytes
     *
     *     8 points * 8 bytes
     */
    uint8_t chunk_buf[
        PURRGO_MAP_READ_CHUNK_SIZE
    ];


    uint32_t points_read = 0;


    while (
        points_read <
        (uint32_t)num_points
    ) {
        uint32_t points_left =
            (uint32_t)num_points -
            points_read;


        uint32_t points_in_chunk =
            (
                points_left >
                PURRGO_MAP_POINTS_PER_CHUNK
            )
                ? PURRGO_MAP_POINTS_PER_CHUNK
                : points_left;


        uint32_t bytes_to_read =
            points_in_chunk * 8u;


        /*
         * Читаем chunk целиком.
         *
         * Для последнего chunk размер может быть меньше 64 bytes.
         */
        if (
            mlp_fs->read(
                mlp_fs->handle,
                chunk_buf,
                bytes_to_read
            ) != bytes_to_read
        ) {
            /*
             * При ошибке чтения нельзя считать polygon
             * полностью полученным.
             */
            if (
                is_polygon_layer &&
                diag != NULL
            ) {
                diag->polygons_skipped++;
            }


            return;
        }


        for (
            uint32_t j = 0;
            j < points_in_chunk;
            j++
        ) {
            uint32_t point_index =
                points_read + j;


            uint8_t *point_buf =
                &chunk_buf[j * 8u];


            /*
             * MLP V2:
             *
             *     X = longitude * 10^7
             *     Y = latitude  * 10^7
             *
             * Значения уже совпадают с внутренним
             * PurrGO coordinate representation.
             */
            int32_t raw_x =
                unpack_i32_le(
                    &point_buf[0]
                );


            int32_t raw_y =
                unpack_i32_le(
                    &point_buf[4]
                );


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


            /*
             * Polygon renderer сохраняет все точки.
             */
            if (is_polygon_layer) {
                screen_points[point_index].x = sx;
                screen_points[point_index].y = sy;
            }


            /*
             * Если текущая точка является началом следующего
             * part, переключаем current_part_idx ДО line drawing.
             */
            if (
                num_parts > 0 &&
                point_index == next_part_start
            ) {
                current_part_idx++;


                next_part_start =
                    (
                        current_part_idx + 1 <
                        (uint32_t)num_parts
                    )
                        ? parts[
                            current_part_idx + 1
                        ]
                        : (uint32_t)num_points;
            }


            /*
             * Line geometry рендерится потоково.
             *
             * Первая точка каждого part не соединяется
             * с последней точкой предыдущего part.
             */
            if (!is_polygon_layer) {
                uint32_t current_part_offset =
                    (num_parts > 0)
                        ? parts[current_part_idx]
                        : 0;


                if (
                    point_index >
                    current_part_offset
                ) {
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


        points_read += points_in_chunk;
    }


    /* ---------------------------------------------------------------------- */
    /* Polygon rendering                                                      */
    /* ---------------------------------------------------------------------- */

    if (is_polygon_layer) {
        uint32_t part_count =
            (uint32_t)num_parts;


        /*
         * Polygon geometry должна иметь хотя бы один part.
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
         * Проверка API gfx_fill_compound_polygon().
         */
        if (
            num_points > UINT16_MAX ||
            part_count > UINT16_MAX
        ) {
            PURRGO_LOG(
                "MAP: polygon geometry exceeds "
                "uint16_t limit "
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
         * gfx_fill_compound_polygon() использует even-odd rule,
         * поэтому внутренние rings могут образовывать holes.
         */
        gfx_fill_compound_polygon(
            gfx,
            screen_points,
            (uint16_t)num_points,
            parts,
            (uint16_t)part_count
        );


        if (diag != NULL) {
            diag->polygons_filled++;
        }
    }
}


/* -------------------------------------------------------------------------- */
/* IDX node parser                                                            */
/* -------------------------------------------------------------------------- */

/*
 * Рекурсивный обход IDX SQT tree.
 *
 * IDX V2 использует единый 28-byte node record.
 *
 * DATA node:
 *
 *     +0x00 int32  xmin
 *     +0x04 int32  ymin
 *     +0x08 int32  xmax
 *     +0x0C int32  ymax
 *     +0x10 uint32 type
 *     +0x14 uint32 v1
 *     +0x18 uint32 v2
 *
 * NAV node:
 *
 *     +0x00 uint32 v3_jump
 *     +0x04 int32  xmin
 *     +0x08 int32  ymin
 *     +0x0C int32  xmax
 *     +0x10 int32  ymax
 *     +0x14 uint32 level
 *     +0x18 uint32 child_count
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
         * DATA node BBox.
         *
         * Сначала проверяем Y.
         * Это позволяет не выполнять X comparison,
         * если node заведомо находится вне camera.
         */
        int32_t ymin =
            unpack_i32_le(
                &node_buf[4]
            );


        int32_t ymax =
            unpack_i32_le(
                &node_buf[12]
            );


        bool passes = false;


        int32_t xmin = 0;
        int32_t xmax = 0;


        if (
            !(ymax < cam->min_y ||
              ymin > cam->max_y)
        ) {
            xmin =
                unpack_i32_le(
                    &node_buf[0]
                );


            xmax =
                unpack_i32_le(
                    &node_buf[8]
                );


            passes =
                bbox_intersects_camera(
                    xmin,
                    ymin,
                    xmax,
                    ymax,
                    cam
                );
        }


        if (diag != NULL) {
            if (passes) {
                diag->data_passed++;
            } else {
                diag->data_culled++;
            }


            if (diag->nodes_logged < 10) {
                /*
                 * Для диагностического сообщения X должен быть
                 * известен даже если DATA был отброшен по Y.
                 */
                if (
                    ymax < cam->min_y ||
                    ymin > cam->max_y
                ) {
                    xmin =
                        unpack_i32_le(
                            &node_buf[0]
                        );


                    xmax =
                        unpack_i32_le(
                            &node_buf[8]
                        );
                }


                PURRGO_LOG(
                    "MAP: DATA "
                    "raw=(%08x,%08x,%08x,%08x) "
                    "int=(%d,%d,%d,%d) %s\n",

                    unpack_u32_le(
                        &node_buf[0]
                    ),

                    unpack_u32_le(
                        &node_buf[4]
                    ),

                    unpack_u32_le(
                        &node_buf[8]
                    ),

                    unpack_u32_le(
                        &node_buf[12]
                    ),

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
             * DATA node:
             *
             *     v1 @ +0x14
             *
             * В V2 v1 указывает непосредственно
             * на MLP geometry body.
             */
            uint32_t v1 =
                unpack_u32_le(
                    &node_buf[20]
                );


            /*
             * v1 == 0 означает отсутствие MLP geometry.
             *
             * В частности, это может быть DATA node,
             * для которого geometry не представлена в MLP.
             *
             * POI сейчас намеренно не обрабатываем.
             */
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
     * v3_jump используется для аппаратного/потокового
     * пропуска subtree.
     */
    uint32_t v3_jump =
        unpack_u32_le(
            &node_buf[0]
        );


    int32_t c_ymin =
        unpack_i32_le(
            &node_buf[8]
        );


    int32_t c_ymax =
        unpack_i32_le(
            &node_buf[16]
        );


    bool passes = false;


    int32_t c_xmin = 0;
    int32_t c_xmax = 0;


    if (
        !(c_ymax < cam->min_y ||
          c_ymin > cam->max_y)
    ) {
        c_xmin =
            unpack_i32_le(
                &node_buf[4]
            );


        c_xmax =
            unpack_i32_le(
                &node_buf[12]
            );


        passes =
            bbox_intersects_camera(
                c_xmin,
                c_ymin,
                c_xmax,
                c_ymax,
                cam
            );
    }


    if (
        diag != NULL &&
        diag->nodes_logged < 10
    ) {
        if (
            c_ymax < cam->min_y ||
            c_ymin > cam->max_y
        ) {
            c_xmin =
                unpack_i32_le(
                    &node_buf[4]
                );


            c_xmax =
                unpack_i32_le(
                    &node_buf[12]
                );
        }


        PURRGO_LOG(
            "MAP: NAV "
            "raw=(%08x,%08x,%08x,%08x) "
            "int=(%d,%d,%d,%d)\n",

            unpack_u32_le(
                &node_buf[4]
            ),

            unpack_u32_le(
                &node_buf[8]
            ),

            unpack_u32_le(
                &node_buf[12]
            ),

            unpack_u32_le(
                &node_buf[16]
            ),

            c_xmin,
            c_ymin,
            c_xmax,
            c_ymax
        );


        diag->nodes_logged++;
    }


    uint32_t nav_level =
        unpack_u32_le(
            &node_buf[20]
        );


    uint32_t obj_count =
        unpack_u32_le(
            &node_buf[24]
        );


    /*
     * Если NAV BBox не пересекается с camera,
     * всё дочернее subtree можно пропустить.
     *
     * К этому моменту 28-byte NAV node уже прочитан.
     *
     * Согласно формату v3_jump содержит размер перехода
     * с учётом уже прочитанных 8 bytes.
     *
     * Поэтому:
     *
     *     remaining_jump = v3_jump - 8
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
     * NAV level определяет тип непосредственных children:
     *
     *     level > 0 -> NAV children
     *     level == 0 -> DATA children
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
    /*
     * На текущем этапе renderer работает только с IDX + MLP.
     *
     * .db и POI здесь намеренно отсутствуют.
     */
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


    /* ---------------------------------------------------------------------- */
    /* YZL header                                                              */
    /* ---------------------------------------------------------------------- */

    /*
     * Global YZL header:
     *
     *     32 bytes
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
     * YZL magic.
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


    /* ---------------------------------------------------------------------- */
    /* SQT sections                                                            */
    /* ---------------------------------------------------------------------- */

    /*
     * Читаем SQT sections последовательно.
     *
     * На текущем этапе, как и в старой реализации,
     * используется только первая SQT section.
     *
     * Это сознательное ограничение LOD/Z-culling и не относится
     * к изменению coordinate representation V1 -> V2.
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
         * SQT magic:
         *
         *     "SQT\x01"
         */
        if (
            sqt_header[0] != 'S' ||
            sqt_header[1] != 'Q' ||
            sqt_header[2] != 'T' ||
            sqt_header[3] != 0x01
        ) {
            PURRGO_LOG(
                "MAP: ERROR invalid SQT header\n"
            );


            break;
        }


        /*
         * Пока используем только первую SQT section.
         *
         * Это сохраняет прежнюю стратегию LOD.
         */
        if (diag.sqt_blocks > 0) {
            break;
        }


        diag.sqt_blocks++;


        /*
         * SQT header:
         *
         *     +0x00 magic
         *     +0x04 topology marker
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
         *     mode > 0  -> NAV
         *     mode == 0 -> DATA
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


    /* ---------------------------------------------------------------------- */
    /* Diagnostics                                                             */
    /* ---------------------------------------------------------------------- */

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

        (unsigned)diag.sqt_blocks,
        (unsigned)diag.nav_visited,
        (unsigned)diag.data_visited,
        (unsigned)diag.data_passed,
        (unsigned)diag.data_culled,
        (unsigned)diag.lines_drawn,
        (unsigned)diag.polygons_filled,
        (unsigned)diag.polygons_skipped
    );
}