#ifndef PURRGO_CONFIG_H
#define PURRGO_CONFIG_H

#include <stdint.h>
#include "purrgo/track_logger.h" // Для track_logger_mode_t

// Глобальная структура настроек устройства
typedef struct {
    int8_t timezone_offset_h;     // Сдвиг часового пояса (-12 .. +14)
    track_logger_mode_t log_mode; // Режим записи трека (Standard/Expedition)
    bool backlight_on;            // Состояние подсветки дисплея
} purrgo_config_t;

// Экспорт глобального экземпляра настроек
extern purrgo_config_t app_config;

// Инициализация настроек значениями по умолчанию
void purrgo_config_init(void);

#endif // PURRGO_CONFIG_H