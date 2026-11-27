#include "purrgo/fs_hal.h"
#include "ff.h"
#include <string.h>
#include "purrgo/purrgo_format.h"
#include <stdio.h>
#include "purrgo/logger.h"
#include "purrgo/render_prof.h"
#include "sd_card.h"

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
    uint8_t read_ahead;   /* секторов за один запрос при промахе кэша (1 - по одному); у адаптивного - максимум */
    bool ra_adaptive;     /* адаптивный read-ahead: блок растёт, пока промахи идут подряд */
    uint8_t ra_cur;       /* размер последнего прочитанного блока (адаптивный режим) */
    uint32_t ra_next;     /* сектор сразу за последним прочитанным блоком (адаптивный режим) */
    
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
 * Кэш секторов файлов (512 Б). Память двух видов:
 *  - встроенный маленький (PURRGO_FS_LRU_ENTRIES секторов, статический RAM): есть всегда;
 *  - большой в арене (purrgo_fs_cache_attach): на время режима карты, сотни секторов.
 * Структура одна: хэш-таблица для поиска, двусвязный список LRU для вытеснения и список
 * свободных записей. Поиск и вытеснение O(1): прежний линейный перебор при сотнях записей
 * стоил бы заметных долей кадра. Кэш живёт между кадрами: повторный рисунок того же вида
 * и перекрывающиеся виды читаются из RAM, а не с SD.
 */
#ifndef PURRGO_FS_LRU_ENTRIES
#define PURRGO_FS_LRU_ENTRIES 64
#endif
#define LRU_SECTOR_SIZE 512u
#define CACHE_NONE 0xFFFFu
#define CACHE_MAX_SECTORS 4096u

/* Поиск по хэшу сектор-файл: бакетов 2 * записей, степень двойки. */
#define SMALL_BUCKETS (PURRGO_FS_LRU_ENTRIES * 2)

_Static_assert(PURRGO_FS_READ_AHEAD_MAX_SECTORS < PURRGO_FS_LRU_ENTRIES,
               "read-ahead блок должен помещаться в LRU-кэш");
_Static_assert((SMALL_BUCKETS & (SMALL_BUCKETS - 1)) == 0,
               "PURRGO_FS_LRU_ENTRIES должно быть степенью двойки");

typedef struct {
    uint32_t sector;     /* номер сектора в файле */
    uint16_t prev;       /* к более свежей записи (к head) */
    uint16_t next;       /* к более давней (к tail); у свободных - следующая свободная */
    uint16_t hnext;      /* следующая запись в том же бакете */
    uint8_t  slot;       /* слот файла в file_pool */
    uint8_t  valid;
} cache_meta_t;

typedef struct {
    cache_meta_t *meta;
    uint8_t *data;
    uint16_t *bucket;
    uint32_t count;
    uint32_t bucket_mask;
    uint16_t head;       /* самая свежая запись */
    uint16_t tail;       /* самая давняя запись */
    uint16_t free_head;
    uint16_t slot_used[STM32_MAX_FILES];   /* записей кэша у каждого файла */
} sector_cache_t;

static cache_meta_t s_small_meta[PURRGO_FS_LRU_ENTRIES];
static uint16_t s_small_bucket[SMALL_BUCKETS];
static uint8_t s_small_data[PURRGO_FS_LRU_ENTRIES * LRU_SECTOR_SIZE] __attribute__((aligned(4)));
static sector_cache_t s_small = {
    .meta = s_small_meta, .data = s_small_data, .bucket = s_small_bucket,
    .count = PURRGO_FS_LRU_ENTRIES, .bucket_mask = SMALL_BUCKETS - 1u,
};
static sector_cache_t s_big;
static sector_cache_t *g_cache;     /* активный кэш; NULL до первого обращения */

/* Буфер для многосекторного чтения: FatFs читает в него одним запросом к SD. */
static uint8_t ra_buf[PURRGO_FS_READ_AHEAD_MAX_SECTORS * LRU_SECTOR_SIZE] __attribute__((aligned(4)));

static uint32_t cache_hash(uint8_t slot, uint32_t sector) {
    uint32_t h = sector * 0x9E3779B1u + (uint32_t)slot * 0x85EBCA6Bu;
    return h ^ (h >> 15);
}

static void cache_reset(sector_cache_t* c) {
    for (uint32_t i = 0; i < c->count; i++) {
        c->meta[i].valid = 0;
        c->meta[i].next = (i + 1u < c->count) ? (uint16_t)(i + 1u) : CACHE_NONE;
    }
    for (uint32_t i = 0; i <= c->bucket_mask; i++) {
        c->bucket[i] = CACHE_NONE;
    }
    c->free_head = (c->count != 0u) ? 0u : CACHE_NONE;
    c->head = CACHE_NONE;
    c->tail = CACHE_NONE;
    memset(c->slot_used, 0, sizeof(c->slot_used));
}

static sector_cache_t* cache_active(void) {
    if (g_cache == NULL) {
        cache_reset(&s_small);
        g_cache = &s_small;
    }
    return g_cache;
}

#if PURRGO_RENDER_PROF
/* Нужна только PURRGO_PROF_FILE_INFO; без профилирования макрос пустой. */
static uint32_t cache_entries(void) {
    return cache_active()->count;
}
#endif

static uint8_t* cache_data(const sector_cache_t* c, uint16_t i) {
    return c->data + (size_t)i * LRU_SECTOR_SIZE;
}

static void lru_unlink(sector_cache_t* c, uint16_t i) {
    const uint16_t p = c->meta[i].prev;
    const uint16_t n = c->meta[i].next;
    if (p != CACHE_NONE) c->meta[p].next = n; else c->head = n;
    if (n != CACHE_NONE) c->meta[n].prev = p; else c->tail = p;
}

static void lru_push_head(sector_cache_t* c, uint16_t i) {
    c->meta[i].prev = CACHE_NONE;
    c->meta[i].next = c->head;
    if (c->head != CACHE_NONE) c->meta[c->head].prev = i; else c->tail = i;
    c->head = i;
}

static void cache_touch(sector_cache_t* c, uint16_t i) {
    if (c->head == i) return;
    lru_unlink(c, i);
    lru_push_head(c, i);
}

static void bucket_remove(sector_cache_t* c, uint16_t i) {
    const uint32_t b = cache_hash(c->meta[i].slot, c->meta[i].sector) & c->bucket_mask;
    uint16_t prev = CACHE_NONE;
    uint16_t cur = c->bucket[b];
    while (cur != CACHE_NONE && cur != i) {
        prev = cur;
        cur = c->meta[cur].hnext;
    }
    if (cur != i) return;
    if (prev == CACHE_NONE) c->bucket[b] = c->meta[i].hnext; else c->meta[prev].hnext = c->meta[i].hnext;
}

/* Запись выбывает из списка, из хэша и становится свободной. */
static void cache_evict(sector_cache_t* c, uint16_t i) {
    lru_unlink(c, i);
    bucket_remove(c, i);
    if (c->slot_used[c->meta[i].slot] != 0u) c->slot_used[c->meta[i].slot]--;
    c->meta[i].valid = 0;
    c->meta[i].next = c->free_head;
    c->free_head = i;
}

/* Индекс записи кэша с данным сектором файла или CACHE_NONE. */
static uint16_t cache_find(sector_cache_t* c, uint8_t slot, uint32_t sector) {
    uint16_t i = c->bucket[cache_hash(slot, sector) & c->bucket_mask];
    while (i != CACHE_NONE) {
        if (c->meta[i].sector == sector && c->meta[i].slot == slot) return i;
        i = c->meta[i].hnext;
    }
    return CACHE_NONE;
}

/* Новая запись под сектор (самая давняя вытесняется) становится самой свежей. */
static uint16_t cache_insert(sector_cache_t* c, uint8_t slot, uint32_t sector) {
    if (c->free_head == CACHE_NONE) {
        cache_evict(c, c->tail);
    }
    const uint16_t i = c->free_head;
    c->free_head = c->meta[i].next;

    c->meta[i].sector = sector;
    c->meta[i].slot = slot;
    c->meta[i].valid = 1;
    const uint32_t b = cache_hash(slot, sector) & c->bucket_mask;
    c->meta[i].hnext = c->bucket[b];
    c->bucket[b] = i;
    lru_push_head(c, i);
    c->slot_used[slot]++;
    return i;
}

static void cache_invalidate_slot(sector_cache_t* c, uint8_t slot) {
    if (c->slot_used[slot] == 0u) return;
    for (uint32_t i = 0; i < c->count; i++) {
        if (c->meta[i].valid && c->meta[i].slot == slot) {
            cache_evict(c, (uint16_t)i);
        }
    }
}

static uint8_t file_slot(const purrgo_file_t* file) {
    return (uint8_t)(file - file_pool);
}

static void invalidate_cache_for_file(purrgo_file_t* file) {
    cache_invalidate_slot(cache_active(), file_slot(file));
}

/* ---- Большой кэш в арене ------------------------------------------------- */

static uint32_t pow2_at_least(uint32_t v) {
    uint32_t p = 1u;
    while (p < v) p <<= 1;
    return p;
}

/* Раскладка кэша в одном блоке памяти: meta | bucket | data. */
static size_t cache_layout(uint32_t sectors, size_t* bucket_off, size_t* data_off, uint32_t* buckets) {
    const uint32_t nb = pow2_at_least(sectors * 2u);
    const size_t meta_bytes = ((size_t)sectors * sizeof(cache_meta_t) + 7u) & ~(size_t)7u;
    const size_t bucket_bytes = (((size_t)nb * sizeof(uint16_t)) + 7u) & ~(size_t)7u;
    *bucket_off = meta_bytes;
    *data_off = meta_bytes + bucket_bytes;
    *buckets = nb;
    return *data_off + (size_t)sectors * LRU_SECTOR_SIZE;
}

size_t purrgo_fs_cache_bytes(uint32_t sectors) {
    if (sectors <= PURRGO_FS_READ_AHEAD_MAX_SECTORS || sectors > CACHE_MAX_SECTORS) return 0u;
    size_t bucket_off, data_off;
    uint32_t buckets;
    return cache_layout(sectors, &bucket_off, &data_off, &buckets);
}

bool purrgo_fs_cache_attach(void* mem, size_t size, uint32_t sectors) {
    const size_t need = purrgo_fs_cache_bytes(sectors);
    if (mem == NULL || need == 0u || size < need || ((uintptr_t)mem & 7u) != 0u) {
        return false;
    }

    size_t bucket_off, data_off;
    uint32_t buckets;
    (void)cache_layout(sectors, &bucket_off, &data_off, &buckets);

    uint8_t* base = (uint8_t*)mem;
    s_big.meta = (cache_meta_t*)base;
    s_big.bucket = (uint16_t*)(base + bucket_off);
    s_big.data = base + data_off;
    s_big.count = sectors;
    s_big.bucket_mask = buckets - 1u;
    cache_reset(&s_big);
    g_cache = &s_big;
    return true;
}

void purrgo_fs_cache_detach(void) {
    if (g_cache == &s_big) {
        memset(&s_big, 0, sizeof(s_big));
        cache_reset(&s_small);
        g_cache = &s_small;
    }
}

/*
 * Физический seek только если внутренний указатель FatFs сбился и не равен
 * началу нужного сектора (иначе f_lseek сбрасывал бы окно FatFs зря).
 */
static bool file_seek_physical(purrgo_file_t* file, FSIZE_t offset) {
    if (f_tell(&file->fil) == offset) return true;
    return f_lseek(&file->fil, offset) == FR_OK;
}

/*
 * Промах: читаем блок из n секторов с сектора base одним f_read (FatFs отдаёт его драйверу SD
 * как многосекторное чтение), кладём в кэш. Остальные сектора блока кладутся раньше
 * запрошенного, поэтому запрошенный остаётся самым свежим. Уже лежащие в кэше сектора
 * блока обновляются (не дублируются).
 */
/*
 * Копирование сектора из ra_buf в кэш. Замер Release (prof5_1.log): вне драйвера SD на один
 * промахнувшийся сектор уходит около 40 мкс (sd - sum в PROF5), что похоже на побайтный
 * memcpy из newlib-nano (-Os). Для целого выровненного сектора копируем словами, по 8 слов
 * на итерацию (чтение всех, потом запись: компилятор не превращает цикл обратно в memcpy).
 */
typedef uint32_t __attribute__((may_alias)) sector_word_t;

static void copy_sector_in(uint8_t* dst, const uint8_t* src, uint32_t valid) {
    if (valid == LRU_SECTOR_SIZE && (((uintptr_t)dst | (uintptr_t)src) & 3u) == 0u) {
        sector_word_t* d = (sector_word_t*)dst;
        const sector_word_t* s = (const sector_word_t*)src;
        for (uint32_t i = 0; i < LRU_SECTOR_SIZE / 4u; i += 8u) {
            const uint32_t a0 = s[i], a1 = s[i + 1], a2 = s[i + 2], a3 = s[i + 3];
            const uint32_t a4 = s[i + 4], a5 = s[i + 5], a6 = s[i + 6], a7 = s[i + 7];
            d[i] = a0; d[i + 1] = a1; d[i + 2] = a2; d[i + 3] = a3;
            d[i + 4] = a4; d[i + 5] = a5; d[i + 6] = a6; d[i + 7] = a7;
        }
        return;
    }
    memcpy(dst, src, valid);
    if (valid < LRU_SECTOR_SIZE) memset(dst + valid, 0, LRU_SECTOR_SIZE - valid);
}

static const uint8_t* read_block_cached(purrgo_file_t* file, uint32_t base, uint32_t n,
                                        uint32_t sector_num, uint32_t* out_read_bytes) {
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

    /* Состояние адаптивного read-ahead: следующий сектор после прочитанного блока. */
    file->ra_cur = (uint8_t)n;
    file->ra_next = base + sectors_got;

    sector_cache_t* c = cache_active();
    const uint8_t slot = file_slot(file);

    for (uint32_t k = 0; k < sectors_got; k++) {
        if (k == wanted) continue;

        const uint16_t existing = cache_find(c, slot, base + k);
        if (existing != CACHE_NONE) {
            cache_touch(c, existing);
            continue;
        }

        uint32_t valid = (uint32_t)br - k * LRU_SECTOR_SIZE;
        if (valid > LRU_SECTOR_SIZE) valid = LRU_SECTOR_SIZE;
        const uint16_t i = cache_insert(c, slot, base + k);
        copy_sector_in(cache_data(c, i), ra_buf + k * LRU_SECTOR_SIZE, valid);
    }

    uint32_t valid = (uint32_t)br - wanted * LRU_SECTOR_SIZE;
    if (valid > LRU_SECTOR_SIZE) valid = LRU_SECTOR_SIZE;

    uint16_t i = cache_find(c, slot, sector_num);
    if (i != CACHE_NONE) {
        cache_touch(c, i);
    } else {
        i = cache_insert(c, slot, sector_num);
        copy_sector_in(cache_data(c, i), ra_buf + wanted * LRU_SECTOR_SIZE, valid);
    }

    if (out_read_bytes) *out_read_bytes = valid;
    return cache_data(c, i);
}

/*
 * Промах кэша: чтение с SD (seek + f_read). Размер блока:
 *  - обычный файл: read_ahead секторов, блок выровнен по read_ahead;
 *  - адаптивный (ra_adaptive): 1 сектор, а если промах пришёлся ровно на сектор за концом
 *    предыдущего прочитанного блока (обход идёт подряд) - вдвое больше предыдущего блока
 *    (2, 4, ... до read_ahead), блок начинается с нужного сектора.
 */
static const uint8_t* read_sector_miss(purrgo_file_t* file, uint32_t sector_num, uint32_t* out_read_bytes) {
    uint32_t n = (file->read_ahead > 1u) ? file->read_ahead : 1u;
    uint32_t base;

    if (file->ra_adaptive) {
        if (file->ra_next == sector_num && n > 1u) {
            uint32_t grown = (uint32_t)file->ra_cur * 2u;
            if (grown < 2u) grown = 2u;
            if (grown < n) n = grown;
        } else {
            n = 1u;
        }
        base = sector_num;
    } else {
        base = sector_num - (sector_num % n);
    }

    PURRGO_PROF_COUNT_ADD(PURRGO_PROF_CNT_SD_SECTORS, n);
    return read_block_cached(file, base, n, sector_num, out_read_bytes);
}

static const uint8_t* read_cached_sector(purrgo_file_t* file, uint32_t sector_num, uint32_t* out_read_bytes) {
    sector_cache_t* c = cache_active();
    const uint16_t hit = cache_find(c, file_slot(file), sector_num);
    if (hit != CACHE_NONE) {
        PURRGO_PROF_COUNT_ADD(PURRGO_PROF_CNT_CACHE_HIT, 1u);
        PURRGO_PROF_SECTOR(file_slot(file), sector_num, false);
        cache_touch(c, hit);
        if (out_read_bytes) *out_read_bytes = LRU_SECTOR_SIZE;
        return cache_data(c, hit);
    }

    PURRGO_PROF_BEGIN(prof_sd);
    const uint8_t* data = read_sector_miss(file, sector_num, out_read_bytes);
    PURRGO_PROF_END(prof_sd, PURRGO_PROF_SD);
    PURRGO_PROF_SECTOR(file_slot(file), sector_num, true);
    return data;
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

const char* purrgo_fs_get_routes_path(void) {
    return "0:/PURRGO/ROUTES";
}

const char* purrgo_fs_get_waypts_path(void) {
    return "0:/PURRGO/WAYPTS";
}

const char* purrgo_fs_get_books_path(void) {
    return "0:/PURRGO/BOOKS";
}

purrgo_file_t* purrgo_fs_open(const char* filepath, fs_mode_t mode) {
    if (!filepath) return NULL;
    (void)purrgo_sd_wake();     /* карта могла быть припаркована по тайм-ауту простоя */
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
        PURRGO_LOGE("purrgo_fs_open: %s open error: no free file slots\n\r", filepath);
        return NULL;
    }

    FRESULT res = f_open(&file->fil, filepath, ff_mode);
    if (res != FR_OK) {
        /* Отсутствие файла/пути - штатная ситуация (проверка наличия), не ошибка. */
        if ((res == FR_NO_FILE) || (res == FR_NO_PATH)) {
            PURRGO_LOGD("purrgo_fs_open: %s not found (%d)\n\r", filepath, (int)res);
        } else {
            PURRGO_LOGE("purrgo_fs_open: %s open error: %d\n\r", filepath, (int)res);
        }
        file->in_use = false;
        return NULL;
    }

    file->read_only = (mode == FS_READ);
    file->read_ahead = 1u;
    file->ra_adaptive = false;
    file->ra_cur = 1u;
    file->ra_next = 0xFFFFFFFFu;
    PURRGO_PROF_FILE_INFO(file_slot(file), filepath, 1u, cache_entries());
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
        if (clmt_res != FR_OK) {   /* успех не логируем: достаточно строки "opened OK" */
            PURRGO_LOGW("purrgo_fs_open: %s CLMT off: err %d, needs %lu dwords (have %lu)\n\r", filepath,
                       (int)clmt_res, (unsigned long)file->clmt[0], (unsigned long)PURRGO_FS_CLMT_ENTRIES);
            file->fil.cltbl = NULL;
        }
    }

    /* Инициализация логического смещения для нового файла (с учетом FA_OPEN_APPEND) */
    file->logical_offset = (uint32_t)f_tell(&file->fil);

    invalidate_cache_for_file(file);

    PURRGO_LOGD("purrgo_fs_open: %s opened OK\n\r", filepath);
    return file;
}

uint32_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* data, uint32_t size) {
    if (!file || !data || size == 0) return 0;

    invalidate_cache_for_file(file);

    /* Синхронизируем физический указатель с логическим только перед записью */
    if (f_tell(&file->fil) != file->logical_offset) {
        FRESULT seek_res = f_lseek(&file->fil, file->logical_offset);
        if (seek_res != FR_OK) {
            PURRGO_LOGE("purrgo_fs_write: seek error: %d\n\r", seek_res);
            return 0;
        }
    }

    UINT bw = 0;
    FRESULT res = f_write(&file->fil, data, (UINT)size, &bw);
    if (res != FR_OK) {
        PURRGO_LOGE("purrgo_fs_write: write error: %d\n\r", res);
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
    PURRGO_PROF_COUNT_ADD(PURRGO_PROF_CNT_FS_READ, 1u);
    PURRGO_PROF_COUNT_ADD(PURRGO_PROF_CNT_FS_READ_BYTES, size);
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

bool purrgo_fs_size(purrgo_file_t* file, uint32_t* size_out) {
    if (!file || !file->in_use || !size_out) return false;
    *size_out = (uint32_t)f_size(&file->fil);
    return true;
}

bool purrgo_fs_truncate(purrgo_file_t* file, uint32_t size) {
    if (!file || !file->in_use || file->read_only) return false;
    if (size > (uint32_t)f_size(&file->fil)) return false;

    invalidate_cache_for_file(file);

    /* f_truncate() режет файл по текущему положению указателя FatFs. */
    FRESULT res = FR_OK;
    if (f_tell(&file->fil) != size) {
        res = f_lseek(&file->fil, size);
    }
    if (res == FR_OK) {
        res = f_truncate(&file->fil);
    }
    file->logical_offset = (uint32_t)f_tell(&file->fil);
    if (res != FR_OK) {
        PURRGO_LOGE("purrgo_fs_truncate: error: %d\n\r", res);
    }
    return res == FR_OK;
}

void purrgo_fs_set_read_ahead(purrgo_file_t* file, uint8_t sectors) {
    if (!file || !file->in_use || !file->read_only) return;

    if (sectors < 1u) sectors = 1u;
    if (sectors > PURRGO_FS_READ_AHEAD_MAX_SECTORS) sectors = (uint8_t)PURRGO_FS_READ_AHEAD_MAX_SECTORS;
    file->read_ahead = sectors;
    file->ra_adaptive = false;
    PURRGO_PROF_FILE_INFO(file_slot(file), NULL, sectors, cache_entries());
}

void purrgo_fs_set_adaptive_read_ahead(purrgo_file_t* file, uint8_t max_sectors) {
    if (!file || !file->in_use || !file->read_only) return;

    if (max_sectors < 1u) max_sectors = 1u;
    if (max_sectors > PURRGO_FS_READ_AHEAD_MAX_SECTORS) max_sectors = (uint8_t)PURRGO_FS_READ_AHEAD_MAX_SECTORS;
    file->read_ahead = max_sectors;
    file->ra_adaptive = (max_sectors > 1u);
    file->ra_cur = 1u;
    file->ra_next = 0xFFFFFFFFu;
    /* Для трассы PROF это файл с read-ahead 1: блок растёт только по факту подряд идущих промахов. */
    PURRGO_PROF_FILE_INFO(file_slot(file), NULL, 1u, cache_entries());
}

bool purrgo_fs_sync(purrgo_file_t* file) {
    if (!file) return false;
    FRESULT res = f_sync(&file->fil);
    if (res != FR_OK) {
        PURRGO_LOGE("purrgo_fs_sync: error: %d\n\r", res);
    }
    return res == FR_OK;
}

/*
 * Внутренние флаги FIL::flag из ff.c (в ff.h не экспортируются). Значения сверены
 * с FatFs R0.16 (FF_DEFINED 80386). При обновлении FatFs проверку ниже и обходной
 * путь в purrgo_fs_close() нужно пересмотреть.
 */
#define PURRGO_FA_MODIFIED 0x40
#define PURRGO_FA_DIRTY    0x80
_Static_assert(FF_DEFINED == 80386, "FatFs updated: recheck FA_MODIFIED/FA_DIRTY workaround in purrgo_fs_close");

bool purrgo_fs_close(purrgo_file_t* file) {
    if (!file || !file->in_use) return false;
    invalidate_cache_for_file(file);

    FRESULT res = f_close(&file->fil);
    if (res != FR_OK) {
        PURRGO_LOGE("purrgo_fs_close: error: %d\n\r", res);
        /*
         * В FatFs (R0.12c-R0.16) f_close() при ошибке внутреннего f_sync() не
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
    purrgo_sd_touch();          /* тайм-аут парковки отсчитывается от последнего закрытия */
    return res == FR_OK;
}

bool purrgo_fs_replace(const char* src, const char* dst) {
    if (!src || !dst) return false;
    (void)purrgo_sd_wake();

    /* f_rename() не перезаписывает существующий файл: сначала удаляем. */
    FRESULT res = f_unlink(dst);
    if (res != FR_OK && res != FR_NO_FILE && res != FR_NO_PATH) {
        PURRGO_LOGE("purrgo_fs_replace: unlink %s error: %d\n\r", dst, res);
        return false;
    }

    res = f_rename(src, dst);
    if (res != FR_OK) {
        PURRGO_LOGE("purrgo_fs_replace: rename error: %d\n\r", res);
        return false;
    }
    return true;
}

void purrgo_fs_idle_hint(void) {
    /* Открытые файлы и каталоги (карта, журнал на SD) парковку не пускают: purrgo_sd_park_now() вернёт false. */
    (void)purrgo_sd_park_now();
}

bool purrgo_fs_exists(const char* path) {
    if (!path) return false;
    (void)purrgo_sd_wake();
    /* fno == NULL допустим: нужна только проверка наличия, буфер FILINFO на стеке не нужен */
    return f_stat(path, NULL) == FR_OK;
}

purrgo_dir_t* purrgo_fs_opendir(const char* path) {
    if (!path) return NULL;
    (void)purrgo_sd_wake();

    purrgo_dir_t* dir = NULL;
    for (int i = 0; i < STM32_MAX_DIRS; i++) {
        if (!dir_pool[i].in_use) {
            dir = &dir_pool[i];
            dir->in_use = true;
            break;
        }
    }

    if (!dir) {
        PURRGO_LOGE("purrgo_fs_opendir: %s open error: no free dir slots\n\r", path);
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
    purrgo_sd_touch();
}

uint32_t purrgo_fs_stm32_open_count(void) {
    uint32_t n = 0u;
    for (int i = 0; i < STM32_MAX_FILES; i++) {
        if (file_pool[i].in_use) n++;
    }
    for (int i = 0; i < STM32_MAX_DIRS; i++) {
        if (dir_pool[i].in_use) n++;
    }
    return n;
}
