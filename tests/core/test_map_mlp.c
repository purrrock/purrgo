#include "purrgo/map.h"
#include "../../src/core/map_mlp.h"
#include "purrgo/fs_hal.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

static bool mock_seek_fail(void* handle, uint32_t offset) {
    return false;
}

static uint32_t mock_read_fail(void* handle, void* buffer, uint32_t size) {
    return 0;
}

void test_map_mlp_iter_init_invalid_seek() {
    map_mlp_iter_t iter;
    purrgo_fs_t mlp_fs = { .handle = NULL, .read = mock_read_fail, .seek = mock_seek_fail };

    bool result = map_mlp_iter_init(&iter, &mlp_fs, 0);
    assert(!result);
}

int main() {
    test_map_mlp_iter_init_invalid_seek();
    printf("map_mlp tests passed!\n");
    return 0;
}
