#include "purrgo/fs_hal.h"
#include "fatfs.h"
#include <string.h>
#include "purrgo/purrgo_format.h"
#include <stdio.h>
#include "purrgo/logger.h"

/*
 * Use fixed storage pools instead of heap allocation on STM32 to avoid
 * memory fragmentation and out-of-memory crashes during map rendering
 * or file operations.
 * Map rendering opens up to 4*3 files concurrently, plus config/track logger.
 * A pool of 14 files and 2 directories is safely enough for the current usage.
 */
#define STM32_MAX_FILES 14
#define STM32_MAX_DIRS 2

struct purrgo_file_s {
    FIL fil;
    bool in_use;
};

struct purrgo_dir_s {
    DIR dir;
    bool in_use;
};

static struct purrgo_file_s file_pool[STM32_MAX_FILES] = {0};
static struct purrgo_dir_s dir_pool[STM32_MAX_DIRS] = {0};

const char* purrgo_fs_get_config_path(void) {
    return "0:/PURRGO/PURRGO.CFG";
}

const char* purrgo_fs_get_maps_path(void) {
    return "0:/PURRGO/MAPS";
}

const char* purrgo_fs_get_tracks_path(void) {
    return "0:/PURRGO/TRACKS";
}

purrgo_file_t* purrgo_fs_open(const char* filepath, fs_mode_t mode) {
    if (!filepath) return NULL;
  //  PURRGO_LOG("purrgo_fs_open: %s mode: %d\n\r", filepath, mode);
    BYTE ff_mode = 0;
    switch (mode) {
        case FS_READ:
            ff_mode = FA_READ | FA_OPEN_EXISTING;
            break;
        case FS_WRITE_CREATE:
            ff_mode = FA_WRITE | FA_CREATE_ALWAYS;
            break;
        case FS_WRITE_APPEND:
            ff_mode = FA_WRITE | FA_OPEN_ALWAYS | FA_OPEN_APPEND;
            break;
        default:
            return NULL;
    }

    purrgo_file_t* file = NULL;
    for (int i = 0; i < STM32_MAX_FILES; i++) {
        if (!file_pool[i].in_use) {
            file = &file_pool[i];
            file->in_use = true;
            break;
        }
    }

    if (!file) {
        PURRGO_LOG("purrgo_fs_open: %s open error: no free file slots\n\r", filepath);
        return NULL;
    }

    FRESULT res = f_open(&file->fil, filepath, ff_mode);
    if (res != FR_OK) {
        PURRGO_LOG("purrgo_fs_open: %s open error: %d\n\r", filepath, res);
        file->in_use = false;
        return NULL;
    }
    PURRGO_LOG("purrgo_fs_open: %s opened OK\n\r", filepath);
    return file;
}

uint32_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* data, uint32_t size) {
    if (!file || !data || size == 0) return 0;

    UINT bw = 0;
    FRESULT res = f_write(&file->fil, data, (UINT)size, &bw);
    if (res != FR_OK) {
        return 0;
    }
    return (uint32_t)bw;
}

uint32_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, uint32_t size) {
    if (!file || !buffer || size == 0) return 0;

    UINT br = 0;
    FRESULT res = f_read(&file->fil, buffer, (UINT)size, &br);
    if (res != FR_OK) {
        return 0;
    }
    return (uint32_t)br;
}

bool purrgo_fs_seek(purrgo_file_t* file, uint32_t offset) {
    if (!file) return false;
    FRESULT res = f_lseek(&file->fil, (FSIZE_t)offset);
    return (res == FR_OK);
}

void purrgo_fs_sync(purrgo_file_t* file) {
    if (!file) return;
    f_sync(&file->fil);
}

void purrgo_fs_close(purrgo_file_t* file) {
    if (!file || !file->in_use) return;
    f_close(&file->fil);
    file->in_use = false;
}

purrgo_dir_t* purrgo_fs_opendir(const char* path) {
    if (!path) return NULL;

    purrgo_dir_t* dir = NULL;
    for (int i = 0; i < STM32_MAX_DIRS; i++) {
        if (!dir_pool[i].in_use) {
            dir = &dir_pool[i];
            dir->in_use = true;
            break;
        }
    }

    if (!dir) {
        PURRGO_LOG("purrgo_fs_opendir: %s open error: no free dir slots\n\r", path);
        return NULL;
    }

    FRESULT res = f_opendir(&dir->dir, path);
    if (res != FR_OK) {
        dir->in_use = false;
        return NULL;
    }
    return dir;
}

bool purrgo_fs_readdir(purrgo_dir_t* dir, purrgo_fs_dirent_t* dirent) {
    if (!dir || !dirent) return false;

    FILINFO fno;
    FRESULT res = f_readdir(&dir->dir, &fno);

    if (res != FR_OK || fno.fname[0] == 0) {
        return false;
    }

    purrgo_snprintf(dirent->name, PURRGO_FS_MAX_PATH, "%s", fno.fname);
    dirent->is_directory = (fno.fattrib & AM_DIR) ? true : false;

    return true;
}

void purrgo_fs_closedir(purrgo_dir_t* dir) {
    if (!dir || !dir->in_use) return;
    f_closedir(&dir->dir);
    dir->in_use = false;
}
