#ifndef PURRGO_FS_HAL_H
#define PURRGO_FS_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct purrgo_file_s purrgo_file_t;
typedef struct purrgo_dir_s purrgo_dir_t;

#define PURRGO_FS_MAX_PATH 256

typedef struct {
    char name[PURRGO_FS_MAX_PATH];
    bool is_directory;
} purrgo_fs_dirent_t;

typedef enum {
    FS_READ,            // Добавлен режим чтения
    FS_WRITE_CREATE,
    FS_WRITE_APPEND
} fs_mode_t;

purrgo_file_t* purrgo_fs_open(const char* filepath, fs_mode_t mode);
uint32_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* data, uint32_t size);
// Добавлена функция чтения (уже использовалась в прототипе gpx_parser)
uint32_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, uint32_t size);
bool purrgo_fs_seek(purrgo_file_t* file, uint32_t offset);
void purrgo_fs_sync(purrgo_file_t* file);
void purrgo_fs_close(purrgo_file_t* file);

purrgo_dir_t* purrgo_fs_opendir(const char* path);
bool purrgo_fs_readdir(purrgo_dir_t* dir, purrgo_fs_dirent_t* dirent);
void purrgo_fs_closedir(purrgo_dir_t* dir);

#endif // PURRGO_FS_HAL_H