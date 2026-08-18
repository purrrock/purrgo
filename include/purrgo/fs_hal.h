#ifndef PURRGO_FS_HAL_H
#define PURRGO_FS_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Непрозрачный указатель на структуру файла платформы
typedef struct purrgo_file_s purrgo_file_t;

// Режимы открытия файла (пока требуется только создание на запись или добавление)
typedef enum {
    FS_WRITE_CREATE,
    FS_WRITE_APPEND
} fs_mode_t;

purrgo_file_t* purrgo_fs_open(const char* filepath, fs_mode_t mode);
size_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* data, size_t size);
void purrgo_fs_sync(purrgo_file_t* file);
void purrgo_fs_close(purrgo_file_t* file);

#endif // PURRGO_FS_HAL_H