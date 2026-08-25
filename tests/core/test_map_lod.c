#include "purrgo/map.h"
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

static purrgo_map_scale_t mock_zoom_level = PURRGO_MAP_SCALE_500M;
purrgo_map_scale_t purrgo_app_get_map_zoom_level(void) {
    return mock_zoom_level;
}

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

    // PGO header (32 bytes)
    uint8_t pgo[32] = {'P','G','O'};

    // We append the payload size so map.c can correctly bound max_idx_offset
    // Payload size = LOD0 (41) + LOD1 (69) + LOD2 (41) = 151
    uint32_t payload_size = 151;
    pgo[4] = payload_size & 0xFF;
    pgo[5] = (payload_size >> 8) & 0xFF;
    pgo[6] = (payload_size >> 16) & 0xFF;
    pgo[7] = (payload_size >> 24) & 0xFF;

    append_bytes(pgo, 32);

    // LOD 0: Standard SQT block with one Data Node (41 bytes total)
    uint8_t sqt0[16] = {'S','Q','T', 0x01, 0,0,0,0, 0,0,0,0, 1,0,0,0}; // mode = 0, count = 1
    append_bytes(sqt0, 16);
    append_u32((uint32_t) -100); // xmin
    append_u32((uint32_t) -100); // ymin
    append_u32((uint32_t) 100);  // xmax
    append_u32((uint32_t) 100);  // ymax
    // Code (1 byte) + v1 (4 bytes) + v2 (4 bytes) = 9 bytes
    mock_idx_data[mock_idx_size++] = 0; // code
    append_u32(0);               // v1
    append_u32(0);               // v2

    // LOD 1: SQT block with a Nav Node to test subtree skipping (mode = 1)
    // SQT header: 16 bytes
    uint8_t sqt1[16] = {'S','Q','T', 0x01, 0,0,0,0, 1,0,0,0, 1,0,0,0}; // mode = 1, count = 1
    append_bytes(sqt1, 16);

    // Nav Node (28 bytes)
    // v3_jump = size of children (1 Data Node = 25) = 25
    append_u32(25);
    append_u32((uint32_t) -100); // xmin
    append_u32((uint32_t) -100); // ymin
    append_u32((uint32_t) 100);  // xmax
    append_u32((uint32_t) 100);  // ymax
    append_u32(0);               // level
    append_u32(1);               // child count

    // Child Data Node (25 bytes, skipped during Nav jump when inactive)
    append_u32((uint32_t) -10);
    append_u32((uint32_t) -10);
    append_u32((uint32_t) 10);
    append_u32((uint32_t) 10);
    mock_idx_data[mock_idx_size++] = 1; // code
    append_u32(0); // v1
    append_u32(0); // v2

    // LOD 2: Standard SQT block with one Data Node (41 bytes total)
    uint8_t sqt2[16] = {'S','Q','T', 0x01, 0,0,0,0, 0,0,0,0, 1,0,0,0}; // mode = 0, count = 1
    append_bytes(sqt2, 16);
    append_u32((uint32_t) -100);
    append_u32((uint32_t) -100);
    append_u32((uint32_t) 100);
    append_u32((uint32_t) 100);
    mock_idx_data[mock_idx_size++] = 2; // code
    append_u32(0); // v1
    append_u32(0); // v2
}

void setup_test_map_malformed_nav() {
    mock_idx_size = 0;
    mock_idx_pos = 0;

    // PGO header (32 bytes)
    uint8_t pgo[32] = {'P','G','O'};

    uint32_t payload_size = 151;
    pgo[4] = payload_size & 0xFF;
    pgo[5] = (payload_size >> 8) & 0xFF;
    pgo[6] = (payload_size >> 16) & 0xFF;
    pgo[7] = (payload_size >> 24) & 0xFF;

    append_bytes(pgo, 32);

    // LOD 0: Malformed NAV node
    uint8_t sqt[16] = {'S','Q','T', 0x01, 0,0,0,0, 1,0,0,0, 1,0,0,0}; // mode = 1, count = 1
    append_bytes(sqt, 16);

    append_u32(4000); // Malformed v3_jump causing out of bounds
    append_u32((uint32_t) -100); // xmin
    append_u32((uint32_t) -100); // ymin
    append_u32((uint32_t) 100);  // xmax
    append_u32((uint32_t) 100);  // ymax
    append_u32(0);               // level
    append_u32(1);               // child count
}

void test_lod_0() {
    setup_test_map();
    mock_zoom_level = PURRGO_MAP_SCALE_500M;
    mock_mlp_pos = 0;

    purrgo_fs_t idx = { .handle = NULL, .read = mock_read, .seek = mock_seek };
    purrgo_fs_t mlp = { .handle = NULL, .read = mock_mlp_read, .seek = mock_mlp_seek };

    gfx_context_t gfx;
    purrgo_bbox_t cam = { -200, -200, 200, 200 };
    purrgo_viewport_t vp = { 0, 0, 100, 100 };

    purrgo_map_render_layer(&idx, &mlp, &gfx, &cam, &vp, false);

    // PGO (32) + LOD0 SQT (16) + LOD0 Data Node (25) = 73
    assert(mock_idx_pos == 73);
    // MLP never read
    assert(mock_mlp_pos == 0);
}

void test_lod_1_1km() {
    setup_test_map();
    mock_zoom_level = PURRGO_MAP_SCALE_1KM;
    mock_mlp_pos = 0;

    purrgo_fs_t idx = { .handle = NULL, .read = mock_read, .seek = mock_seek };
    purrgo_fs_t mlp = { .handle = NULL, .read = mock_mlp_read, .seek = mock_mlp_seek };
    gfx_context_t gfx;
    purrgo_bbox_t cam = { -200, -200, 200, 200 };
    purrgo_viewport_t vp = { 0, 0, 100, 100 };
    purrgo_map_render_layer(&idx, &mlp, &gfx, &cam, &vp, false);

    // PGO (32) + LOD0 Skip (16+25=41) + LOD1 SQT (16) + LOD1 Nav Node (28) + LOD1 Child Node (25) = 142
    assert(mock_idx_pos == 142);
    assert(mock_mlp_pos == 0);
}

void test_lod_1_5km() {
    setup_test_map();
    mock_zoom_level = PURRGO_MAP_SCALE_5KM;
    mock_mlp_pos = 0;

    purrgo_fs_t idx = { .handle = NULL, .read = mock_read, .seek = mock_seek };
    purrgo_fs_t mlp = { .handle = NULL, .read = mock_mlp_read, .seek = mock_mlp_seek };
    gfx_context_t gfx;
    purrgo_bbox_t cam = { -200, -200, 200, 200 };
    purrgo_viewport_t vp = { 0, 0, 100, 100 };
    purrgo_map_render_layer(&idx, &mlp, &gfx, &cam, &vp, false);

    assert(mock_idx_pos == 142);
    assert(mock_mlp_pos == 0);
}

void test_lod_2_10km() {
    setup_test_map();
    mock_zoom_level = PURRGO_MAP_SCALE_10KM;
    mock_mlp_pos = 0;

    purrgo_fs_t idx = { .handle = NULL, .read = mock_read, .seek = mock_seek };
    purrgo_fs_t mlp = { .handle = NULL, .read = mock_mlp_read, .seek = mock_mlp_seek };
    gfx_context_t gfx;
    purrgo_bbox_t cam = { -200, -200, 200, 200 };
    purrgo_viewport_t vp = { 0, 0, 100, 100 };
    purrgo_map_render_layer(&idx, &mlp, &gfx, &cam, &vp, false);

    // PGO (32) + LOD0 Skip (41) + LOD1 Skip (16+28 + v3_jump_seek(25) = 69) + LOD2 SQT (16) + LOD2 Data Node (25) = 183
    assert(mock_idx_pos == 183);
    assert(mock_mlp_pos == 0);
}

void test_malformed_v3_jump() {
    setup_test_map_malformed_nav();
    mock_zoom_level = PURRGO_MAP_SCALE_10KM; // Try to skip LOD 0 which is malformed
    mock_mlp_pos = 0;

    purrgo_fs_t idx = { .handle = NULL, .read = mock_read, .seek = mock_seek };
    purrgo_fs_t mlp = { .handle = NULL, .read = mock_mlp_read, .seek = mock_mlp_seek };
    gfx_context_t gfx;
    purrgo_bbox_t cam = { -200, -200, 200, 200 };
    purrgo_viewport_t vp = { 0, 0, 100, 100 };

    // Should abort immediately without crashing
    purrgo_map_render_layer(&idx, &mlp, &gfx, &cam, &vp, false);

    // It stopped at PGO (32) + SQT (16) + Nav Node (28) = 76 before aborting due to failed seek
    assert(mock_idx_pos == 76);
}

void test_regression_v3_jump_is_exact() {
    setup_test_map();
    mock_zoom_level = PURRGO_MAP_SCALE_10KM;

    // We want to test that a culled Nav Node skips EXACTLY v3_jump bytes.
    // In setup_test_map(), the LOD1 Nav Node has:
    // v3_jump = 25
    // obj_count = 1
    // It contains 1 child Data Node of 25 bytes.
    // So for level-0 children, v3_jump == child_count * 25
    // 25 == 1 * 25.

    // Since cam is far away, the LOD 1 Nav Node will fail intersection and jump.
    purrgo_fs_t idx = { .handle = NULL, .read = mock_read, .seek = mock_seek };
    purrgo_fs_t mlp = { .handle = NULL, .read = mock_mlp_read, .seek = mock_mlp_seek };
    gfx_context_t gfx;

    // Camera way outside the mock node bbox
    purrgo_bbox_t cam = { 500, 500, 1000, 1000 };
    purrgo_viewport_t vp = { 0, 0, 100, 100 };
    purrgo_map_render_layer(&idx, &mlp, &gfx, &cam, &vp, false);

    // Check that it reaches the exact same final offset successfully
    assert(mock_idx_pos == 183);
}

int main() {
    test_lod_0();
    test_lod_1_1km();
    test_lod_1_5km();
    test_lod_2_10km();
    test_malformed_v3_jump();
    test_regression_v3_jump_is_exact();

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
