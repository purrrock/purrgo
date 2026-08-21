// file: src/core/map.c
#include "purrgo/map.h"
#include "purrgo/logger.h"
#include <stdbool.h>
#include <stdint.h>

#define PURRGO_MAP_MAX_PARTS 32

/*
 * Диагностические счётчики map subsystem.
 *
 * Эти значения нужны только для отладки parser/render pipeline:
 *
 * IDX -> SQT -> NAV -> DATA -> AABB -> MLP -> projection -> GFX
 *
 * Они позволяют определить, на каком именно этапе исчезает геометрия.
 */
typedef struct {
    uint32_t sqt_blocks;
    uint32_t nav_visited;
    uint32_t data_visited;
    uint32_t data_passed;
    uint32_t data_culled;
    uint32_t lines_drawn;

    /*
     * Ограничивает количество подробных сообщений о DATA/NAV nodes.
     * Иначе один render карты может полностью забить консоль.
     */
    uint32_t nodes_logged;
} map_diag_t;

/* Безопасное чтение Little-Endian int32_t. */
static inline int32_t unpack_i32_le(const uint8_t* buf) {
    return (int32_t)(
        (uint32_t)buf[0] |
        ((uint32_t)buf[1] << 8) |
        ((uint32_t)buf[2] << 16) |
        ((uint32_t)buf[3] << 24)
    );
}

/* Безопасное чтение Little-Endian uint32_t. */
static inline uint32_t unpack_u32_le(const uint8_t* buf) {
    return
        (uint32_t)buf[0] |
        ((uint32_t)buf[1] << 8) |
        ((uint32_t)buf[2] << 16) |
        ((uint32_t)buf[3] << 24);
}

/*
 * Чтение IEEE-754 float из четырёх байт Little-Endian.
 *
 * Формат IDX по текущему parser-коду хранит координаты BBox как float.
 * Здесь выполняется только побитовое восстановление float; никакого
 * дополнительного преобразования формата нет.
 */
static inline float unpack_float_le(const uint8_t* buf) {
    union {
        uint32_t i;
        float f;
    } u;

    u.i = unpack_u32_le(buf);
    return u.f;
}

/*
 * Преобразование координат карты в координаты framebuffer.
 *
 * Внутренние координаты карты имеют масштаб 10^7:
 *
 * longitude = integer / 10^7
 * latitude  = integer / 10^7
 *
 * Camera задаёт прямоугольник карты, а viewport — прямоугольник
 * дисплея, куда этот прямоугольник должен быть спроецирован.
 *
 * Используется 64-битная промежуточная арифметика, чтобы умножение
 * координат на размеры viewport не выполнялось в 32-битном диапазоне.
 *
 * Ось Y framebuffer инвертируется:
 *
 * map min_y -> нижняя граница viewport
 * map max_y -> верхняя граница viewport
 */
static void project_to_screen(
    int32_t lon,
    int32_t lat,
    const purrgo_bbox_t* cam,
    const purrgo_viewport_t* vp,
    int16_t* sx,
    int16_t* sy
) {
    int64_t dx = (int64_t)(lon - cam->min_x) * vp->width;
    int64_t w = (int64_t)(cam->max_x - cam->min_x);

    *sx = (int16_t)(w > 0 ? (dx / w) : 0) + vp->offset_x;

    int64_t dy = (int64_t)(lat - cam->min_y) * vp->height;
    int64_t h = (int64_t)(cam->max_y - cam->min_y);

    *sy = (int16_t)(vp->height - (h > 0 ? (dy / h) : 0))
         + vp->offset_y;
}

/*
 * Потоковый парсинг геометрии MLP.
 *
 * Здесь уже нет пространственного поиска: решение о том, нужно ли
 * читать конкретный Data Node, принимается в parse_node() по его BBox.
 *
 * Для диагностики дополнительно выводится первая реально построенная
 * линия каждого render-вызова. Это позволяет проверить:
 *
 * MLP coordinates -> project_to_screen() -> screen coordinates
 *
 * и отделить проблему projection от проблемы framebuffer/SDL.
 */
#include "purrgo/gfx_line.h"

static void parse_geometry_mlp(
    purrgo_fs_t* mlp_fs,
    uint32_t v1_offset,
    const purrgo_bbox_t* cam,
    const purrgo_viewport_t* vp,
    gfx_context_t* gfx,
    map_diag_t* diag
) {
    if (!mlp_fs->seek(mlp_fs->handle, 32 + v1_offset)) {
        return;
    }

    uint8_t head[24];

    if (mlp_fs->read(mlp_fs->handle, head, 24) != 24) {
        return;
    }

    int32_t num_parts = unpack_i32_le(&head[16]);
    int32_t num_points = unpack_i32_le(&head[20]);

    if (num_parts < 0 ||
        num_parts > PURRGO_MAP_MAX_PARTS ||
        num_points <= 0) {
        return;
    }

    uint32_t parts[PURRGO_MAP_MAX_PARTS] = {0};

    if (num_parts > 0) {
        uint8_t part_buf[PURRGO_MAP_MAX_PARTS * 4];
        uint32_t bytes_to_read = (uint32_t)num_parts * 4;

        if (mlp_fs->read(
                mlp_fs->handle,
                part_buf,
                bytes_to_read) != bytes_to_read) {
            return;
        }

        for (int i = 0; i < num_parts; i++) {
            parts[i] = unpack_u32_le(&part_buf[i * 4]);
        }
    }

    int16_t prev_sx = 0;
    int16_t prev_sy = 0;

    uint32_t current_part_idx = 0;

    uint32_t next_part_start =
        (num_parts > 1)
            ? parts[1]
            : (uint32_t)num_points;

    for (uint32_t i = 0; i < (uint32_t)num_points; i++) {
        uint8_t pt_buf[8];

        if (mlp_fs->read(mlp_fs->handle, pt_buf, 8) != 8) {
            break;
        }

        /*
         * MLP хранит координаты в своей целочисленной шкале.
         * Текущий parser преобразует их умножением на 10.
         * Это существующая логика проекта; она здесь намеренно
         * не изменяется.
         */
        int32_t raw_x = unpack_i32_le(&pt_buf[0]);
        int32_t raw_y = unpack_i32_le(&pt_buf[4]);

        int32_t norm_x = raw_x * 10;
        int32_t norm_y = raw_y * 10;

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
         * Сначала проверяем переход к новой части геометрии
         */
        if (i == next_part_start) {
            current_part_idx++;

            next_part_start =
                (current_part_idx + 1 < (uint32_t)num_parts)
                    ? parts[current_part_idx + 1]
                    : (uint32_t)num_points;
        }

        uint32_t current_part_offset = (num_parts > 0) ? parts[current_part_idx] : 0;

        /*
         * Линия отрисовывается ТОЛЬКО если текущая точка i не является первой
         * точкой внутри текущей части массива геометрии.
         */
        if (i > current_part_offset) {
            /*
             * Диагностируем только первую линию, которая действительно
             * передаётся в GFX. Это не изменяет rendering behaviour.
             */
            if (diag && diag->lines_drawn == 0) {
                PURRGO_LOG(

                    "MAP: FIRST LINE "
                    "map=(%ld,%ld)->(%ld,%ld) "
                    "screen=(%d,%d)->(%d,%d) "
                    "viewport=(%d,%d,%u,%u) "
                    "camera=(%ld,%ld,%ld,%ld)\n",
                    (long)(prev_sx), (long)(prev_sy),
                    (long)norm_x, (long)norm_y,
                    (int)prev_sx, (int)prev_sy,
                    (int)sx, (int)sy,
                    (int)vp->offset_x, (int)vp->offset_y,
                    (unsigned)vp->width, (unsigned)vp->height,
                    (long)cam->min_x, (long)cam->min_y,
                    (long)cam->max_x, (long)cam->max_y
                );

            }

            gfx_draw_line(gfx, prev_sx, prev_sy, sx, sy);

            if (diag) {
                diag->lines_drawn++;
            }
        }

        prev_sx = sx;
        prev_sy = sy;
    }
}

/* Рекурсивный обход SQT R-дерева. */
static void parse_node(
    purrgo_fs_t* idx_fs,
    uint32_t* current_idx_offset,
    purrgo_fs_t* mlp_fs,
    bool is_nav_node,
    uint32_t level,
    const purrgo_bbox_t* cam,
    const purrgo_viewport_t* vp,
    gfx_context_t* gfx,
    map_diag_t* diag
) {
    uint8_t node_buf[28];

    if (idx_fs->read(idx_fs->handle, node_buf, 28) != 28) {
        return;
    }

    *current_idx_offset += 28;

    if (!is_nav_node) {
        if (diag) {
            diag->data_visited++;
        }

        float f_xmin = unpack_float_le(&node_buf[0]);
        float f_ymin = unpack_float_le(&node_buf[4]);
        float f_xmax = unpack_float_le(&node_buf[8]);
        float f_ymax = unpack_float_le(&node_buf[12]);

        int32_t xmin = (int32_t)(f_xmin * 10000000.0f);
        int32_t ymin = (int32_t)(f_ymin * 10000000.0f);
        int32_t xmax = (int32_t)(f_xmax * 10000000.0f);
        int32_t ymax = (int32_t)(f_ymax * 10000000.0f);

        bool passes =
            xmax >= cam->min_x &&
            xmin <= cam->max_x &&
            ymax >= cam->min_y &&
            ymin <= cam->max_y;

        if (diag) {
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
                    f_xmin, f_ymin, f_xmax, f_ymax,
                    xmin, ymin, xmax, ymax,
                    passes ? "PASS" : "CULL"
                );

                diag->nodes_logged++;
            }
        }

        if (passes) {
            uint32_t v1 = unpack_u32_le(&node_buf[20]);

            if (v1 > 0) {
                parse_geometry_mlp(
                    mlp_fs,
                    v1,
                    cam,
                    vp,
                    gfx,
                    diag
                );
            }
        }

        return;
    }

    if (diag) {
        diag->nav_visited++;
    }

    uint32_t v3_jump = unpack_u32_le(&node_buf[0]);

    float f_c_xmin = unpack_float_le(&node_buf[4]);
    float f_c_ymin = unpack_float_le(&node_buf[8]);
    float f_c_xmax = unpack_float_le(&node_buf[12]);
    float f_c_ymax = unpack_float_le(&node_buf[16]);

    int32_t c_xmin = (int32_t)(f_c_xmin * 10000000.0f);
    int32_t c_ymin = (int32_t)(f_c_ymin * 10000000.0f);
    int32_t c_xmax = (int32_t)(f_c_xmax * 10000000.0f);
    int32_t c_ymax = (int32_t)(f_c_ymax * 10000000.0f);

    if (diag && diag->nodes_logged < 10) {
        PURRGO_LOG(

            "MAP: NAV "
            "raw=(%08x,%08x,%08x,%08x) "
            "flt=(%f,%f,%f,%f) "
            "int=(%d,%d,%d,%d)\n",
            unpack_u32_le(&node_buf[4]),
            unpack_u32_le(&node_buf[8]),
            unpack_u32_le(&node_buf[12]),
            unpack_u32_le(&node_buf[16]),
            f_c_xmin, f_c_ymin, f_c_xmax, f_c_ymax,
            c_xmin, c_ymin, c_xmax, c_ymax
        );

        diag->nodes_logged++;
    }

    uint32_t nav_level = unpack_u32_le(&node_buf[20]);
    uint32_t obj_count = unpack_u32_le(&node_buf[24]);

    /*
     * Если BBox NAV node не пересекается с camera, можно пропустить
     * весь его поддеревом через v3_jump.
     */
    if (c_xmax < cam->min_x ||
        c_xmin > cam->max_x ||
        c_ymax < cam->min_y ||
        c_ymin > cam->max_y) {

        if (v3_jump >= 8) {
            uint32_t jump_amount = v3_jump - 8;

            if (idx_fs->seek(
                    idx_fs->handle,
                    *current_idx_offset + jump_amount)) {
                *current_idx_offset += jump_amount;
            }
        }

        return;
    }

bool child_is_nav = (nav_level > 0);
    uint32_t child_level =
        (nav_level > 0) ? (nav_level - 1) : 0;

    for (uint32_t i = 0; i < obj_count; i++) {
        parse_node(
            idx_fs,
            current_idx_offset,
            mlp_fs,
            child_is_nav,
            child_level,
            cam,
            vp,
            gfx,
            diag
        );
    }
}

void purrgo_map_render_layer(
    purrgo_fs_t* idx_fs,
    purrgo_fs_t* mlp_fs,
    gfx_context_t* gfx,
    const purrgo_bbox_t* camera,
    const purrgo_viewport_t* viewport,
    bool is_polygon_layer
) {
    PURRGO_LOG("MAP: IDX opened\n");

    PURRGO_LOG(

        "MAP: CAMERA min=(%ld,%ld) max=(%ld,%ld)\n",
        (long)camera->min_x,
        (long)camera->min_y,
        (long)camera->max_x,
        (long)camera->max_y
    );

    PURRGO_LOG(

        "MAP: VIEWPORT offset=(%d,%d) size=(%u,%u)\n",
        (int)viewport->offset_x,
        (int)viewport->offset_y,
        (unsigned)viewport->width,
        (unsigned)viewport->height
    );

    map_diag_t diag = {0};

    uint32_t current_idx_offset = 0;

    /*
     * YZL header занимает первые 32 байта IDX.
     */
    uint8_t yzl_header[32];

    if (idx_fs->read(
            idx_fs->handle,
            yzl_header,
            32) != 32) {
        PURRGO_LOG("MAP: ERROR reading YZL header\n");

        return;
    }

    current_idx_offset += 32;

    if (yzl_header[0] != 'Y' ||
        yzl_header[1] != 'Z' ||
        yzl_header[2] != 'L') {
        PURRGO_LOG("MAP: ERROR invalid YZL header\n");

        return;
    }

    /*
     * Последовательно читаем SQT blocks до EOF/невалидного блока.
     */
    while (true) {
        uint8_t sqt_header[16];

        if (idx_fs->read(
                idx_fs->handle,
                sqt_header,
                16) != 16) {
            break;
        }

        current_idx_offset += 16;

        if (sqt_header[0] != 'S' ||
            sqt_header[1] != 'Q' ||
            sqt_header[2] != 'T' ||
            sqt_header[3] != 0x01) {
            break;
        }

        diag.sqt_blocks++;

        uint32_t mode = unpack_u32_le(&sqt_header[8]);
        uint32_t count = unpack_u32_le(&sqt_header[12]);

        if (count == 0) {
            continue;
        }

        bool is_nav = (mode > 0);
        uint32_t level =
            (mode > 0) ? (mode - 1) : 0;

        for (uint32_t i = 0; i < count; i++) {
            parse_node(
                idx_fs,
                &current_idx_offset,
                mlp_fs,
                is_nav,
                level,
                camera,
                viewport,
                gfx,
                &diag
            );
        }
    }

    PURRGO_LOG(

        "MAP: SQT=%u NAV=%u DATA=%u PASS=%u CULL=%u LINES=%u\n",
        diag.sqt_blocks,
        diag.nav_visited,
        diag.data_visited,
        diag.data_passed,
        diag.data_culled,
        diag.lines_drawn
    );


}