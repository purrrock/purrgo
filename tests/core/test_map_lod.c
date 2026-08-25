#include "purrgo/map.h"
#include "purrgo/app_fsm.h"
#include "..//../src/core/map_idx.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

static uint8_t mock_idx_data[1024];
static uint32_t mock_idx_size = 0;
static uint32_t mock_idx_pos = 0;

static uint32_t mock_read(void* handle, void* buffer, uint32_t size) {
    if (mock_idx_pos >= mock_idx_size) return 0;
    uint32_t to_read = size;
    if (mock_idx_pos + to_read > mock_idx_size) {
        to_read = mock_idx_size - mock_idx_pos;
    }
    memcpy(buffer, &mock_idx_data[mock_idx_pos], to_read);
    mock_idx_pos += to_read;
    return to_read;
}

static bool mock_seek(void* handle, uint32_t offset) {
    if (offset > mock_idx_size) return false;
    mock_idx_pos = offset;
    return true;
}

static int render_calls = 0;
void mock_draw_pixel(void *fb, int16_t x, int16_t y, gfx_color_t color) {
    render_calls++;
}

static purrgo_map_scale_t mock_zoom_level = PURRGO_MAP_SCALE_500M;
purrgo_map_scale_t purrgo_app_get_map_zoom_level(void) {
    return mock_zoom_level;
}

// Ensure other mock stubs exist for what map_idx needs to not crash during rendering tests
// e.g., the map render mock that calls draw_pixel etc. But since we use map_idx_parse_node which calls map_render_feature,
// we just provide a basic mock geometry.

static uint8_t mock_mlp_data[1024];
static uint32_t mock_mlp_size = 0;
static uint32_t mock_mlp_pos = 0;

static uint32_t mock_mlp_read(void* handle, void* buffer, uint32_t size) {
    if (mock_mlp_pos >= mock_mlp_size) return 0;
    uint32_t to_read = size;
    if (mock_mlp_pos + to_read > mock_mlp_size) {
        to_read = mock_mlp_size - mock_mlp_pos;
    }
    memcpy(buffer, &mock_mlp_data[mock_mlp_pos], to_read);
    mock_mlp_pos += to_read;
    return to_read;
}

static bool mock_mlp_seek(void* handle, uint32_t offset) {
    if (offset > mock_mlp_size) return false;
    mock_mlp_pos = offset;
    return true;
}

// Helper to write to idx stream
void append_u32(uint32_t val) {
    mock_idx_data[mock_idx_size++] = val & 0xFF;
    mock_idx_data[mock_idx_size++] = (val >> 8) & 0xFF;
    mock_idx_data[mock_idx_size++] = (val >> 16) & 0xFF;
    mock_idx_data[mock_idx_size++] = (val >> 24) & 0xFF;
}

void append_bytes(const uint8_t* b, uint32_t size) {
    memcpy(&mock_idx_data[mock_idx_size], b, size);
    mock_idx_size += size;
}

void setup_test_map() {
    mock_idx_size = 0;
    mock_idx_pos = 0;

    // YZL header (32 bytes)
    uint8_t yzl[32] = {'Y','Z','L'};
    append_bytes(yzl, 32);

    // Three SQT blocks (LOD 0, 1, 2)
    for (int i = 0; i < 3; i++) {
        uint8_t sqt[16] = {'S','Q','T', 0x01}; // magic
        // Set mode = 0 (data nodes), count = 1
        sqt[8] = 0; sqt[9] = 0; sqt[10] = 0; sqt[11] = 0; // mode
        sqt[12] = 1; sqt[13] = 0; sqt[14] = 0; sqt[15] = 0; // count

        append_bytes(sqt, 16);

        // One Data Node (28 bytes)
        // bbox (xmin, ymin, xmax, ymax)
        append_u32((uint32_t) -100);
        append_u32((uint32_t) -100);
        append_u32((uint32_t) 100);
        append_u32((uint32_t) 100);

        // code = i (to identify LOD)
        append_u32(i);
        // v1 = 0, v2 = 0
        append_u32(0);
        append_u32(0);
    }
}

// Track visited codes
int visited_codes[10];
int visited_count = 0;

// Override purrgo_map_style_from_feature for the test to just track what we visited
#include "purrgo/map_style.h"

purrgo_map_style_t purrgo_map_style_from_feature(uint32_t code) {
    if (visited_count < 10) {
        visited_codes[visited_count++] = code;
    }
    return PURRGO_STYLE_NONE; // return none to avoid full render loop
}

// Let's replace purrgo_map_style_from_feature properly by overriding it.
// The real one is in map_style.c, but if we link map_style.c it might complain about multiple definitions.
// Let's just inspect diag.data_visited? Wait, diag counts total nodes visited.
// But we want to know WHICH LOD was visited.
// We can use a trick: the diagnostic struct counts nodes visited, but we can't get the code.
// Instead, since the Data Node bbox contains xmin, etc., maybe we can just make `purrgo_map_render_layer` run
// and since it's a test, we can just intercept `purrgo_app_get_map_zoom_level`.
// Since we used `map.c` which calls `map_idx_parse_node` which calls `purrgo_map_style_from_feature`,
// if we link to map.o and map_idx.o, we DO link to map_style.o. So we shouldn't redefine it.
// Let's verify by checking the diag output or by counting the bytes processed.
// Actually, `mock_idx_pos` tells us how far it read!
// LOD 0 uses 1st block: YZL (32) + SQT (16) + Node (28) = 76 bytes.
// LOD 1 uses 1st block skip (16+28), 2nd block parse (16+28) = 32 + 44 + 44 = 120 bytes.
// LOD 2 uses 1st skip, 2nd skip, 3rd parse = 32 + 44 + 44 + 44 = 164 bytes.

void test_lod_0() {
    setup_test_map();
    mock_zoom_level = PURRGO_MAP_SCALE_500M;

    purrgo_fs_t idx = { .handle = NULL, .read = mock_read, .seek = mock_seek };
    purrgo_fs_t mlp = { .handle = NULL, .read = mock_mlp_read, .seek = mock_mlp_seek };

    gfx_context_t gfx;
    purrgo_bbox_t cam = { -200, -200, 200, 200 };
    purrgo_viewport_t vp = { 0, 0, 100, 100 };

    purrgo_map_render_layer(&idx, &mlp, &gfx, &cam, &vp, false);

    assert(mock_idx_pos == 76);
}

void test_lod_1_1km() {
    setup_test_map();
    mock_zoom_level = PURRGO_MAP_SCALE_1KM;
    purrgo_fs_t idx = { .handle = NULL, .read = mock_read, .seek = mock_seek };
    purrgo_fs_t mlp = { .handle = NULL, .read = mock_mlp_read, .seek = mock_mlp_seek };
    gfx_context_t gfx;
    purrgo_bbox_t cam = { -200, -200, 200, 200 };
    purrgo_viewport_t vp = { 0, 0, 100, 100 };
    purrgo_map_render_layer(&idx, &mlp, &gfx, &cam, &vp, false);
    assert(mock_idx_pos == 120);
}

void test_lod_1_5km() {
    setup_test_map();
    mock_zoom_level = PURRGO_MAP_SCALE_5KM;
    purrgo_fs_t idx = { .handle = NULL, .read = mock_read, .seek = mock_seek };
    purrgo_fs_t mlp = { .handle = NULL, .read = mock_mlp_read, .seek = mock_mlp_seek };
    gfx_context_t gfx;
    purrgo_bbox_t cam = { -200, -200, 200, 200 };
    purrgo_viewport_t vp = { 0, 0, 100, 100 };
    purrgo_map_render_layer(&idx, &mlp, &gfx, &cam, &vp, false);
    assert(mock_idx_pos == 120);
}

void test_lod_2_10km() {
    setup_test_map();
    mock_zoom_level = PURRGO_MAP_SCALE_10KM;
    purrgo_fs_t idx = { .handle = NULL, .read = mock_read, .seek = mock_seek };
    purrgo_fs_t mlp = { .handle = NULL, .read = mock_mlp_read, .seek = mock_mlp_seek };
    gfx_context_t gfx;
    purrgo_bbox_t cam = { -200, -200, 200, 200 };
    purrgo_viewport_t vp = { 0, 0, 100, 100 };
    purrgo_map_render_layer(&idx, &mlp, &gfx, &cam, &vp, false);
    assert(mock_idx_pos == 164);
}

int main() {
    test_lod_0();
    test_lod_1_1km();
    test_lod_1_5km();
    test_lod_2_10km();

    printf("LOD tests passed!\n");
    return 0;
}
purrgo_dir_t* purrgo_fs_opendir(const char* path) { return NULL; }
bool purrgo_fs_readdir(purrgo_dir_t* dir, purrgo_fs_dirent_t* entry) { return false; }
void purrgo_fs_closedir(purrgo_dir_t* dir) {}
purrgo_file_t* purrgo_fs_open(const char* path, fs_mode_t mode) { return NULL; }
uint32_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, uint32_t size) { return 0; }
uint32_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* buffer, uint32_t size) { return size; }
void purrgo_fs_close(purrgo_file_t* file) {}
void purrgo_fs_sync(purrgo_file_t* file) {}
bool purrgo_fs_seek(purrgo_file_t* file, uint32_t offset) { return false; }
