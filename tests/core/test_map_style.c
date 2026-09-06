#include "purrgo/map_style.h"
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

static void test_purrgo_map_style_from_feature(void) {
    // Valid mappings
    assert(purrgo_map_style_from_feature(PURRGO_FEATURE_NO_CLASS) == PURRGO_STYLE_NONE);
    assert(purrgo_map_style_from_feature(PURRGO_FEATURE_ROAD_MAJOR) == PURRGO_STYLE_DARK_GRAY_THICK_LINE);
    assert(purrgo_map_style_from_feature(PURRGO_FEATURE_ROAD_NORMAL) == PURRGO_STYLE_DARK_GRAY_SEMITHICK_LINE);
    assert(purrgo_map_style_from_feature(PURRGO_FEATURE_ROAD_MINOR) == PURRGO_STYLE_DARK_GRAY_LINE);
    assert(purrgo_map_style_from_feature(PURRGO_FEATURE_ROAD_NORMAL_2) == PURRGO_STYLE_DARK_GRAY_SEMITHICK_LINE);
    assert(purrgo_map_style_from_feature(PURRGO_FEATURE_ROAD_UNPAVED) == PURRGO_STYLE_DARK_GRAY_DASHED_LINE);
    assert(purrgo_map_style_from_feature(PURRGO_FEATURE_ROAD_PATH) == PURRGO_STYLE_DARK_GRAY_DOTTED_LINE);
    assert(purrgo_map_style_from_feature(PURRGO_FEATURE_RAILWAY) == PURRGO_STYLE_RAILWAY_LINE);
    assert(purrgo_map_style_from_feature(PURRGO_FEATURE_LANDUSE_NATURAL) == PURRGO_STYLE_LIGHT_GRAY_FILL);
    assert(purrgo_map_style_from_feature(PURRGO_FEATURE_LANDUSE_HUMAN) == PURRGO_STYLE_LIGHT_GRAY_FILL);
    assert(purrgo_map_style_from_feature(PURRGO_FEATURE_WATER) == PURRGO_STYLE_DARK_GRAY_FILL);
    assert(purrgo_map_style_from_feature(PURRGO_FEATURE_POI_BIG) == PURRGO_STYLE_DARK_GRAY_BIG_CIRCLE);
    assert(purrgo_map_style_from_feature(PURRGO_FEATURE_POI_SMALL) == PURRGO_STYLE_DARK_GRAY_CIRCLE);

    // Invalid/Unknown mapping
    assert(purrgo_map_style_from_feature(9999) == PURRGO_STYLE_NONE);
}

static void test_purrgo_map_style_is_visible(void) {
    // Should be false for PURRGO_STYLE_NONE mapping
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_NO_CLASS) == false);
    assert(purrgo_map_style_is_visible(9999) == false);

    // Should be true for other mappings
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_ROAD_MAJOR) == true);
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_WATER) == true);
}

int main(void) {
    test_purrgo_map_style_from_feature();
    test_purrgo_map_style_is_visible();
    printf("test_map_style passed!\n");
    return 0;
}
