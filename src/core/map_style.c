// file: src/core/map_style.c

#include "purrgo/map_style.h"


/*
 * Таблица соответствия:
 *
 *     PurrGO feature code -> render style
 *
 * Таблица намеренно статическая и const:
 *
 * - не требует RAM для модифицируемых данных;
 * - не требует динамической памяти;
 * - легко сопоставляется с features.csv;
 * - подходит для STM32.
 *
 * ВНИМАНИЕ:
 *
 * Feature code 2 и 4 имеют одинаковый PG_class
 * ROAD_NORMAL и одинаковый STYLE.
 *
 * Они остаются отдельными кодами, поскольку это два разных
 * feature code бинарного формата.
 */
static const purrgo_map_style_entry_t s_style_table[] = {

    {
        PURRGO_FEATURE_NO_CLASS,
        PURRGO_STYLE_NONE
    },

    {
        PURRGO_FEATURE_ROAD_MAJOR,
        PURRGO_STYLE_DARK_GRAY_THICK_LINE
    },

    {
        PURRGO_FEATURE_ROAD_NORMAL,
        PURRGO_STYLE_DARK_GRAY_SEMITHICK_LINE
    },

    {
        PURRGO_FEATURE_ROAD_MINOR,
        PURRGO_STYLE_DARK_GRAY_LINE
    },

    {
        PURRGO_FEATURE_ROAD_NORMAL_2,
        PURRGO_STYLE_DARK_GRAY_SEMITHICK_LINE
    },

    {
        PURRGO_FEATURE_ROAD_UNPAVED,
        PURRGO_STYLE_DARK_GRAY_DASHED_LINE
    },

    {
        PURRGO_FEATURE_ROAD_PATH,
        PURRGO_STYLE_DARK_GRAY_DOTTED_LINE
    },

    {
        PURRGO_FEATURE_RAILWAY,
        PURRGO_STYLE_RAILWAY_LINE
    },

    {
        PURRGO_FEATURE_LANDUSE_NATURAL,
        PURRGO_STYLE_LIGHT_GRAY_FILL
    },

    {
        PURRGO_FEATURE_LANDUSE_HUMAN,
        PURRGO_STYLE_LIGHT_GRAY_FILL
    },

    {
        PURRGO_FEATURE_WATER,
        PURRGO_STYLE_WATER_FILL
    },

    {
        PURRGO_FEATURE_WETLAND,
        PURRGO_STYLE_WETLAND_HATCH
    },

    {
        PURRGO_FEATURE_GLACIER,
        PURRGO_STYLE_GLACIER_HATCH
    },

    {
        PURRGO_FEATURE_SAND,
        PURRGO_STYLE_SAND_DOTS
    },

    {
        PURRGO_FEATURE_WATERWAY_MAJOR,
        PURRGO_STYLE_WATER_THICK_LINE
    },

    {
        PURRGO_FEATURE_WATERWAY_MINOR,
        PURRGO_STYLE_WATER_LINE
    },

    {
        PURRGO_FEATURE_POI_BIG,
        PURRGO_STYLE_DARK_GRAY_BIG_CIRCLE
    },

    {
        PURRGO_FEATURE_POI_SMALL,
        PURRGO_STYLE_DARK_GRAY_CIRCLE
    }
};


#define PURRGO_MAP_STYLE_COUNT \
    (sizeof(s_style_table) / sizeof(s_style_table[0]))


purrgo_map_style_t purrgo_map_style_from_feature(
    uint32_t feature_code
) {
    /*
     * Feature codes разнесены по диапазонам слоёв (0..31 roads,
     * 32..63 waterways, 64..95 water, 96..127 landuse, 128..191 POI,
     * 192..223 UI-иконки, 224..255 reserved). Остальные коды POI (130..) в таблице не
     * перечислены: слой pois рисуется иконкой purrgo_poi_icons[code]
     * или кругом и этой таблицей не пользуется.
     *
     * Выполняем линейный поиск, а не обращение feature_code напрямую
     * как к индексу массива: коды разрежены, а API остаётся прежним.
     */
    for (
        uint32_t i = 0;
        i < PURRGO_MAP_STYLE_COUNT;
        i++
    ) {
        if (
            (uint32_t)s_style_table[i].feature_code ==
            feature_code
        ) {
            return s_style_table[i].style;
        }
    }

    /*
     * Неизвестный feature code не должен приводить
     * к случайному стилю.
     */
    return PURRGO_STYLE_NONE;
}


bool purrgo_map_style_is_visible(
    uint32_t feature_code
) {
    return
        purrgo_map_style_from_feature(feature_code) !=
        PURRGO_STYLE_NONE;
}