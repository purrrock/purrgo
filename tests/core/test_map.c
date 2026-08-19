#include "purrgo/map.h"
#include "purrgo/gfx_renderer.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

#define MOCK_BUFFER_SIZE 1024

typedef struct {
    uint8_t buffer[MOCK_BUFFER_SIZE];
    uint32_t size;
    uint32_t offset;
    uint32_t last_seek_offset;
    bool seek_called;
} mock_file_t;

static uint32_t mock_read(void* handle, void* buffer, uint32_t size) {
    mock_file_t* file = (mock_file_t*)handle;
    if (file->offset + size > file->size) {
        size = file->size - file->offset;
    }
    memcpy(buffer, file->buffer + file->offset, size);
    file->offset += size;
    return size;
}

static bool mock_seek(void* handle, uint32_t offset) {
    mock_file_t* file = (mock_file_t*)handle;
    file->seek_called = true;
    file->last_seek_offset = offset;
    if (offset <= file->size) {
        file->offset = offset;
        return true;
    }
    return false;
}

// Mock GFX line drawing
static int mock_line_count = 0;
static void mock_draw_line(void *fb, int16_t x, int16_t y, gfx_color_t color) {
    // Should not be called directly since we mock gfx context draw pixel or just check lines drawn
}
// But map.c uses gfx_draw_line which we need to mock or stub if we don't compile with real gfx_line.c
// Oh wait, gfx_draw_line is linked. Let's just provide a dummy draw_pixel for the context.
static void dummy_draw_pixel(void *fb, int16_t x, int16_t y, gfx_color_t color) {
    mock_line_count++;
}

static void pack_u32_le(uint8_t* buf, uint32_t val) {
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
    buf[3] = (val >> 24) & 0xFF;
}

static void pack_float_le(uint8_t* buf, float val) {
    union {
        float f;
        uint32_t i;
    } u;
    u.f = val;
    pack_u32_le(buf, u.i);
}


// Test 3: SQT Parsing empty and valid lists
void test_sqt_parsing() {
    mock_file_t idx_mock = {0};
    mock_file_t mlp_mock = {0};

    purrgo_fs_t idx_fs = { .handle = &idx_mock, .read = mock_read, .seek = mock_seek };
    purrgo_fs_t mlp_fs = { .handle = &mlp_mock, .read = mock_read, .seek = mock_seek };

    gfx_context_t gfx;
    gfx_init(&gfx, 100, 100, NULL, dummy_draw_pixel);

    purrgo_bbox_t cam = { .min_x = 0, .min_y = 0, .max_x = 100000000, .max_y = 100000000 };
    purrgo_viewport_t vp = { .width = 100, .height = 100 };

    memcpy(&idx_mock.buffer[0], "YZL\0", 4);

    memcpy(&idx_mock.buffer[32], "SQT\x01", 4);
    pack_u32_le(&idx_mock.buffer[32 + 4], 0x00000001); // Topo marker
    pack_u32_le(&idx_mock.buffer[32 + 8], 0); // Mode
    pack_u32_le(&idx_mock.buffer[32 + 12], 1); // Count = 1

    // 1 Data node
    pack_float_le(&idx_mock.buffer[48 + 0], 1.0f); // xmin
    pack_float_le(&idx_mock.buffer[48 + 4], 1.0f); // ymin
    pack_float_le(&idx_mock.buffer[48 + 8], 2.0f); // xmax
    pack_float_le(&idx_mock.buffer[48 + 12], 2.0f); // ymax
    pack_u32_le(&idx_mock.buffer[48 + 16], 1234); // obj_type
    pack_u32_le(&idx_mock.buffer[48 + 20], 0); // v1
    pack_u32_le(&idx_mock.buffer[48 + 24], 0); // v2

    idx_mock.size = 32 + 16 + 28;
    idx_mock.offset = 0;

    purrgo_map_render_layer(&idx_fs, &mlp_fs, &gfx, &cam, &vp);
    assert(idx_mock.offset == 32 + 16 + 28);
}

int main(void) {
    test_sqt_parsing();

    // original code from main
    mock_file_t idx_mock = {0};
    mock_file_t mlp_mock = {0};

    purrgo_fs_t idx_fs = { .handle = &idx_mock, .read = mock_read, .seek = mock_seek };
    purrgo_fs_t mlp_fs = { .handle = &mlp_mock, .read = mock_read, .seek = mock_seek };

    gfx_context_t gfx;
    gfx_init(&gfx, 100, 100, NULL, dummy_draw_pixel);

    purrgo_bbox_t cam = { .min_x = 0, .min_y = 0, .max_x = 100000000, .max_y = 100000000 };
    purrgo_viewport_t vp = { .width = 100, .height = 100 };

    // Test 1: Empty YZL and SQT
    memcpy(&idx_mock.buffer[0], "YZL\0", 4);
    idx_mock.size = 32 + 16;
    idx_mock.offset = 0;

    memcpy(&idx_mock.buffer[32], "SQT\x01", 4);
    pack_u32_le(&idx_mock.buffer[32 + 4], 0x00000001); // Topo marker
    pack_u32_le(&idx_mock.buffer[32 + 8], 0); // Mode
    pack_u32_le(&idx_mock.buffer[32 + 12], 0); // Count = 0

    purrgo_map_render_layer(&idx_fs, &mlp_fs, &gfx, &cam, &vp);
    assert(idx_mock.offset == 48);

    // Test 2: Nav Node Culling (v3_jump)
    idx_mock.size = 32 + 16 + 28;
    idx_mock.offset = 0;
    idx_mock.seek_called = false;

    memcpy(&idx_mock.buffer[32], "SQT\x01", 4);
    pack_u32_le(&idx_mock.buffer[32 + 4], 0x00000001);
    pack_u32_le(&idx_mock.buffer[32 + 8], 1); // Mode = 1 (Nav Node)
    pack_u32_le(&idx_mock.buffer[32 + 12], 1); // Count = 1

    // Create Nav Node fully outside camera
    pack_u32_le(&idx_mock.buffer[48 + 0], 100); // v3_jump = 100
    pack_float_le(&idx_mock.buffer[48 + 4], 20.0f); // c_xmin = 20 (outside 0-10)
    pack_float_le(&idx_mock.buffer[48 + 8], 20.0f); // c_ymin
    pack_float_le(&idx_mock.buffer[48 + 12], 30.0f); // c_xmax
    pack_float_le(&idx_mock.buffer[48 + 16], 30.0f); // c_ymax
    pack_u32_le(&idx_mock.buffer[48 + 20], 1); // nav_level
    pack_u32_le(&idx_mock.buffer[48 + 24], 5); // obj_count

    purrgo_map_render_layer(&idx_fs, &mlp_fs, &gfx, &cam, &vp);
    assert(idx_mock.seek_called == true);
    assert(idx_mock.last_seek_offset == (48 + 28) + (100 - 8)); // current offset after read + jump

    printf("Tests passed!\n");
    return 0;
}
