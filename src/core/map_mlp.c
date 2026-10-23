#include "map_mlp.h"
#include "purrgo/logger.h"
#include <stddef.h>
#include <string.h>

static bool validate_parts(
    const uint32_t *parts,
    uint32_t num_parts,
    uint32_t num_points
) {
    if (num_parts == 0) {
        return true;
    }

    if (parts[0] != 0) {
        return false;
    }

    for (uint32_t i = 1; i < num_parts; i++) {
        if (parts[i] <= parts[i - 1]) {
            return false;
        }
        if (parts[i] >= num_points) {
            return false;
        }
    }

    return true;
}


#define MLP_SIZE_UNKNOWN UINT32_MAX
#define MLP_HEADER_SIZE  24u

/* Прочитать следующую порцию из файла в chunk_buf. */
static bool mlp_refill(map_mlp_iter_t *iter)
{
    uint32_t len;

    if (iter->total_bytes == MLP_SIZE_UNKNOWN) {
        /* Размер объекта неизвестен: читаем только до конца текущего сектора. */
        len = PURRGO_MAP_READ_CHUNK_SIZE - (iter->file_pos % PURRGO_MAP_READ_CHUNK_SIZE);
    } else if (iter->fetched >= iter->total_bytes) {
        return false;
    } else {
        len = iter->total_bytes - iter->fetched;
        if (len > PURRGO_MAP_READ_CHUNK_SIZE) {
            len = PURRGO_MAP_READ_CHUNK_SIZE;
        }
    }

    uint32_t got = iter->mlp_fs->read(iter->mlp_fs->handle, iter->chunk_buf, len);
    if (got == 0 || got > len) {
        return false;
    }

    iter->buf_len = got;
    iter->buf_pos = 0;
    iter->file_pos += got;
    iter->fetched += got;
    return true;
}

/* Взять ровно n байт из потока; false - данные кончились (файл усечён). */
static bool mlp_take(map_mlp_iter_t *iter, uint8_t *dst, uint32_t n)
{
    while (n > 0) {
        if (iter->buf_pos >= iter->buf_len && !mlp_refill(iter)) {
            return false;
        }

        uint32_t chunk = iter->buf_len - iter->buf_pos;
        if (chunk > n) {
            chunk = n;
        }
        memcpy(dst, &iter->chunk_buf[iter->buf_pos], chunk);
        iter->buf_pos += chunk;
        dst += chunk;
        n -= chunk;
    }
    return true;
}

bool map_mlp_iter_init(
    map_mlp_iter_t *iter,
    purrgo_fs_t *mlp_fs,
    uint32_t v1_offset
) {
    if (iter == NULL || mlp_fs == NULL || mlp_fs->read == NULL || mlp_fs->seek == NULL) {
        return false;
    }

    iter->mlp_fs = mlp_fs;
    iter->points_read = 0;
    iter->current_part_idx = 0;
    iter->buf_len = 0;
    iter->buf_pos = 0;
    iter->fetched = 0;
    iter->total_bytes = MLP_SIZE_UNKNOWN;

    uint32_t absolute_body_offset = 32u + v1_offset;

    if (!mlp_fs->seek(mlp_fs->handle, absolute_body_offset)) {
        return false;
    }
    iter->file_pos = absolute_body_offset;

    uint8_t head[MLP_HEADER_SIZE];
    if (!mlp_take(iter, head, sizeof(head))) {
        return false;
    }

    int32_t num_parts = unpack_i32_le(&head[16]);
    int32_t num_points = unpack_i32_le(&head[20]);

    if (num_parts < 0 || num_parts > PURRGO_MAP_MAX_PARTS ||
        num_points <= 0 || num_points > PURRGO_MAP_MAX_POINTS) {
        PURRGO_LOG(
            "MAP: invalid MLP geometry "
            "parts=%ld points=%ld\n\r",
            (long)num_parts,
            (long)num_points
        );
        return false;
    }

    iter->num_parts = (uint32_t)num_parts;
    iter->num_points = (uint32_t)num_points;
    iter->total_bytes = MLP_HEADER_SIZE + iter->num_parts * 4u + iter->num_points * 8u;

    for (uint32_t i = 0; i < PURRGO_MAP_MAX_PARTS; i++) {
        iter->parts[i] = 0;
    }

    if (iter->num_parts > 0) {
        uint32_t bytes_to_read = iter->num_parts * 4u;
        uint8_t part_buf[PURRGO_MAP_MAX_PARTS * 4];

        if (!mlp_take(iter, part_buf, bytes_to_read)) {
            return false;
        }

        for (uint32_t i = 0; i < iter->num_parts; i++) {
            iter->parts[i] = unpack_u32_le(&part_buf[i * 4u]);
        }
    }

    if (!validate_parts(iter->parts, iter->num_parts, iter->num_points)) {
        PURRGO_LOG(
            "MAP: invalid MLP parts "
            "parts=%ld points=%ld\n\r",
            (long)num_parts,
            (long)num_points
        );
        return false;
    }

    iter->next_part_start = (iter->num_parts > 1) ? iter->parts[1] : iter->num_points;

    return true;
}

bool map_mlp_iter_next(
    map_mlp_iter_t *iter,
    int32_t *raw_x,
    int32_t *raw_y,
    bool *is_new_part
) {
    if (iter->points_read >= iter->num_points) {
        return false;
    }

    uint8_t point_buf[8];
    if (!mlp_take(iter, point_buf, sizeof(point_buf))) {
        return false;
    }

    *raw_x = unpack_i32_le(&point_buf[0]);
    *raw_y = unpack_i32_le(&point_buf[4]);

    *is_new_part = false;
    if (iter->num_parts > 0 && iter->points_read == iter->next_part_start) {
        iter->current_part_idx++;
        iter->next_part_start = (iter->current_part_idx + 1 < iter->num_parts)
            ? iter->parts[iter->current_part_idx + 1]
            : iter->num_points;
        *is_new_part = true;
    }

    iter->points_read++;
    return true;
}
