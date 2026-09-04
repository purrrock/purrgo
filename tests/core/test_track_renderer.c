#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "purrgo/track_renderer.h"
#include "purrgo/gfx_renderer.h"
#include "purrgo/track_logger.h"
#include "purrgo/map.h"

// Since we are linking against purrgo_core, purrgo_logger_get_track_points is already provided.
// To satisfy "Requires mocking purrgo_logger_get_track_points to return 0", we can either rely on its default state
// (which returns 0 if we haven't added points), OR we can use the preprocessor trick in CMake,
// OR since it's testing purrgo_track_render we can just make sure the track logger has 0 points.
// Actually, let's just make sure track_logger has 0 points initially. The track logger RAM buffer starts empty (BSS).
// So `purrgo_logger_get_track_points` will naturally return 0.
// Let's create a custom `gfx_draw_pixel` and `gfx_read_pixel` and check if `gfx_set_color` modifies the color.

static int draw_pixel_calls = 0;

static void draw_pixel(void *fb, int16_t x, int16_t y, gfx_color_t color) {
    draw_pixel_calls++;
}

static gfx_color_t read_pixel(void *fb, int16_t x, int16_t y) {
    return 0;
}

int main(void) {
    gfx_context_t gfx;
    gfx_init(&gfx, 100, 100, NULL, draw_pixel, read_pixel);

    // Initial color
    gfx.color_fg = 99; // Dummy value

    purrgo_bbox_t camera = {0};
    purrgo_viewport_t vp = {0};

    // Make sure logger has no points
    purrgo_logger_stop();

    // Render track
    purrgo_track_render(&gfx, &camera, &vp, "dummy.gpx");

    // The test requires that gfx is untouched if num_points == 0
    if (gfx.color_fg != 99) {
        printf("FAILED: gfx_set_color was called (color_fg changed to %d)!\n", gfx.color_fg);
        return 1;
    }

    if (draw_pixel_calls > 0) {
        printf("FAILED: draw_pixel was called!\n");
        return 1;
    }

    printf("All tests passed!\n");
    return 0;
}
