// file: src/core/map.c
#include "purrgo/map.h"

#define PURRGO_MAP_MAX_PARTS 32
#define SCREEN_WIDTH 600
#define SCREEN_HEIGHT 600

/* * Безопасное чтение Little-Endian int32_t. 
 * Предотвращает HardFault (Unaligned Memory Access) на ARM-архитектурах.
 */
static inline int32_t unpack_i32_le(const uint8_t* buf) {
    return (int32_t)(buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
}

static inline uint32_t unpack_u32_le(const uint8_t* buf) {
    return (uint32_t)(buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
}

/* Проекция целочисленных координат (10^7) в экранные пиксели */
static void project_to_screen(int32_t lon, int32_t lat, const purrgo_bbox_t* cam, int16_t* sx, int16_t* sy) {
    int64_t dx = (int64_t)(lon - cam->min_x) * SCREEN_WIDTH;
    int64_t w = (int64_t)(cam->max_x - cam->min_x);
    *sx = (int16_t)(w > 0 ? (dx / w) : 0);

    int64_t dy = (int64_t)(lat - cam->min_y) * SCREEN_HEIGHT;
    int64_t h = (int64_t)(cam->max_y - cam->min_y);
    *sy = (int16_t)(SCREEN_HEIGHT - (h > 0 ? (dy / h) : 0)); // Ось Y инвертирована
}

/* * Потоковый парсинг геометрии. 
 * Читает вершины по одной (буфер 8 байт), избегая переполнения RAM.
 */
static void parse_geometry_mlp(purrgo_fs_t* mlp_fs, uint32_t v1_offset, const purrgo_bbox_t* cam, purrgo_gfx_t* gfx) {
    // v1 указывает на смещение от конца глобального YZL-заголовка (32 байта)
    if (!mlp_fs->seek(mlp_fs->handle, 32 + v1_offset)) return;

    uint8_t head[24];
    if (mlp_fs->read(mlp_fs->handle, head, 24) != 24) return;

    int32_t num_parts = unpack_i32_le(&head[16]);
    int32_t num_points = unpack_i32_le(&head[20]);

    if (num_parts < 0 || num_parts > PURRGO_MAP_MAX_PARTS || num_points <= 0) return;

    // Чтение массива индексов (узлы начала контуров/отверстий)
    uint32_t parts[PURRGO_MAP_MAX_PARTS] = {0};
    if (num_parts > 0) {
        uint8_t part_buf[PURRGO_MAP_MAX_PARTS * 4];
        uint32_t bytes_to_read = num_parts * 4;
        if (mlp_fs->read(mlp_fs->handle, part_buf, bytes_to_read) != bytes_to_read) return;
        
        for (int i = 0; i < num_parts; i++) {
            parts[i] = unpack_u32_le(&part_buf[i * 4]);
        }
    }

    int16_t prev_sx = 0, prev_sy = 0;
    uint32_t current_part_idx = 0;
    uint32_t next_part_start = (num_parts > 1) ? parts[1] : (uint32_t)num_points;

    // Потоковое чтение координат (по 8 байт на итерацию)
    for (uint32_t i = 0; i < (uint32_t)num_points; i++) {
        uint8_t pt_buf[8];
        if (mlp_fs->read(mlp_fs->handle, pt_buf, 8) != 8) break;

        // Координаты в формате 10^6
        int32_t raw_x = unpack_i32_le(&pt_buf[0]);
        int32_t raw_y = unpack_i32_le(&pt_buf[4]);

        // Приведение к базовой сетке системы (10^7)
        int32_t norm_x = raw_x * 10;
        int32_t norm_y = raw_y * 10;

        int16_t sx, sy;
        project_to_screen(norm_x, norm_y, cam, &sx, &sy);

        // Управление "поднятием пера" при переходе между изолированными кольцами мультиполигона
        if (i == next_part_start) {
            current_part_idx++;
            next_part_start = (current_part_idx + 1 < (uint32_t)num_parts) ? parts[current_part_idx + 1] : (uint32_t)num_points;
        } else if (i > parts[current_part_idx]) {
            // Отрисовка сегмента линии (если это не первая точка текущего контура)
            gfx->draw_line(prev_sx, prev_sy, sx, sy, 1);
        }

        prev_sx = sx;
        prev_sy = sy;
    }
}

/* Рекурсивный обход SQT R-дерева */
static void parse_node(purrgo_fs_t* idx_fs, purrgo_fs_t* mlp_fs, bool is_nav_node, uint32_t level, const purrgo_bbox_t* cam, purrgo_gfx_t* gfx) {
    uint8_t node_buf[28];
    if (idx_fs->read(idx_fs->handle, node_buf, 28) != 28) return;

    if (!is_nav_node) {
        // Data Node
        // Преобразование float BBox в int32_t (10^7) для целочисленного Culling-а
        float f_xmin = *((float*)&node_buf[0]);
        float f_ymin = *((float*)&node_buf[4]);
        float f_xmax = *((float*)&node_buf[8]);
        float f_ymax = *((float*)&node_buf[12]);
        
        int32_t xmin = (int32_t)(f_xmin * 10000000.0f);
        int32_t ymin = (int32_t)(f_ymin * 10000000.0f);
        int32_t xmax = (int32_t)(f_xmax * 10000000.0f);
        int32_t ymax = (int32_t)(f_ymax * 10000000.0f);

        if (xmax >= cam->min_x && xmin <= cam->max_x && ymax >= cam->min_y && ymin <= cam->max_y) {
            uint32_t v1 = unpack_u32_le(&node_buf[20]);
            if (v1 > 0) {
                parse_geometry_mlp(mlp_fs, v1, cam, gfx);
            }
        }
        return;
    }

    // Nav Node
    uint32_t v3_jump = unpack_u32_le(&node_buf[0]);
    
    float f_c_xmin = *((float*)&node_buf[4]);
    float f_c_ymin = *((float*)&node_buf[8]);
    float f_c_xmax = *((float*)&node_buf[12]);
    float f_c_ymax = *((float*)&node_buf[16]);

    int32_t c_xmin = (int32_t)(f_c_xmin * 10000000.0f);
    int32_t c_ymin = (int32_t)(f_c_ymin * 10000000.0f);
    int32_t c_xmax = (int32_t)(f_c_xmax * 10000000.0f);
    int32_t c_ymax = (int32_t)(f_c_ymax * 10000000.0f);

    uint32_t obj_count = unpack_u32_le(&node_buf[24]);

    if (c_xmax < cam->min_x || c_xmin > cam->max_x || c_ymax < cam->min_y || c_ymin > cam->max_y) {
        // Аппаратный прыжок (отсечение ветви)
        // Компенсация префетча (-8 байт) встроена в формат
        // Для эмулятора/FATFS требуется получить текущую позицию указателя
        // Примечание: абстракция fs должна поддерживать относительный сдвиг или чтение tell()
        // В данном концепте предполагается, что seek принимает относительный прыжок, либо требуется расширение API
        // Заглушка для демонстрации логики:
        // idx_fs->seek(idx_fs->handle, current_pos + v3_jump - 8);
        return;
    }

    bool child_is_nav = (level > 0);
    uint32_t child_level = (level > 0) ? (level - 1) : 0;

    for (uint32_t i = 0; i < obj_count; i++) {
        parse_node(idx_fs, mlp_fs, child_is_nav, child_level, cam, gfx);
    }
}