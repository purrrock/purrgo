#include "purrgo/map.h"
#include "../../src/core/map_culling.h"
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

static bool test_normal_intersection() {
    printf("Running test_normal_intersection...\n");
    purrgo_bbox_t cam = { .min_x = 10, .max_x = 20, .min_y = 10, .max_y = 20 };

    // Inside
    assert(bbox_intersects_camera(12, 12, 18, 18, &cam));

    // Intersecting edges
    assert(bbox_intersects_camera(5, 5, 15, 15, &cam));
    assert(bbox_intersects_camera(15, 15, 25, 25, &cam));
    assert(bbox_intersects_camera(15, 5, 25, 15, &cam));
    assert(bbox_intersects_camera(5, 15, 15, 25, &cam));

    // Completely outside
    assert(!bbox_intersects_camera(0, 0, 5, 5, &cam));
    assert(!bbox_intersects_camera(25, 25, 30, 30, &cam));
    assert(!bbox_intersects_camera(15, 0, 25, 5, &cam));
    assert(!bbox_intersects_camera(0, 15, 5, 25, &cam));

    // Touching edges
    assert(bbox_intersects_camera(5, 5, 10, 10, &cam)); // touches corner (10, 10)
    assert(bbox_intersects_camera(20, 20, 25, 25, &cam)); // touches corner (20, 20)

    printf("PASSED test_normal_intersection\n");
    return true;
}

static bool test_dateline_crossing() {
    printf("Running test_dateline_crossing...\n");
    // Camera crosses dateline, meaning max_x < min_x
    // Say longitudes [-180, 180] maps to [0, 360]
    // Dateline is 0/360 or similar.
    // Let's assume dateline is crossed when min_x = 350, max_x = 10
    purrgo_bbox_t cam = { .min_x = 350, .max_x = 10, .min_y = 10, .max_y = 20 };

    // bbox entirely in the right part of the map [355, 360] -> max_x = 360
    assert(bbox_intersects_camera(355, 12, 360, 18, &cam));

    // bbox entirely in the left part of the map [0, 5] -> min_x = 0
    assert(bbox_intersects_camera(0, 12, 5, 18, &cam));

    // bbox outside, e.g., in the middle [100, 110]
    assert(!bbox_intersects_camera(100, 12, 110, 18, &cam));

    // bbox intersecting right edge of camera [345, 355]
    assert(bbox_intersects_camera(345, 12, 355, 18, &cam));

    // bbox intersecting left edge of camera [5, 15]
    assert(bbox_intersects_camera(5, 12, 15, 18, &cam));

    // bbox fully covering the dateline crossing [355, 5] (Wait, bbox is standard xmin < xmax in this context usually,
    // unless it also crosses. If bbox xmin < xmax is always true, let's test that).

    printf("PASSED test_dateline_crossing\n");
    return true;
}

static bool test_y_out_of_bounds() {
    printf("Running test_y_out_of_bounds...\n");
    purrgo_bbox_t cam = { .min_x = 10, .max_x = 20, .min_y = 10, .max_y = 20 };
    purrgo_bbox_t cam_cross = { .min_x = 350, .max_x = 10, .min_y = 10, .max_y = 20 };

    // X is fine, Y is completely below
    assert(!bbox_intersects_camera(12, 0, 18, 5, &cam));
    assert(!bbox_intersects_camera(355, 0, 360, 5, &cam_cross));

    // X is fine, Y is completely above
    assert(!bbox_intersects_camera(12, 25, 18, 30, &cam));
    assert(!bbox_intersects_camera(355, 25, 360, 30, &cam_cross));

    printf("PASSED test_y_out_of_bounds\n");
    return true;
}

static bool test_bbox_crossing_dateline();
static bool test_bbox_edges();

int main() {
    bool success = true;

    if (!test_normal_intersection()) success = false;
    if (!test_dateline_crossing()) success = false;
    if (!test_y_out_of_bounds()) success = false;
    if (!test_bbox_crossing_dateline()) success = false;
    if (!test_bbox_edges()) success = false;

    if (success) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}

static bool test_bbox_crossing_dateline() {
    printf("Running test_bbox_crossing_dateline...\n");
    // While the objects in map DB usually have xmin <= xmax, sometimes they might not.
    // If we assume standard representation (xmin <= xmax) for object bounding boxes,
    // the current logic is complete. Let's make sure we test xmin <= xmax thoroughly.

    // Normal cam, normal bbox
    purrgo_bbox_t cam = { .min_x = 10, .max_x = 20, .min_y = 10, .max_y = 20 };
    // Test case where bbox covers camera entirely
    assert(bbox_intersects_camera(0, 0, 30, 30, &cam));

    // Test case where bbox spans across, but is disjoint in Y
    assert(!bbox_intersects_camera(0, 0, 30, 5, &cam));

    // Camera crosses dateline
    purrgo_bbox_t cam_cross = { .min_x = 350, .max_x = 10, .min_y = 10, .max_y = 20 };

    // bbox spans exactly the dateline gap
    assert(!bbox_intersects_camera(15, 12, 345, 18, &cam_cross));

    // bbox spans exactly the whole world
    assert(bbox_intersects_camera(0, 12, 360, 18, &cam_cross));

    printf("PASSED test_bbox_crossing_dateline\n");
    return true;
}

static bool test_bbox_edges() {
    printf("Running test_bbox_edges...\n");

    // Normal cam
    purrgo_bbox_t cam = { .min_x = 10, .max_x = 20, .min_y = 10, .max_y = 20 };

    // Inside edges
    assert(bbox_intersects_camera(10, 10, 20, 20, &cam));
    assert(bbox_intersects_camera(10, 12, 12, 18, &cam));
    assert(bbox_intersects_camera(18, 12, 20, 18, &cam));
    assert(bbox_intersects_camera(12, 10, 18, 12, &cam));
    assert(bbox_intersects_camera(12, 18, 18, 20, &cam));

    // Outside edges
    assert(bbox_intersects_camera(0, 0, 10, 10, &cam));
    assert(bbox_intersects_camera(20, 20, 30, 30, &cam));
    assert(!bbox_intersects_camera(0, 0, 9, 9, &cam));
    assert(!bbox_intersects_camera(21, 21, 30, 30, &cam));

    printf("PASSED test_bbox_edges\n");
    return true;
}
