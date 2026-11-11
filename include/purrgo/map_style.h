// file: include/purrgo/map_style.h
#ifndef PURRGO_MAP_STYLE_H
#define PURRGO_MAP_STYLE_H

#include <stdint.h>
#include <stdbool.h>

/* Границы диапазонов feature code по слоям (включительно). */
#define PURRGO_FEATURE_ROADS_FIRST      0u
#define PURRGO_FEATURE_ROADS_LAST      31u
#define PURRGO_FEATURE_WATERWAYS_FIRST 32u
#define PURRGO_FEATURE_WATERWAYS_LAST  63u
#define PURRGO_FEATURE_WATER_FIRST     64u
#define PURRGO_FEATURE_WATER_LAST      95u
#define PURRGO_FEATURE_LANDUSE_FIRST   96u
#define PURRGO_FEATURE_LANDUSE_LAST   127u
#define PURRGO_FEATURE_POI_FIRST      128u
#define PURRGO_FEATURE_POI_LAST       191u
#define PURRGO_FEATURE_RESERVED_FIRST 192u
#define PURRGO_FEATURE_RESERVED_LAST  255u

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
 *
 * Диапазоны кодов (docs/map-format-v3.md, раздел 8.1):
 *
 *       0..31   ROADS
 *      32..63   WATERWAYS
 *      64..95   WATER
 *      96..127  LANDUSE
 *     128..191  POI
 *     192..255  RESERVED
 *
 * Новый класс добавляется в конец диапазона своего слоя. Код, однажды
 * попавший в карты, не переназначается.
 */
typedef enum {
    /* --- 0..31: ROADS (слой roads) --- */
    PURRGO_FEATURE_NO_CLASS       = 0,

    PURRGO_FEATURE_ROAD_MAJOR     = 1,
    PURRGO_FEATURE_ROAD_NORMAL    = 2,
    PURRGO_FEATURE_ROAD_MINOR     = 3,
    PURRGO_FEATURE_ROAD_NORMAL_2  = 4,
    PURRGO_FEATURE_ROAD_UNPAVED   = 5,
    PURRGO_FEATURE_ROAD_PATH      = 6,
    PURRGO_FEATURE_RAILWAY        = 7,

    /* --- 32..63: WATERWAYS (линейные водные объекты, слой waterways) --- */
    PURRGO_FEATURE_WATERWAY_MAJOR = 32,
    PURRGO_FEATURE_WATERWAY_MINOR = 33,

    /* --- 64..95: WATER (полигоны слоя water) --- */
    PURRGO_FEATURE_WATER          = 64,
    /* Рисуются штриховкой, а не чёрной заливкой. */
    PURRGO_FEATURE_WETLAND        = 65,
    PURRGO_FEATURE_GLACIER        = 66,

    /* --- 96..127: LANDUSE (полигоны слоя landuse) --- */
    PURRGO_FEATURE_LANDUSE_NATURAL = 96,
    PURRGO_FEATURE_LANDUSE_HUMAN   = 97,
    /* Пляж, песок, дюны, галька, осыпь: рисуются точками. */
    PURRGO_FEATURE_SAND            = 98,

    /*
     * --- 128..191: POI (слой pois) ---
     *
     * Номер иконки в purrgo_poi_icons[] равен feature code.
     * Прочие коды POI (130..) заданы в features.csv; здесь перечислены
     * только те, что нужны прошивке по имени.
     */
    PURRGO_FEATURE_POI_BIG        = 128,
    PURRGO_FEATURE_POI_SMALL      = 129

    /* --- 192..255: RESERVED --- */

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

    /*
     * Вода (линии): чёрная линия без обводки.
     * THICK - крупные водотоки (река, канал): 3 px.
     * LINE  - малые (ручей): 1 px.
     */
    PURRGO_STYLE_WATER_THICK_LINE,
    PURRGO_STYLE_WATER_LINE,

    /* Filled areas */
    PURRGO_STYLE_LIGHT_GRAY_FILL,
    PURRGO_STYLE_DARK_GRAY_FILL,
    /* Вода (полигоны): чёрная заливка с тёмно-серой обводкой контура. */
    PURRGO_STYLE_WATER_FILL,
    /* Болото: короткие горизонтальные чёрные штрихи в шахматном порядке, без контура. */
    PURRGO_STYLE_WETLAND_HATCH,
    /* Ледник: заливка мелкими тёмно-серыми снежинками, без контура. */
    PURRGO_STYLE_GLACIER_HATCH,
    /* Пляж, песок и подобное: заливка тёмно-серыми точками в шахматном порядке, без контура. */
    PURRGO_STYLE_SAND_DOTS,

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