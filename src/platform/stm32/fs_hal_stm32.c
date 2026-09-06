#include "purrgo/fs_hal.h"

/*
 * STM32 Stub Implementation of the filesystem HAL.
 *
 * This implementation acts as a placeholder when no physical
 * SD card or filesystem backend is available on the hardware.
 * It simulates an empty/unavailable filesystem.
 */

purrgo_file_t* purrgo_fs_open(const char* filepath, fs_mode_t mode) {
    /* No storage available to open files */
    (void)filepath;
    (void)mode;
    return NULL;
}

uint32_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* data, uint32_t size) {
    /* Write fails, 0 bytes written */
    (void)file;
    (void)data;
    (void)size;
    return 0;
}

uint32_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, uint32_t size) {
    /* Read fails, 0 bytes read */
    (void)file;
    (void)buffer;
    (void)size;
    return 0;
}

bool purrgo_fs_seek(purrgo_file_t* file, uint32_t offset) {
    /* Cannot seek an invalid file */
    (void)file;
    (void)offset;
    return false;
}

void purrgo_fs_sync(purrgo_file_t* file) {
    /* No-op for stub */
    (void)file;
}

void purrgo_fs_close(purrgo_file_t* file) {
    /* No-op for stub */
    (void)file;
}

purrgo_dir_t* purrgo_fs_opendir(const char* path) {
    /* No directories exist in the stub */
    (void)path;
    return NULL;
}

bool purrgo_fs_readdir(purrgo_dir_t* dir, purrgo_fs_dirent_t* dirent) {
    /* No entries to read */
    (void)dir;
    (void)dirent;
    return false;
}

void purrgo_fs_closedir(purrgo_dir_t* dir) {
    /* No-op for stub */
    (void)dir;
}
