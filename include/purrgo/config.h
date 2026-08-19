#ifndef PURRGO_CONFIG_H
#define PURRGO_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "purrgo/track_logger.h" // Для track_logger_mode_t

// Глобальная структура настроек устройства
typedef struct {
    int16_t tz_offset_minutes;    // Смещение часового пояса от UTC в минутах (например, +180 для UTC+3:00, -210 для UTC-3:30)
    track_logger_mode_t log_mode; // Режим записи трека (Standard/Expedition)
    bool backlight_on;            // Состояние подсветки дисплея
} purrgo_config_t;

// Экспорт глобального экземпляра настроек
extern purrgo_config_t app_config;

// Инициализация настроек значениями по умолчанию
void purrgo_config_init(void);

#endif // PURRGO_CONFIG_H