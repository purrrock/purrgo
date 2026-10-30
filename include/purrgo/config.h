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
/*
 * Слои карты, которые пользователь включает и выключает в меню MAP LAYERS.
 * Порядок совпадает с порядком пунктов меню. Ключ в PURRGO.CFG (строка
 * LAYER_<КЛЮЧ>=0|1, например LAYER_ROADS=1), заголовок в меню и значение по
 * умолчанию заданы одной таблицей в config.c.
 */
typedef enum {
    PURRGO_LAYER_LANDUSE = 0,
    PURRGO_LAYER_WATER,
    PURRGO_LAYER_LANDUSE_LABELS,
    PURRGO_LAYER_WATER_LABELS,
    PURRGO_LAYER_ROADS,
    PURRGO_LAYER_POI,
    PURRGO_LAYER_POI_LABELS,
    PURRGO_LAYER_ROUTE,
    PURRGO_LAYER_TRACK,
    PURRGO_LAYER_COUNT
} purrgo_layer_t;

_Static_assert(PURRGO_LAYER_COUNT <= 16, "purrgo_config_t.layers - uint16_t");

static inline bool purrgo_layer_on(uint16_t mask, purrgo_layer_t id)
{
    return (((unsigned)mask >> (unsigned)id) & 1u) != 0u;
}

/* Заголовок пункта меню MAP LAYERS. */
const char* purrgo_layer_title(purrgo_layer_t id);

/* Маска слоёв по умолчанию. */
uint16_t purrgo_layers_default(void);

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
     * Видимость слоев карты: бит (1u << purrgo_layer_t) установлен - слой
     * показывается. Проверка - purrgo_layer_on().
     */
    uint16_t layers;
} purrgo_config_t;


extern purrgo_config_t app_config;


void purrgo_config_init(void);


/*
 * Функции управления файлом конфигурации.
 */
bool purrgo_config_load(void);
bool purrgo_config_save(void);


#endif /* PURRGO_CONFIG_H */