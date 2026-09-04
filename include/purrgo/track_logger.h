#ifndef PURRGO_TRACK_LOGGER_H
#define PURRGO_TRACK_LOGGER_H

#include "purrgo/gnss_types.h"
#include "purrgo/fs_hal.h"

typedef enum {
    LOGGER_STATE_IDLE,
    LOGGER_STATE_RECORDING,
    LOGGER_STATE_ERROR
} track_logger_state_t;

#define TRACK_RAM_MAX_POINTS 2048 //8192 - если RAM будет с избытком

typedef struct {
    int32_t lat_1e7;
    int32_t lon_1e7;
} track_point_t;

// Режимы записи трека
typedef enum {
    LOGGER_MODE_STANDARD = 0,   // 5 метров или 5 минут
    LOGGER_MODE_EXPEDITION = 1, // 100 метров или 15 минут
    LOGGER_MODE_OFF = 2
} track_logger_mode_t;

// Установка режима (можно вызывать на лету из меню настроек)
void purrgo_logger_set_mode(track_logger_mode_t mode);
track_logger_mode_t purrgo_logger_get_mode(void);

bool purrgo_logger_start(const purrgo_gnss_solution_t* first_fix);
bool purrgo_logger_add_point(const purrgo_gnss_solution_t* fix);
void purrgo_logger_stop(void);
const char* purrgo_logger_get_active_filename(void);
track_logger_state_t purrgo_logger_get_state(void);

size_t purrgo_logger_get_track_points(
    track_point_t* out_points,
    size_t max_points
);

#endif // PURRGO_TRACK_LOGGER_H