// file: include/purrgo/map_style.h
#ifndef PURRGO_MAP_STYLE_H
#define PURRGO_MAP_STYLE_H

#include <stdint.h>
#include <stdbool.h>

/*
 * PurrGO feature codes.
 *
 * Эти значения являются частью бинарного формата карты.
 *
 * ВАЖНО:
 * Feature code != OSM tag.
 *
 * OSM -> PurrGO feature code выполняется компилятором карт.
 * Навигатор работает только с уже скомпилированным feature code.
 */
typedef enum {
    PURRGO_FEATURE_NO_CLASS       = 0,

    PURRGO_FEATURE_ROAD_MAJOR     = 1,
    PURRGO_FEATURE_ROAD_NORMAL    = 2,
    PURRGO_FEATURE_ROAD_MINOR     = 3,
    PURRGO_FEATURE_ROAD_NORMAL_2  = 4,
    PURRGO_FEATURE_ROAD_UNPAVED   = 5,
    PURRGO_FEATURE_ROAD_PATH      = 6,
    PURRGO_FEATURE_RAILWAY        = 7,

    PURRGO_FEATURE_LANDUSE_NATURAL = 8,
    PURRGO_FEATURE_LANDUSE_HUMAN   = 9,

    PURRGO_FEATURE_WATER          = 10,

    PURRGO_FEATURE_POI_BIG        = 11,
    PURRGO_FEATURE_POI_SMALL      = 12

} purrgo_feature_code_t;


/*
 * Геометрический тип стиля.
 *
 * Пока описываем только принцип отображения.
 * Конкретные параметры GFX будут определены отдельно.
 */
typedef enum {
    PURRGO_STYLE_NONE = 0,

    /* Lines */
    PURRGO_STYLE_DARK_GRAY_THICK_LINE,
    PURRGO_STYLE_DARK_GRAY_SEMITHICK_LINE,
    PURRGO_STYLE_DARK_GRAY_LINE,
    PURRGO_STYLE_DARK_GRAY_DASHED_LINE,
    PURRGO_STYLE_DARK_GRAY_DOTTED_LINE,
    PURRGO_STYLE_RAILWAY_LINE,

    /* Filled areas */
    PURRGO_STYLE_LIGHT_GRAY_FILL,
    PURRGO_STYLE_DARK_GRAY_FILL,

    /* Points */
    PURRGO_STYLE_DARK_GRAY_BIG_CIRCLE,
    PURRGO_STYLE_DARK_GRAY_CIRCLE

} purrgo_map_style_t;


/*
 * Описание соответствия feature code -> style.
 *
 * Layer здесь намеренно не хранится.
 *
 * Layer уже определяется вызывающим кодом:
 *
 *     roads
 *     landuse
 *     water
 *     pois
 *
 * Feature code является ключом выбора стиля.
 */
typedef struct {
    purrgo_feature_code_t feature_code;
    purrgo_map_style_t style;
} purrgo_map_style_entry_t;


/*
 * Получить стиль для PurrGO feature code.
 *
 * Неизвестный feature code возвращает PURRGO_STYLE_NONE.
 */
purrgo_map_style_t purrgo_map_style_from_feature(
    uint32_t feature_code
);


/*
 * Проверка существования отображаемого стиля.
 */
bool purrgo_map_style_is_visible(
    uint32_t feature_code
);

#endif /* PURRGO_MAP_STYLE_H */