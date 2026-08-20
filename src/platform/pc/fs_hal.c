#include "purrgo/fs_hal.h"
#include <stdio.h>

purrgo_file_t* purrgo_fs_open(const char* filepath, fs_mode_t mode) {
    const char* c_mode = "wb";
    if (mode == FS_READ) {
        c_mode = "rb";
    } else if (mode == FS_WRITE_APPEND) {
        c_mode = "ab";
    }
    // Приведение типа указателя FILE* ОС к платформонезависимому purrgo_file_t*
    return (purrgo_file_t*)fopen(filepath, c_mode);
}

size_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, size_t size) {
    if (!file || !buffer) return 0;
    return fread(buffer, 1, size, (FILE*)file);
}

size_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* data, size_t size) {
    if (!file || !data) return 0;
    return fwrite(data, 1, size, (FILE*)file);
}

bool purrgo_fs_seek(purrgo_file_t* file, uint32_t offset) {
    if (!file) return false;
    return fseek((FILE*)file, (long)offset, SEEK_SET) == 0;
}

void purrgo_fs_sync(purrgo_file_t* file) {
    if (file) fflush((FILE*)file);
}

void purrgo_fs_close(purrgo_file_t* file) {
    if (file) fclose((FILE*)file);
}