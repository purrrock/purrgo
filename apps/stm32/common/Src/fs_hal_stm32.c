#include "purrgo/fs_hal.h"
#include "ff.h"
#include <string.h>
#include "purrgo/purrgo_format.h"
#include <stdio.h>
#include "purrgo/logger.h"

#define STM32_MAX_FILES 14
#define STM32_MAX_DIRS 2

/*
 * CLMT (cluster link map table, FatFs fast seek): без неё f_lseek() назад идёт
 * по цепочке кластеров от начала файла, для файла в 95 МБ это порядка 10 мс на
 * каждый переход. Таблица строится один раз при открытии (обход всей цепочки
 * FAT, десятки мс на большой файл) и делает seek O(log фрагментов).
 *
 *   PURRGO_FS_CLMT_MIN_SIZE - таблица только для файлов не меньше этого размера
 *                             (карты); конфиг и треки её не получают.
 *   PURRGO_FS_CLMT_ENTRIES  - DWORD на файл: 2 * (фрагментов + 1). 64 хватает на
 *                             31 фрагмент (~256 Б на слот). При нехватке FatFs
 *                             возвращает FR_NOT_ENOUGH_CORE, файл работает как
 *                             раньше (без быстрого seek).
 */
#ifndef PURRGO_FS_CLMT_MIN_SIZE
#define PURRGO_FS_CLMT_MIN_SIZE (256u * 1024u)
#endif
#ifndef PURRGO_FS_CLMT_ENTRIES
#define PURRGO_FS_CLMT_ENTRIES 64u
#endif

/* Максимальный размер read-ahead блока (секторов по 512 Б), буфер ra_buf. */
#ifndef PURRGO_FS_READ_AHEAD_MAX_SECTORS
#define PURRGO_FS_READ_AHEAD_MAX_SECTORS 8u
#endif

struct purrgo_file_s {
    FIL fil;
    bool in_use;
    bool read_only;       /* открыт в режиме FS_READ */
    uint8_t read_ahead;   /* секторов за один запрос при промахе кэша (1 - по одному) */
    
    /* 
     * Логический (виртуальный) указатель.
     * Позволяет выполнять purrgo_fs_seek и purrgo_fs_read из кеша,
     * вообще не дергая f_lseek и не сбрасывая внутреннее окно FatFs.
     */
    uint32_t logical_offset;

    DWORD clmt[PURRGO_FS_CLMT_ENTRIES];   /* FIL::cltbl, если CLMT построена */
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
 * 64 entries * (512 + 16 bytes overhead) ≈ 33 KB
 *
 * 64 записей, а не 32: read-ahead за один промах вытесняет до
 * PURRGO_FS_READ_AHEAD_MAX_SECTORS записей.
 */
#ifndef PURRGO_FS_LRU_ENTRIES
#define PURRGO_FS_LRU_ENTRIES 64
#endif
#define LRU_CACHE_ENTRIES PURRGO_FS_LRU_ENTRIES
#define LRU_SECTOR_SIZE 512

_Static_assert(PURRGO_FS_READ_AHEAD_MAX_SECTORS < PURRGO_FS_LRU_ENTRIES,
               "read-ahead блок должен помещаться в LRU-кэш");

typedef struct {
    bool valid;
    purrgo_file_t* file_id;
    uint32_t sector_num;
    uint32_t last_used;
    uint8_t data[LRU_SECTOR_SIZE];
} lru_cache_entry_t;

static lru_cache_entry_t lru_cache[LRU_CACHE_ENTRIES];
static uint32_t lru_counter = 0;

/* Буфер для многосекторного чтения: FatFs читает в него одним запросом к SD. */
static uint8_t ra_buf[PURRGO_FS_READ_AHEAD_MAX_SECTORS * LRU_SECTOR_SIZE] __attribute__((aligned(4)));

static void invalidate_cache_for_file(purrgo_file_t* file) {
    for (int i = 0; i < LRU_CACHE_ENTRIES; i++) {
        if (lru_cache[i].valid && lru_cache[i].file_id == file) {
            lru_cache[i].valid = false;
        }
    }
}

/* Индекс записи кэша с данным сектором файла или -1. */
static int lru_find(purrgo_file_t* file, uint32_t sector_num) {
    for (int i = 0; i < LRU_CACHE_ENTRIES; i++) {
        if (lru_cache[i].valid && lru_cache[i].file_id == file && lru_cache[i].sector_num == sector_num) {
            return i;
        }
    }
    return -1;
}

/*
 * Запись под новый сектор: пустая, иначе самая давняя. Записи с locked[i]
 * (уже занятые текущим блоком read-ahead) не вытесняются. -1 - нет кандидатов.
 */
static int lru_victim(const bool* locked) {
    int victim = -1;
    uint32_t oldest_time = 0xFFFFFFFFu;

    for (int i = 0; i < LRU_CACHE_ENTRIES; i++) {
        if (locked && locked[i]) continue;
        if (!lru_cache[i].valid) return i;
        if (victim == -1 || lru_cache[i].last_used < oldest_time) {
            victim = i;
            oldest_time = lru_cache[i].last_used;
        }
    }
    return victim;
}

/*
 * Физический seek только если внутренний указатель FatFs сбился и не равен
 * началу нужного сектора (иначе f_lseek сбрасывал бы окно FatFs зря).
 */
static bool file_seek_physical(purrgo_file_t* file, FSIZE_t offset) {
    if (f_tell(&file->fil) == offset) return true;
    return f_lseek(&file->fil, offset) == FR_OK;
}

/* Положить сектор в кэш: valid_bytes из data, остальное нули. */
static int lru_store(purrgo_file_t* file, uint32_t sector_num, const uint8_t* data,
                     uint32_t valid_bytes, bool* locked) {
    int slot = lru_victim(locked);
    if (slot < 0) return -1;

    memcpy(lru_cache[slot].data, data, valid_bytes);
    if (valid_bytes < LRU_SECTOR_SIZE) {
        memset(lru_cache[slot].data + valid_bytes, 0, LRU_SECTOR_SIZE - valid_bytes);
    }
    lru_cache[slot].valid = true;
    lru_cache[slot].file_id = file;
    lru_cache[slot].sector_num = sector_num;
    lru_cache[slot].last_used = ++lru_counter;
    if (locked) locked[slot] = true;
    return slot;
}

/*
 * Промах при включённом read-ahead: читаем выровненный блок из read_ahead
 * секторов, содержащий нужный, одним f_read (FatFs отдаёт его драйверу SD как
 * многосекторное чтение). Остальные сектора блока кладутся в кэш раньше
 * запрошенного, поэтому запрошенный остаётся самым свежим.
 */
static const uint8_t* read_block_cached(purrgo_file_t* file, uint32_t sector_num, uint32_t* out_read_bytes) {
    const uint32_t n = file->read_ahead;
    const uint32_t base = sector_num - (sector_num % n);

    if (out_read_bytes) *out_read_bytes = 0;

    if (!file_seek_physical(file, (FSIZE_t)base * LRU_SECTOR_SIZE)) {
        return NULL;
    }

    UINT br = 0;
    if (f_read(&file->fil, ra_buf, (UINT)(n * LRU_SECTOR_SIZE), &br) != FR_OK || br == 0) {
        return NULL;
    }

    const uint32_t sectors_got = ((uint32_t)br + LRU_SECTOR_SIZE - 1u) / LRU_SECTOR_SIZE;
    const uint32_t wanted = sector_num - base;
    if (wanted >= sectors_got) {
        return NULL;    /* запрошенный сектор за концом файла */
    }

    bool locked[LRU_CACHE_ENTRIES] = {false};

    for (uint32_t k = 0; k < sectors_got; k++) {
        if (k == wanted) continue;

        int existing = lru_find(file, base + k);
        if (existing >= 0) {
            locked[existing] = true;    /* уже в кэше (данные те же): не вытеснять */
            continue;
        }

        uint32_t valid = (uint32_t)br - k * LRU_SECTOR_SIZE;
        if (valid > LRU_SECTOR_SIZE) valid = LRU_SECTOR_SIZE;
        (void)lru_store(file, base + k, ra_buf + k * LRU_SECTOR_SIZE, valid, locked);
    }

    uint32_t valid = (uint32_t)br - wanted * LRU_SECTOR_SIZE;
    if (valid > LRU_SECTOR_SIZE) valid = LRU_SECTOR_SIZE;

    int slot = lru_find(file, sector_num);
    if (slot >= 0) {
        lru_cache[slot].last_used = ++lru_counter;
    } else {
        slot = lru_store(file, sector_num, ra_buf + wanted * LRU_SECTOR_SIZE, valid, locked);
        if (slot < 0) return NULL;
    }

    if (out_read_bytes) *out_read_bytes = valid;
    return lru_cache[slot].data;
}

static const uint8_t* read_cached_sector(purrgo_file_t* file, uint32_t sector_num, uint32_t* out_read_bytes) {
    // Search in cache
    int hit = lru_find(file, sector_num);
    if (hit >= 0) {
        lru_cache[hit].last_used = ++lru_counter;
        if (out_read_bytes) *out_read_bytes = LRU_SECTOR_SIZE;
        return lru_cache[hit].data;
    }

    if (file->read_ahead > 1u) {
        return read_block_cached(file, sector_num, out_read_bytes);
    }

    // Cache miss: Load from disk
    int lru_index = lru_victim(NULL);
    FSIZE_t sector_offset = (FSIZE_t)sector_num * LRU_SECTOR_SIZE;

    /* 
     * ИСПРАВЛЕНИЕ КОНФЛИКТА С FATFS:
     * Выполняем физический seek только если внутренний указатель FatFs 
     * сбился и не равен началу нужного нам сектора.
     */
    if (!file_seek_physical(file, sector_offset)) {
        if (out_read_bytes) *out_read_bytes = 0;
        return NULL;
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

    file->read_only = (mode == FS_READ);
    file->read_ahead = 1u;
    file->fil.cltbl = NULL;

    /*
     * Быстрый seek (CLMT) для больших файлов, открытых на чтение (файлы карты).
     * Если фрагментов больше, чем помещается в таблицу, FatFs вернёт
     * FR_NOT_ENOUGH_CORE; тогда работаем как раньше, без CLMT.
     */
    if (file->read_only && f_size(&file->fil) >= PURRGO_FS_CLMT_MIN_SIZE) {
        file->clmt[0] = PURRGO_FS_CLMT_ENTRIES;
        file->fil.cltbl = file->clmt;

        FRESULT clmt_res = f_lseek(&file->fil, CREATE_LINKMAP);
        if (clmt_res == FR_OK) {
            PURRGO_LOG("purrgo_fs_open: %s CLMT ok (%lu of %lu dwords)\n\r", filepath,
                       (unsigned long)file->clmt[0], (unsigned long)PURRGO_FS_CLMT_ENTRIES);
        } else {
            PURRGO_LOG("purrgo_fs_open: %s CLMT off: err %d, needs %lu dwords (have %lu)\n\r", filepath,
                       (int)clmt_res, (unsigned long)file->clmt[0], (unsigned long)PURRGO_FS_CLMT_ENTRIES);
            file->fil.cltbl = NULL;
        }
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
        FRESULT seek_res = f_lseek(&file->fil, file->logical_offset);
        if (seek_res != FR_OK) {
            PURRGO_LOG("purrgo_fs_write: seek error: %d\n\r", seek_res);
            return 0;
        }
    }

    UINT bw = 0;
    FRESULT res = f_write(&file->fil, data, (UINT)size, &bw);
    if (res != FR_OK) {
        PURRGO_LOG("purrgo_fs_write: write error: %d\n\r", res);
    }
    /*
     * При ошибке FatFs bw может быть > 0 (часть данных записана);
     * логический указатель должен соответствовать реальному положению.
     */
    file->logical_offset = (uint32_t)f_tell(&file->fil);
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

void purrgo_fs_set_read_ahead(purrgo_file_t* file, uint8_t sectors) {
    if (!file || !file->in_use || !file->read_only) return;

    if (sectors < 1u) sectors = 1u;
    if (sectors > PURRGO_FS_READ_AHEAD_MAX_SECTORS) sectors = (uint8_t)PURRGO_FS_READ_AHEAD_MAX_SECTORS;
    file->read_ahead = sectors;
}

bool purrgo_fs_sync(purrgo_file_t* file) {
    if (!file) return false;
    FRESULT res = f_sync(&file->fil);
    if (res != FR_OK) {
        PURRGO_LOG("purrgo_fs_sync: error: %d\n\r", res);
    }
    return res == FR_OK;
}

/*
 * Внутренние флаги FIL::flag из ff.c (FatFs R0.12c, в ff.h не экспортируются).
 */
#define PURRGO_FA_MODIFIED 0x40
#define PURRGO_FA_DIRTY    0x80

bool purrgo_fs_close(purrgo_file_t* file) {
    if (!file || !file->in_use) return false;
    invalidate_cache_for_file(file);

    FRESULT res = f_close(&file->fil);
    if (res != FR_OK) {
        PURRGO_LOG("purrgo_fs_close: error: %d\n\r", res);
        /*
         * В FatFs R0.12c f_close() при ошибке внутреннего f_sync() не
         * вызывает dec_lock(): запись в таблице блокировок (_FS_LOCK)
         * остаётся занятой, и повторное открытие того же файла вернёт
         * FR_LOCKED. Данные уже потеряны, поэтому сбрасываем признак
         * "изменён" и закрываем повторно, чтобы освободить блокировку.
         */
        if (file->fil.obj.fs != NULL) {
            file->fil.flag &= (BYTE)~(PURRGO_FA_MODIFIED | PURRGO_FA_DIRTY);
            (void)f_close(&file->fil);
        }
    }
    file->in_use = false;
    return res == FR_OK;
}

bool purrgo_fs_replace(const char* src, const char* dst) {
    if (!src || !dst) return false;

    /* f_rename() не перезаписывает существующий файл: сначала удаляем. */
    FRESULT res = f_unlink(dst);
    if (res != FR_OK && res != FR_NO_FILE && res != FR_NO_PATH) {
        PURRGO_LOG("purrgo_fs_replace: unlink %s error: %d\n\r", dst, res);
        return false;
    }

    res = f_rename(src, dst);
    if (res != FR_OK) {
        PURRGO_LOG("purrgo_fs_replace: rename error: %d\n\r", res);
        return false;
    }
    return true;
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