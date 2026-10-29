#ifndef PURRGO_MAP_INTERNAL_H
#define PURRGO_MAP_INTERNAL_H

#include <stdint.h>

/*
 * Shared map subsystem constants and diagnostics.
 */

#define PURRGO_MAP_MAX_PARTS 32
#define PURRGO_MAP_MAX_POINTS 512
/* Размер сектора SD/FatFs; геометрия читается порциями до 512 байт. */
#define PURRGO_MAP_READ_CHUNK_SIZE 512u

/*
 * Read-ahead для .mlp: секторов за один многосекторный запрос к SD при промахе
 * кэша (см. purrgo_fs_set_read_ahead). Геометрия читается подряд, и блок из
 * нескольких секторов дешевле, чем столько же одиночных запросов (на реальных
 * картах Беларуси -10...-20% времени ввода-вывода на 2-5 км). Для .idx и .db
 * блоки не включаются: доступ к ним случайный.
 */
#define PURRGO_MAP_MLP_READ_AHEAD_SECTORS 4u

/*
 * Порог пропуска мелких объектов: если обе стороны bbox объекта меньше порога
 * в пикселях экрана, его геометрия не читается и не рисуется (на дальних
 * масштабах это десятки процентов объектов и столько же чтений с SD).
 * Для линий порог ниже: дорога из коротких отрезков не должна рваться.
 * 0 отключает пропуск.
 */
#define PURRGO_MAP_MIN_PX_LINE    2
#define PURRGO_MAP_MIN_PX_POLYGON 3

typedef struct {
    uint8_t  file_type;
    uint32_t payload_size;
    uint32_t lod_offset[3];
} pgo_header_info_t;

/* Little-endian helpers */
static inline int32_t unpack_i32_le(const uint8_t *buf)
{
    return (int32_t)(
        (uint32_t)buf[0] |
        ((uint32_t)buf[1] << 8) |
        ((uint32_t)buf[2] << 16) |
        ((uint32_t)buf[3] << 24)
    );
}

static inline uint32_t unpack_u32_le(const uint8_t *buf)
{
    return
        (uint32_t)buf[0] |
        ((uint32_t)buf[1] << 8) |
        ((uint32_t)buf[2] << 16) |
        ((uint32_t)buf[3] << 24);
}

#endif // PURRGO_MAP_INTERNAL_H
