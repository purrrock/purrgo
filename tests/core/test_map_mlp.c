#include "purrgo/map.h"
#include "../../src/core/map_mlp.c"
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

void test_validate_parts_edge_cases() {
    assert(validate_parts(NULL, 0, 10) == true);

    uint32_t parts2[] = {0, 5};
    assert(validate_parts(parts2, 2, 10) == true);

    uint32_t parts3[] = {1, 5};
    assert(validate_parts(parts3, 2, 10) == false); // First element not 0

    uint32_t parts4[] = {0, 5, 5};
    assert(validate_parts(parts4, 3, 10) == false); // Not strictly increasing

    uint32_t parts5[] = {0, 5, 3};
    assert(validate_parts(parts5, 3, 10) == false); // Not increasing

    uint32_t parts6[] = {0, 5, 10};
    assert(validate_parts(parts6, 3, 10) == false); // Element >= num_points

    uint32_t parts7[] = {0, 11};
    assert(validate_parts(parts7, 2, 10) == false); // Element >= num_points

    printf("test_validate_parts_edge_cases passed\n");
}

int main() {
    test_map_mlp_iter_init_invalid_seek();
    test_validate_parts_edge_cases();
    printf("map_mlp tests passed!\n");
    return 0;
}
