/*
 * Кэш секторов и read-ahead в прошивочном fs_hal_stm32.c (хэш + список LRU, большой кэш в
 * арене, адаптивный read-ahead) на заглушке FatFs: файлы в памяти, обращения к "SD" считаются.
 * Проверяется: данные, попадания, LRU-вытеснение, отдельные файлы, перекрытие кэша между
 * "кадрами", рост блока при чтении подряд и одиночные сектора при случайном доступе,
 * подключение/отключение крупного кэша, инвалидация при переоткрытии, сравнение числа чтений
 * с независимой моделью LRU на случайной нагрузке.
 */
#include <stdlib.h>
#include <string.h>
#include "ff.h"
#include "purrgo/fs_hal.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

/* ---- Заглушка FatFs ------------------------------------------------------ */

#define STUB_FILES 4
static struct { const char *name; uint32_t size; uint8_t salt; } g_files[STUB_FILES] = {
    { "a.idx", 300u * 512u + 137u, 11 },    /* неполный последний сектор */
    { "b.mlp", 120u * 512u, 22 },
    { "c.db", 40u * 512u, 33 },
    { "d.idx", 600u * 512u, 44 },
};

static uint8_t gen(int file, uint32_t off)
{
    return (uint8_t)(((off * 2654435761u) >> 13) ^ ((uint32_t)g_files[file].salt * 37u) ^ (off >> 9));
}

static uint32_t g_nread;                 /* вызовов f_read (команд чтения SD) */
static uint32_t g_sectors_read;          /* секторов, запрошенных у SD */
static uint32_t g_read_len[8192];        /* размер каждого f_read, секторов */
static uint32_t g_read_off[8192];        /* смещение каждого f_read, секторов */
static int g_epoch;                      /* меняет содержимое файлов (проверка инвалидации) */

static void stats_reset(void) { g_nread = 0; g_sectors_read = 0; }

FRESULT f_open(FIL *fp, const TCHAR *path, BYTE mode)
{
    (void)mode;
    for (int i = 0; i < STUB_FILES; i++) {
        if (strstr(path, g_files[i].name) != NULL) {
            memset(fp, 0, sizeof *fp);
            fp->obj.fs = fp;
            fp->stub_file = i;
            fp->fsize = g_files[i].size;
            return FR_OK;
        }
    }
    return FR_NO_FILE;
}
FRESULT f_close(FIL *fp) { fp->obj.fs = NULL; return FR_OK; }
FRESULT f_read(FIL *fp, void *buff, UINT btr, UINT *br)
{
    uint32_t n = btr;
    if (fp->fptr >= fp->fsize) { *br = 0; return FR_OK; }
    if (n > fp->fsize - fp->fptr) n = fp->fsize - fp->fptr;
    for (uint32_t i = 0; i < n; i++) {
        ((uint8_t *)buff)[i] = (uint8_t)(gen(fp->stub_file, fp->fptr + i) + g_epoch);
    }
    if (g_nread < 8192) {
        g_read_len[g_nread] = (btr + 511u) / 512u;
        g_read_off[g_nread] = fp->fptr / 512u;
    }
    g_nread++;
    g_sectors_read += (btr + 511u) / 512u;
    fp->fptr += n;
    *br = n;
    return FR_OK;
}
FRESULT f_lseek(FIL *fp, FSIZE_t ofs)
{
    if (ofs == CREATE_LINKMAP) return FR_OK;
    fp->fptr = ofs > fp->fsize ? fp->fsize : ofs;
    return FR_OK;
}
FRESULT f_write(FIL *fp, const void *b, UINT n, UINT *bw) { (void)fp; (void)b; *bw = n; return FR_OK; }
FRESULT f_sync(FIL *fp) { (void)fp; return FR_OK; }
FRESULT f_unlink(const TCHAR *p) { (void)p; return FR_NO_FILE; }
FRESULT f_rename(const TCHAR *a, const TCHAR *b) { (void)a; (void)b; return FR_NO_FILE; }
FRESULT f_stat(const TCHAR *p, FILINFO *f) { (void)p; (void)f; return FR_NO_FILE; }
FRESULT f_opendir(DIR *d, const TCHAR *p) { (void)d; (void)p; return FR_NO_PATH; }
FRESULT f_readdir(DIR *d, FILINFO *f) { (void)d; (void)f; return FR_NO_FILE; }
FRESULT f_closedir(DIR *d) { (void)d; return FR_OK; }

/* ---- Помощники теста ----------------------------------------------------- */

static purrgo_file_t *open_file(int i)
{
    return purrgo_fs_open(g_files[i].name, FS_READ);
}

/* Читает n байт с offset и сверяет с генератором; возвращает true, если всё совпало. */
static bool read_check(purrgo_file_t *f, int file, uint32_t off, uint32_t n)
{
    uint8_t buf[2048];
    if (n > sizeof buf) n = sizeof buf;
    if (!purrgo_fs_seek(f, off)) return false;
    const uint32_t got = purrgo_fs_read(f, buf, n);
    uint32_t want = n;
    if (off >= g_files[file].size) want = 0;
    else if (n > g_files[file].size - off) want = g_files[file].size - off;
    if (got != want) return false;
    for (uint32_t i = 0; i < got; i++) {
        if (buf[i] != (uint8_t)(gen(file, off + i) + g_epoch)) return false;
    }
    return true;
}

static uint32_t g_rng = 1;
static uint32_t rnd(void) { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }

/* Независимая модель LRU-кэша на n записей: сколько чтений с SD потребует последовательность. */
typedef struct { int file; uint32_t sector; uint32_t stamp; int valid; } mdl_t;
static uint32_t model_misses(mdl_t *m, uint32_t n, uint32_t *clock, int file, uint32_t sector)
{
    uint32_t victim = 0, oldest = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < n; i++) {
        if (m[i].valid && m[i].file == file && m[i].sector == sector) { m[i].stamp = ++*clock; return 0; }
        if (!m[i].valid) { victim = i; oldest = 0; }
        else if (m[i].stamp < oldest) { oldest = m[i].stamp; victim = i; }
    }
    m[victim].valid = 1; m[victim].file = file; m[victim].sector = sector; m[victim].stamp = ++*clock;
    return 1;
}

/* ---- Тесты ---------------------------------------------------------------- */

static void test_basic_and_hits(void)
{
    purrgo_file_t *f = open_file(0);
    CHECK(f != NULL);

    stats_reset();
    CHECK(read_check(f, 0, 0, 100));
    CHECK(g_nread == 1);
    CHECK(read_check(f, 0, 50, 100));      /* тот же сектор: из кэша */
    CHECK(g_nread == 1);
    CHECK(read_check(f, 0, 500, 100));     /* через границу секторов: ещё один сектор */
    CHECK(g_nread == 2);
    CHECK(read_check(f, 0, 300u * 512u, 137));            /* неполный последний сектор */
    CHECK(read_check(f, 0, 300u * 512u + 100, 100));      /* чтение за концом файла: короткий результат */
    CHECK(read_check(f, 0, 5000, 2000));
    purrgo_fs_close(f);
}

static void test_lru_eviction(void)
{
    /* Кэш на 16 секторов в памяти теста: порядок вытеснения LRU. */
    const size_t bytes = purrgo_fs_cache_bytes(16);
    CHECK(bytes > 16u * 512u);
    void *mem = aligned_alloc(8, (bytes + 7u) & ~(size_t)7u);
    CHECK(purrgo_fs_cache_attach(mem, bytes, 16));

    purrgo_file_t *f = open_file(1);
    for (uint32_t s = 0; s < 16; s++) CHECK(read_check(f, 1, s * 512u, 8));
    stats_reset();
    CHECK(read_check(f, 1, 0, 8));         /* сектор 0 - в кэше, и теперь самый свежий */
    CHECK(g_nread == 0);
    CHECK(read_check(f, 1, 16u * 512u, 8)); /* 17-й сектор вытесняет самый давний - сектор 1 */
    CHECK(g_nread == 1);
    stats_reset();
    CHECK(read_check(f, 1, 0, 8));         /* сектор 0 пережил вытеснение */
    CHECK(g_nread == 0);
    CHECK(read_check(f, 1, 1u * 512u, 8)); /* сектор 1 вытеснен */
    CHECK(g_nread == 1);
    purrgo_fs_close(f);

    purrgo_fs_cache_detach();
    free(mem);
}

static void test_two_files_no_collision(void)
{
    purrgo_file_t *a = open_file(0), *b = open_file(3);
    CHECK(read_check(a, 0, 7u * 512u, 64));
    CHECK(read_check(b, 3, 7u * 512u, 64));      /* тот же номер сектора, другой файл */
    stats_reset();
    CHECK(read_check(a, 0, 7u * 512u + 10, 64));
    CHECK(read_check(b, 3, 7u * 512u + 10, 64));
    CHECK(g_nread == 0);
    purrgo_fs_close(a);
    purrgo_fs_close(b);
}

static void scan_sectors(purrgo_file_t *f, int file, uint32_t from, uint32_t count)
{
    for (uint32_t s = from; s < from + count; s++) CHECK(read_check(f, file, s * 512u, 16));
}

static void test_big_cache_persists_between_frames(void)
{
    const size_t bytes = purrgo_fs_cache_bytes(768);
    void *mem = aligned_alloc(8, (bytes + 7u) & ~(size_t)7u);

    /* Малый кэш (64): повторный проход по 200 секторам вытесняется полностью. */
    purrgo_file_t *f = open_file(3);
    scan_sectors(f, 3, 0, 200);
    stats_reset();
    scan_sectors(f, 3, 0, 200);
    CHECK(g_nread == 200);
    purrgo_fs_close(f);

    /* Большой кэш (768): второй проход - ноль обращений к SD. */
    CHECK(purrgo_fs_cache_attach(mem, bytes, 768));
    f = open_file(3);
    scan_sectors(f, 3, 0, 500);
    stats_reset();
    scan_sectors(f, 3, 0, 500);
    CHECK(g_nread == 0);
    purrgo_fs_close(f);

    /* Закрытие файла сбрасывает его записи; переоткрытие читает заново. */
    f = open_file(3);
    stats_reset();
    scan_sectors(f, 3, 0, 10);
    CHECK(g_nread == 10);

    /* Отключение: данные доступны через малый кэш, память арены можно освободить. */
    purrgo_fs_cache_detach();
    memset(mem, 0xEE, bytes);
    free(mem);
    scan_sectors(f, 3, 0, 100);
    purrgo_fs_close(f);
}

static void test_reopen_invalidates(void)
{
    purrgo_file_t *f = open_file(2);
    CHECK(read_check(f, 2, 0, 100));
    purrgo_fs_close(f);
    g_epoch = 5;                          /* содержимое на карте изменилось */
    f = open_file(2);
    CHECK(read_check(f, 2, 0, 100));     /* без инвалидации вернулись бы старые данные */
    purrgo_fs_close(f);
    g_epoch = 0;
}

static void test_adaptive_read_ahead(void)
{
    purrgo_file_t *f = open_file(3);
    purrgo_fs_set_adaptive_read_ahead(f, 8);

    /* Чтение подряд: блоки 1, 2, 4, 8, 8, ... секторов. */
    stats_reset();
    scan_sectors(f, 3, 100, 80);
    CHECK(g_read_len[0] == 1);
    CHECK(g_read_len[1] == 2);
    CHECK(g_read_len[2] == 4);
    CHECK(g_read_len[3] == 8);
    CHECK(g_read_len[4] == 8);
    CHECK(g_read_off[1] == 101 && g_read_off[2] == 103 && g_read_off[3] == 107);   /* блоки идут встык */
    CHECK(g_sectors_read >= 80 && g_sectors_read <= 80 + 8);
    CHECK(g_nread < 20);                  /* вместо 80 одиночных чтений */

    /* Случайный доступ: каждый промах - один сектор. */
    stats_reset();
    const uint32_t jumps[] = { 400, 300, 500, 250, 333, 590, 550, 450 };
    for (size_t i = 0; i < sizeof jumps / sizeof jumps[0]; i++) {
        CHECK(read_check(f, 3, jumps[i] * 512u, 16));
    }
    CHECK(g_nread == 8);
    CHECK(g_sectors_read == 8);

    /* Случайный промах сбрасывает рост: после него подряд идущие снова начинают с 1. */
    stats_reset();
    scan_sectors(f, 3, 200, 6);
    CHECK(g_read_len[0] == 1 && g_read_len[1] == 2 && g_read_len[2] == 4);
    purrgo_fs_close(f);

    /* Блок у конца файла: последний неполный сектор читается верно. */
    f = open_file(0);
    purrgo_fs_set_adaptive_read_ahead(f, 8);
    scan_sectors(f, 0, 290, 11);
    CHECK(read_check(f, 0, 300u * 512u, 137));
    purrgo_fs_close(f);

    /* Статический read-ahead (mlp): блок выровнен и фиксирован. */
    f = open_file(1);
    purrgo_fs_set_read_ahead(f, 4);
    stats_reset();
    CHECK(read_check(f, 1, 5u * 512u + 3, 8));
    CHECK(g_nread == 1 && g_read_len[0] == 4 && g_read_off[0] == 4);
    purrgo_fs_close(f);
}

static void run_model(int use_big, uint32_t entries, uint32_t seed)
{
    void *mem = NULL;
    if (use_big) {
        const size_t bytes = purrgo_fs_cache_bytes(entries);
        mem = aligned_alloc(8, (bytes + 7u) & ~(size_t)7u);
        CHECK(purrgo_fs_cache_attach(mem, bytes, entries));
    }

    purrgo_file_t *f[3] = { open_file(0), open_file(1), open_file(3) };
    const int idx[3] = { 0, 1, 3 };
    const uint32_t sectors[3] = { 300, 120, 600 };
    mdl_t *m = calloc(entries, sizeof(mdl_t));
    uint32_t clock = 0, model = 0;
    g_rng = seed;
    stats_reset();

    for (int i = 0; i < 20000; i++) {
        const int k = (int)(rnd() % 3);
        /* Локальные перемещения (рабочее множество больше кэша) и редкие прыжки. */
        static uint32_t pos[3] = { 10, 10, 10 };
        if (rnd() % 8 == 0) pos[k] = rnd() % sectors[k];
        else pos[k] = (pos[k] + rnd() % 5) % sectors[k];
        CHECK(read_check(f[k], idx[k], pos[k] * 512u + 3, 4));
        model += model_misses(m, entries, &clock, k, pos[k]);
    }
    CHECK(g_nread == model);               /* число чтений SD равно модели LRU */
    if (g_nread != model) printf("  model=%u real=%u entries=%u\n", model, g_nread, entries);

    free(m);
    for (int i = 0; i < 3; i++) purrgo_fs_close(f[i]);
    if (use_big) {
        purrgo_fs_cache_detach();
        free(mem);
    }
}

static void test_random_matches_model(void)
{
    run_model(0, 64, 1);          /* встроенный кэш, как есть (64) */
    run_model(1, 16, 2);
    run_model(1, 100, 3);
    run_model(1, 768, 4);
}

static void test_attach_limits(void)
{
    CHECK(purrgo_fs_cache_bytes(0) == 0);
    CHECK(purrgo_fs_cache_bytes(8) == 0);        /* блок read-ahead должен помещаться */
    CHECK(purrgo_fs_cache_bytes(100000) == 0);
    CHECK(purrgo_fs_cache_bytes(768) > 768u * 512u);
    CHECK(purrgo_fs_cache_bytes(768) < 410u * 1024u);   /* в 512 КБ арены остаётся запас */
    uint8_t small[64];
    CHECK(!purrgo_fs_cache_attach(small, sizeof small, 768));   /* мало памяти */
    CHECK(!purrgo_fs_cache_attach(NULL, 1u << 20, 768));
    purrgo_fs_cache_detach();                                    /* без attach - безвредно */
}

int main(void)
{
    test_attach_limits();
    test_basic_and_hits();
    test_lru_eviction();
    test_two_files_no_collision();
    test_big_cache_persists_between_frames();
    test_reopen_invalidates();
    test_adaptive_read_ahead();
    test_random_matches_model();
    return test_finish("test_fs_cache_stm32");
}
