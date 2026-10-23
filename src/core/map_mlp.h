#ifndef PURRGO_MAP_MLP_H
#define PURRGO_MAP_MLP_H

#include <stdint.h>
#include <stdbool.h>
#include "purrgo/map.h"
#include "map_internal.h"

typedef struct {
    purrgo_file_t *mlp_file;
    uint32_t num_parts;
    uint32_t num_points;
    uint32_t parts[PURRGO_MAP_MAX_PARTS];

    // Iteration state
    uint32_t points_read;
    uint32_t current_part_idx;
    uint32_t next_part_start;

    /*
     * Буферизованный поток байт геометрии (заголовок, таблица частей, точки).
     * Порции читаются не более чем до конца объекта и не дальше конца текущего
     * сектора, пока размер объекта не известен, поэтому лишние сектора карты
     * не читаются.
     */
    uint8_t  chunk_buf[PURRGO_MAP_READ_CHUNK_SIZE];
    uint32_t buf_len;        /* число действительных байт в chunk_buf */
    uint32_t buf_pos;        /* следующий непрочитанный байт в chunk_buf */
    uint32_t file_pos;       /* абсолютная позиция файла после последнего чтения */
    uint32_t fetched;        /* байт прочитано из файла с начала объекта */
    uint32_t total_bytes;    /* размер объекта; UINT32_MAX, пока заголовок не разобран */
} map_mlp_iter_t;

/* Initializes iterator, reads header and parts */
bool map_mlp_iter_init(
    map_mlp_iter_t *iter,
    purrgo_file_t *mlp_file,
    uint32_t v1_offset
);

/* Returns next point, setting is_new_part if a new part begins */
bool map_mlp_iter_next(
    map_mlp_iter_t *iter,
    int32_t *raw_x,
    int32_t *raw_y,
    bool *is_new_part
);

#endif // PURRGO_MAP_MLP_H
