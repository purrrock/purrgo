#ifndef PURRGO_CONFIG_H
#define PURRGO_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "purrgo/track_logger.h"

// Глобальная структура настроек устройства
typedef struct {
    int16_t tz_offset_minutes;    // Смещение часового пояса от UTC в минутах[cite: 3]
    track_logger_mode_t log_mode; // Режим записи трека[cite: 3]
    bool backlight_on;            // Состояние подсветки дисплея[cite: 3]
    
    // Новые параметры навигатора
    char map_dir[32];             // Директория выбранной карты
    int32_t last_lat_1e7;         // Последняя широта для центрирования при холодном старте
    int32_t last_lon_1e7;         // Последняя долгота для центрирования при холодном старте
} purrgo_config_t;

extern purrgo_config_t app_config;

void purrgo_config_init(void);

// Функции управления файлом конфигурации
bool purrgo_config_load(void);
bool purrgo_config_save(void);

#endif // PURRGO_CONFIG_H