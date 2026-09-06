#include "purrgo/fs_hal.h"
#include <stddef.h>

purrgo_file_t* purrgo_fs_open(const char* filepath, fs_mode_t mode) {
    (void)filepath; (void)mode;
    return NULL;
}

uint32_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* data, uint32_t size) {
    (void)file; (void)data; (void)size;
    return 0;
}

uint32_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, uint32_t size) {
    (void)file; (void)buffer; (void)size;
    return 0;
}

bool purrgo_fs_seek(purrgo_file_t* file, uint32_t offset) {
    (void)file; (void)offset;
    return false;
}

void purrgo_fs_sync(purrgo_file_t* file) {
    (void)file;
}

void purrgo_fs_close(purrgo_file_t* file) {
    (void)file;
}

purrgo_dir_t* purrgo_fs_opendir(const char* path) {
    (void)path;
    return NULL;
}

bool purrgo_fs_readdir(purrgo_dir_t* dir, purrgo_fs_dirent_t* dirent) {
    (void)dir; (void)dirent;
    return false;
}

void purrgo_fs_closedir(purrgo_dir_t* dir) {
    (void)dir;
}
