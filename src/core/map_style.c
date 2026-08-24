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
        PURRGO_STYLE_DARK_GRAY_FILL
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
     * Feature codes сейчас представляют собой небольшую
     * плотную таблицу 0..12.
     *
     * Тем не менее выполняем линейный поиск, а не обращение
     * feature_code напрямую как к индексу массива.
     *
     * Это сохраняет возможность позже добавить sparse
     * feature codes без изменения API.
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