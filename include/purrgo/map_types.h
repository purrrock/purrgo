#ifndef PURRGO_MAP_TYPES_H
#define PURRGO_MAP_TYPES_H

#include <stdbool.h>

/*
 * Типы, общие для ядра карты (map.c, map_idx.c), контроллера карты и конфига.
 * Ядро карты не обращается к конфигу и контроллеру: всё, что влияет на рендер,
 * приходит в purrgo_map_render_params_t.
 */

/* Масштаб карты (ширина экрана). */
typedef enum {
    PURRGO_MAP_SCALE_10M,
    PURRGO_MAP_SCALE_20M,
    PURRGO_MAP_SCALE_50M,
    PURRGO_MAP_SCALE_100M,
    PURRGO_MAP_SCALE_200M,
    PURRGO_MAP_SCALE_500M,
    PURRGO_MAP_SCALE_1KM,
    PURRGO_MAP_SCALE_2KM,
    PURRGO_MAP_SCALE_5KM,
    PURRGO_MAP_SCALE_10KM,
    PURRGO_MAP_SCALE_20KM,
    PURRGO_MAP_SCALE_50KM,
    PURRGO_MAP_SCALE_100KM,
    PURRGO_MAP_SCALE_200KM,
    PURRGO_MAP_SCALE_500KM,
    PURRGO_MAP_SCALE_1000KM,
    PURRGO_MAP_SCALE_2000KM,
    PURRGO_MAP_SCALE_5000KM,
    PURRGO_MAP_SCALE_10000KM,
    PURRGO_MAP_SCALE_COUNT
} purrgo_map_scale_t;

/* Режим отображения POI. */
typedef enum {
    PURRGO_POI_MODE_NO = 0,
    PURRGO_POI_MODE_CIRCLES,
    PURRGO_POI_MODE_ICONS
} purrgo_poi_mode_t;

/* Детализация карты. */
typedef enum {
    PURRGO_MAP_DETAILS_LOW = 0,
    PURRGO_MAP_DETAILS_HIGH
} purrgo_map_details_t;

/*
 * Параметры рендера карты. Заполняет вызывающий (UI), ядро только читает.
 * Нулевая структура: слои выключены, масштаб 10 м, детализация LOW, POI не рисуются.
 */
typedef struct {
    const char          *map_dir;           /* каталог карты (без завершающего '/') */
    purrgo_map_scale_t   scale;             /* текущий масштаб: выбор уровня LOD */
    purrgo_map_details_t details;           /* детализация: выбор уровня LOD */
    purrgo_poi_mode_t    poi_mode;          /* маркеры POI */

    bool layer_landuse;                     /* полигоны landuse/water */
    bool layer_landuse_labels;              /* открывать landuse.db */
    bool layer_roads;                       /* дороги */
    bool layer_poi;                         /* маркеры POI (pois.idx) */
    bool layer_poi_labels;                  /* подписи POI и полигонов (pois.db) */
} purrgo_map_render_params_t;

#endif /* PURRGO_MAP_TYPES_H */
