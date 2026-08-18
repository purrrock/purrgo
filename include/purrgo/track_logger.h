#ifndef PURRGO_TRACK_LOGGER_H
#define PURRGO_TRACK_LOGGER_H

#include "purrgo/gnss_types.h"
#include "purrgo/fs_hal.h"

// Состояния автомата логгера
typedef enum {
    LOGGER_STATE_IDLE,
    LOGGER_STATE_RECORDING,
    LOGGER_STATE_ERROR
} track_logger_state_t;

// Инициализация логгера. Передача указателя на первый валидный фикс для генерации имени файла.
bool purrgo_logger_start(const purrgo_gnss_solution_t* first_fix);

// Добавление точки в буфер. Если буфер заполнен — автоматический сброс на диск.
void purrgo_logger_add_point(const purrgo_gnss_solution_t* fix);

// Принудительный сброс остатков буфера, запись закрывающих тегов XML и закрытие файла.
void purrgo_logger_stop(void);

#endif // PURRGO_TRACK_LOGGER_H