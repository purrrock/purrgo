#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "purrgo/track_renderer.h"
#include "purrgo/track_logger.h"
#include "purrgo/fs_hal.h"
#include "purrgo/gfx_renderer.h"
#include "purrgo/logger.h"
#include "purrgo/config.h"

// Define a simple framebuffer to act as our screen
#define WIDTH 128
#define HEIGHT 128
static uint8_t framebuffer[WIDTH * HEIGHT];
static int pixels_drawn = 0;

static void draw_pixel(void *user_data, int16_t x, int16_t y, gfx_color_t color) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        if (framebuffer[y * WIDTH + x] == 255 && color != 255) {
            pixels_drawn++;
        }
        framebuffer[y * WIDTH + x] = (uint8_t)color;
    }
}

static gfx_color_t read_pixel(void *user_data, int16_t x, int16_t y) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        return framebuffer[y * WIDTH + x];
    }
    return 0;
}

static void reset_framebuffer(gfx_context_t *ctx) {
    memset(framebuffer, 255, sizeof(framebuffer));
    pixels_drawn = 0;
    ctx->color_bg = 255;
    ctx->color_fg = 0;
}

// -------------------------------------------------------------
// Mock filesystem for track logger so it can start successfully
// -------------------------------------------------------------
struct purrgo_file_s {
    int dummy;
};
static struct purrgo_file_s mock_file;
static bool mock_file_opened = false;

purrgo_file_t* purrgo_fs_open(const char* path, uint32_t mode) {
    mock_file_opened = true;
    return &mock_file;
}

uint32_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, uint32_t size) { return 0; }
uint32_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* buffer, uint32_t size) { return size; }
bool purrgo_fs_seek(purrgo_file_t* file, uint32_t offset) { return true; }
void purrgo_fs_close(purrgo_file_t* file) { mock_file_opened = false; }
void purrgo_fs_sync(purrgo_file_t* file) {}
void purrgo_logger_log(const char* level, const char* file, int line, const char* format, ...) {}
// -------------------------------------------------------------


static int num_failures = 0;
#define EXPECT_TRUE(cond) if (!(cond)) { printf("FAIL: %s at %d\n", #cond, __LINE__); num_failures++; }
#define EXPECT_FALSE(cond) if (cond) { printf("FAIL: %s at %d\n", #cond, __LINE__); num_failures++; }
#define EXPECT_EQ(exp, act) if ((exp) != (act)) { printf("FAIL: expected %d, got %d at %d\n", (int)(exp), (int)(act), __LINE__); num_failures++; }

void test_null_arguments() {
    printf("test_null_arguments\n");
    gfx_context_t ctx;
    purrgo_bbox_t cam = {0};
    purrgo_viewport_t vp = {0};

    // Should not crash
    purrgo_track_render(NULL, &cam, &vp, "test.gpx");
    purrgo_track_render(&ctx, NULL, &vp, "test.gpx");
    purrgo_track_render(&ctx, &cam, NULL, "test.gpx");
    purrgo_track_render(&ctx, &cam, &vp, NULL);
}

void test_empty_track() {
    printf("test_empty_track\n");

    // Ensure logger is stopped and has no points
    purrgo_logger_stop();

    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    purrgo_bbox_t cam = { -10000000, -10000000, 10000000, 10000000 };
    purrgo_viewport_t vp = { WIDTH, HEIGHT, 0, 0 };

    purrgo_track_render(&ctx, &cam, &vp, "dummy.gpx");

    EXPECT_EQ(0, pixels_drawn);
}

void test_render_track_points() {
    printf("test_render_track_points\n");

    // Stop and restart logger to ensure clean slate
    purrgo_logger_stop();

    // Set logger mode to standard to ensure distance thresholds apply
    purrgo_logger_set_mode(LOGGER_MODE_STANDARD);

    // Setup tz offset because logger relies on app_config
    app_config.tz_offset_minutes = 0;

    // Start logger
    purrgo_gnss_solution_t fix1 = {0};
    fix1.valid = true;
    fix1.year = 24; fix1.month = 1; fix1.day = 1;
    fix1.hours = 12; fix1.minutes = 0; fix1.seconds = 0;
    fix1.lat_1e7 = 0;
    fix1.lon_1e7 = 0;
    EXPECT_TRUE(purrgo_logger_start(&fix1));
    EXPECT_TRUE(purrgo_logger_add_point(&fix1)); // First point always added

    // Wait! A single point doesn't draw a line since `has_prev` will be true only on the second point.
    // Let's add a second point to draw a line.
    purrgo_gnss_solution_t fix2 = fix1;
    fix2.lat_1e7 = 100000; // ~11 km, easily > 5m
    fix2.lon_1e7 = 100000;
    // Track logger adds point only if moved by some distance. Let's make sure it does.
    EXPECT_TRUE(purrgo_logger_add_point(&fix2));

    purrgo_gnss_solution_t fix3 = fix2;
    fix3.lat_1e7 = 200000; // ~11 km further
    fix3.lon_1e7 = 200000;
    EXPECT_TRUE(purrgo_logger_add_point(&fix3));

    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_framebuffer(&ctx);

    // Check how many points are in the logger
    track_point_t pts[10];
    size_t count = purrgo_logger_get_track_points(pts, 10);
    EXPECT_EQ(3, (int)count);

    // Check what the projected coordinates should be
    purrgo_bbox_t cam = { -100000, -100000, 300000, 300000 };
    purrgo_viewport_t vp = { WIDTH, HEIGHT, 0, 0 };

    purrgo_track_render(&ctx, &cam, &vp, "dummy.gpx");

    // The line should be drawn.
    EXPECT_TRUE(pixels_drawn > 0);
    printf("Pixels drawn: %d\n", pixels_drawn);

    purrgo_logger_stop();
}

int main(void) {
    test_null_arguments();
    test_empty_track();
    test_render_track_points();

    if (num_failures > 0) {
        printf("FAILED %d tests.\n", num_failures);
        return 1;
    }
    printf("All track_renderer tests passed.\n");
    return 0;
}
