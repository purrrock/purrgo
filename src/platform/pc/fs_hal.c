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

uint32_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, uint32_t size) {
    if (!file || !buffer) return 0;
    /*
     * Явное приведение аргументов и возвращаемого значения (size_t <-> uint32_t).
     * Стандартные функции fread/fwrite возвращают size_t (64-бит на x64).
     * На платформе STM32 FatFs оперирует типом UINT (32-бит).
     * Приведение типов гарантирует идентичное поведение кода на ПК и микроконтроллере.
     */
    return (uint32_t)fread(buffer, 1, (size_t)size, (FILE*)file);
}

uint32_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* data, uint32_t size) {
    if (!file || !data) return 0;
    return (uint32_t)fwrite(data, 1, (size_t)size, (FILE*)file);
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