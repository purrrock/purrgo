#ifndef PURRGO_CONFIG_H
#define PURRGO_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "purrgo/track_logger.h"
#include "purrgo/map_types.h"


/* purrgo_poi_mode_t и purrgo_map_details_t определены в map_types.h */


/*
 * Глобальная структура настроек устройства.
 */
typedef struct {
    int16_t tz_offset_minutes;
    track_logger_mode_t log_mode;
    bool backlight_on;

    /*
     * Директория выбранной карты.
     */
    char map_dir[PURRGO_FS_MAX_PATH + 32];

    /*
     * Последняя широта для центрирования при холодном старте.
     */
    int32_t last_lat_1e7;

    /*
     * Последняя долгота для центрирования при холодном старте.
     */
    int32_t last_lon_1e7;

    /*
     * Режим отображения POI.
     */
    purrgo_poi_mode_t poi_mode;

    /*
     * Уровень детализации карты.
     */
    purrgo_map_details_t map_details;

    /*
     * Видимость слоев карты.
     */
    bool layer_landuse;
    bool layer_water;
    bool layer_landuse_labels;
    bool layer_water_labels;
    bool layer_roads;
    bool layer_poi;
    bool layer_poi_labels;
    bool layer_route;
    bool layer_track;

} purrgo_config_t;


extern purrgo_config_t app_config;


void purrgo_config_init(void);


/*
 * Функции управления файлом конфигурации.
 */
bool purrgo_config_load(void);
bool purrgo_config_save(void);


#endif /* PURRGO_CONFIG_H */