#include "purrgo/fs_hal.h"
#include "fatfs.h"
#include <string.h>
#include "purrgo/purrgo_format.h"
#include <stdio.h>
#include "purrgo/logger.h"

#define STM32_MAX_FILES 14
#define STM32_MAX_DIRS 2

struct purrgo_file_s {
    FIL fil;
    bool in_use;
    
    /* 
     * Логический (виртуальный) указатель.
     * Позволяет выполнять purrgo_fs_seek и purrgo_fs_read из кеша,
     * вообще не дергая f_lseek и не сбрасывая внутреннее окно FatFs.
     */
    uint32_t logical_offset;
};

struct purrgo_dir_s {
    DIR dir;
    bool in_use;
};

static struct purrgo_file_s file_pool[STM32_MAX_FILES] = {0};
static struct purrgo_dir_s dir_pool[STM32_MAX_DIRS] = {0};

/*
 * LRU Cache for files.
 * Cache memory footprint:
 * 32 entries * (512 + 16 bytes overhead) ≈ 16.5 KB
 */
#define LRU_CACHE_ENTRIES 32
#define LRU_SECTOR_SIZE 512

typedef struct {
    bool valid;
    purrgo_file_t* file_id;
    uint32_t sector_num;
    uint32_t last_used;
    uint8_t data[LRU_SECTOR_SIZE];
} lru_cache_entry_t;

static lru_cache_entry_t lru_cache[LRU_CACHE_ENTRIES];
static uint32_t lru_counter = 0;

static void invalidate_cache_for_file(purrgo_file_t* file) {
    for (int i = 0; i < LRU_CACHE_ENTRIES; i++) {
        if (lru_cache[i].valid && lru_cache[i].file_id == file) {
            lru_cache[i].valid = false;
        }
    }
}

static const uint8_t* read_cached_sector(purrgo_file_t* file, uint32_t sector_num, uint32_t* out_read_bytes) {
    int lru_index = -1;
    uint32_t oldest_time = 0xFFFFFFFF;

    // Search in cache
    for (int i = 0; i < LRU_CACHE_ENTRIES; i++) {
        if (lru_cache[i].valid && lru_cache[i].file_id == file && lru_cache[i].sector_num == sector_num) {
            lru_cache[i].last_used = ++lru_counter;
            if (out_read_bytes) *out_read_bytes = LRU_SECTOR_SIZE;
            return lru_cache[i].data;
        }

        if (!lru_cache[i].valid) {
            lru_index = i;
            oldest_time = 0; // Prefer empty slots
        } else if (lru_index == -1 || lru_cache[i].last_used < oldest_time) {
            lru_index = i;
            oldest_time = lru_cache[i].last_used;
        }
    }

    // Cache miss: Load from disk
    FSIZE_t sector_offset = (FSIZE_t)sector_num * LRU_SECTOR_SIZE;

    /* 
     * ИСПРАВЛЕНИЕ КОНФЛИКТА С FATFS:
     * Выполняем физический seek только если внутренний указатель FatFs 
     * сбился и не равен началу нужного нам сектора.
     */
    if (f_tell(&file->fil) != sector_offset) {
        if (f_lseek(&file->fil, sector_offset) != FR_OK) {
            if (out_read_bytes) *out_read_bytes = 0;
            return NULL;
        }
    }

    UINT br = 0;
    FRESULT res = f_read(&file->fil, lru_cache[lru_index].data, LRU_SECTOR_SIZE, &br);

    if (res != FR_OK || br == 0) {
        if (out_read_bytes) *out_read_bytes = 0;
        return NULL;
    }

    lru_cache[lru_index].valid = true;
    lru_cache[lru_index].file_id = file;
    lru_cache[lru_index].sector_num = sector_num;
    lru_cache[lru_index].last_used = ++lru_counter;

    if (br < LRU_SECTOR_SIZE) {
        memset(lru_cache[lru_index].data + br, 0, LRU_SECTOR_SIZE - br);
    }

    if (out_read_bytes) *out_read_bytes = br;
    return lru_cache[lru_index].data;
}

static uint32_t purrgo_fs_read_cached(purrgo_file_t* file, uint8_t* buffer, uint32_t size) {
    uint32_t current_offset = file->logical_offset;
    uint32_t bytes_to_read = size;
    uint32_t bytes_read_total = 0;
    uint8_t* ptr = buffer;

    FSIZE_t fsize = f_size(&file->fil);
    if (current_offset >= fsize) {
        return 0;
    }
    if (current_offset + bytes_to_read > fsize) {
        bytes_to_read = fsize - current_offset;
    }

    while (bytes_to_read > 0) {
        uint32_t sector_num = current_offset / LRU_SECTOR_SIZE;
        uint32_t offset_in_sector = current_offset % LRU_SECTOR_SIZE;
        uint32_t chunk_size = LRU_SECTOR_SIZE - offset_in_sector;
        if (chunk_size > bytes_to_read) {
            chunk_size = bytes_to_read;
        }

        uint32_t sector_read = 0;
        const uint8_t* sector_data = read_cached_sector(file, sector_num, &sector_read);
        if (!sector_data || sector_read <= offset_in_sector) {
            break;
        }

        uint32_t available_in_sector = sector_read - offset_in_sector;
        if (chunk_size > available_in_sector) {
            chunk_size = available_in_sector;
        }

        memcpy(ptr, sector_data + offset_in_sector, chunk_size);

        ptr += chunk_size;
        current_offset += chunk_size;
        bytes_read_total += chunk_size;
        bytes_to_read -= chunk_size;

        if (sector_read < LRU_SECTOR_SIZE) {
            break;
        }
    }

    /* ИСПРАВЛЕНИЕ: Обновляем наш логический указатель без вызова f_lseek */
    file->logical_offset = current_offset;
    return bytes_read_total;
}

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

    /* Инициализация логического смещения для нового файла (с учетом FA_OPEN_APPEND) */
    file->logical_offset = (uint32_t)f_tell(&file->fil);

    invalidate_cache_for_file(file);

    PURRGO_LOG("purrgo_fs_open: %s opened OK\n\r", filepath);
    return file;
}

uint32_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* data, uint32_t size) {
    if (!file || !data || size == 0) return 0;

    invalidate_cache_for_file(file);

    /* Синхронизируем физический указатель с логическим только перед записью */
    if (f_tell(&file->fil) != file->logical_offset) {
        f_lseek(&file->fil, file->logical_offset);
    }

    UINT bw = 0;
    FRESULT res = f_write(&file->fil, data, (UINT)size, &bw);
    if (res == FR_OK) {
        file->logical_offset += bw; // Обновляем виртуальный указатель
    }
    return (uint32_t)bw;
}

uint32_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, uint32_t size) {
    if (!file || !buffer || size == 0) return 0;
    return purrgo_fs_read_cached(file, buffer, size);
}

bool purrgo_fs_seek(purrgo_file_t* file, uint32_t offset) {
    if (!file) return false;
    
    FSIZE_t fsize = f_size(&file->fil);
    if (offset > fsize) {
        file->logical_offset = fsize;
    } else {
        file->logical_offset = offset;
    }
    
    /* 
     * ИСПРАВЛЕНИЕ: Мы НЕ вызываем f_lseek() здесь!
     * Это гарантирует, что хаотичный случайный доступ к кешированным
     * именам БД не будет сбрасывать физическое состояние SD-карты и буфер FatFs.
     */
    return true;
}

void purrgo_fs_sync(purrgo_file_t* file) {
    if (!file) return;
    f_sync(&file->fil);
}

void purrgo_fs_close(purrgo_file_t* file) {
    if (!file || !file->in_use) return;
    invalidate_cache_for_file(file);
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