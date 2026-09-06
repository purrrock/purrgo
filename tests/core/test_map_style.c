#include "purrgo/map_style.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void test_purrgo_map_style_from_feature(void) {
    // Test mapping of known feature codes
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

    // Test unknown feature code
    assert(purrgo_map_style_from_feature(999) == PURRGO_STYLE_NONE);
}

void test_purrgo_map_style_is_visible(void) {
    // Invisible feature codes
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_NO_CLASS) == false);
    assert(purrgo_map_style_is_visible(999) == false);

    // Visible feature codes
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_ROAD_MAJOR) == true);
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_ROAD_NORMAL) == true);
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_ROAD_MINOR) == true);
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_ROAD_NORMAL_2) == true);
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_ROAD_UNPAVED) == true);
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_ROAD_PATH) == true);
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_RAILWAY) == true);
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_LANDUSE_NATURAL) == true);
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_LANDUSE_HUMAN) == true);
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_WATER) == true);
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_POI_BIG) == true);
    assert(purrgo_map_style_is_visible(PURRGO_FEATURE_POI_SMALL) == true);
}

int main(void) {
    printf("Running test_map_style...\n");

    test_purrgo_map_style_from_feature();
    test_purrgo_map_style_is_visible();

    printf("All map style tests passed!\n");
    return 0;
}
