#include "purrgo/fs_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_fs_hal_open_missing_file_read(void) {
    // Attempt to open a non-existent file in read mode.
    // Expected behavior: purrgo_fs_open should return NULL.
    purrgo_file_t* file = purrgo_fs_open("non_existent_test_file_purrgo.txt", FS_READ);
    assert(file == NULL);
}

void test_fs_hal_open_invalid_path_write(void) {
    // Attempt to open a file in a non-existent directory for writing.
    // Expected behavior: purrgo_fs_open should return NULL.
    purrgo_file_t* file = purrgo_fs_open("/invalid_dir_purrgo/test.txt", FS_WRITE_CREATE);
    assert(file == NULL);
}

void test_fs_hal_open_invalid_path_append(void) {
    // Attempt to open a file in a non-existent directory for appending.
    // Expected behavior: purrgo_fs_open should return NULL.
    purrgo_file_t* file = purrgo_fs_open("/invalid_dir_purrgo/test.txt", FS_WRITE_APPEND);
    assert(file == NULL);
}

void test_fs_hal_open_valid_file_write_and_read(void) {
    // Create and write to a file, then read it back to verify functionality.
    const char* test_file = "test_fs_hal_temp.txt";
    const char* test_data = "Hello PurrGO";

    // Write
    purrgo_file_t* wfile = purrgo_fs_open(test_file, FS_WRITE_CREATE);
    assert(wfile != NULL);
    uint32_t written = purrgo_fs_write(wfile, (const uint8_t*)test_data, strlen(test_data));
    assert(written == strlen(test_data));
    purrgo_fs_sync(wfile);
    purrgo_fs_close(wfile);

    // Read
    purrgo_file_t* rfile = purrgo_fs_open(test_file, FS_READ);
    assert(rfile != NULL);
    char buf[32] = {0};
    uint32_t read_bytes = purrgo_fs_read(rfile, (uint8_t*)buf, sizeof(buf) - 1);
    assert(read_bytes == strlen(test_data));
    assert(strcmp(buf, test_data) == 0);
    purrgo_fs_close(rfile);

    // Cleanup
    remove(test_file);
}

int main(void) {
    test_fs_hal_open_missing_file_read();
    test_fs_hal_open_invalid_path_write();
    test_fs_hal_open_invalid_path_append();
    test_fs_hal_open_valid_file_write_and_read();

    printf("test_fs_hal passed\n");
    return 0;
}
