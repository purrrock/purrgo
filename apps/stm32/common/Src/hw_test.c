/*
 * Аппаратные тесты (замеры на железе). См. hw_test.h.
 *
 * Тест SD_SPEED. Все замеры по DWT->CYCCNT (разрешение 1 такт), результат
 * печатается в UART1. Карта читается только на чтение: ни один сектор не
 * записывается и файлы не создаются.
 *
 *   A. Сырое последовательное чтение секторов (disk_read -> sd_spi_read):
 *      CMD17 при count = 1, CMD18 при count > 1. Показывает потолок SPI и
 *      цену команды на каждый запрос.
 *   B. Случайное чтение одного сектора: задержка одного обращения
 *      (среднее / минимум / максимум). Так читается индекс карты.
 *   C. FatFs: последовательное чтение файла с карты блоками 512 Б / 4 КБ /
 *      16 КБ, затем случайные seek+read 512 Б без CLMT и с CLMT.
 *      Файл берётся из 0:/PURRGO/MAPS (самый большой, но не больше
 *      PURRGO_HWTEST_SD_FILE_SCAN_MAX файлов); можно задать явно:
 *      -DPURRGO_HWTEST_SD_FILE=\"0:/PURRGO/MAPS/minsk.idx\".
 *
 *   D. Сверка быстрого обмена по регистрам SPI2 с обменом через HAL: те же
 *      сектора читаются обоими способами и сравниваются (до замеров A-C).
 *
 * Скорость печатается в МБ/с (1 МБ = 10^6 Б) и в процентах от теоретического
 * предела SPI (f_spi / 8 Б/с).
 */

#include "hw_test.h"

#ifdef PURRGO_HW_TEST_ENABLED

#include "main.h"
#include "purrgo/logger.h"

#if defined(PURRGO_HW_TEST_SD_SPEED)

#include "spi.h"
#include "ff.h"
#include "diskio.h"
#include "sd_card.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- параметры теста (переопределяются через -D) ------------------------ */

/* Объём одного замера последовательного чтения, секторов (2048 = 1 МиБ). */
#ifndef PURRGO_HWTEST_SD_SEQ_SECTORS
#define PURRGO_HWTEST_SD_SEQ_SECTORS 2048U
#endif

/* Число случайных обращений в замерах B и C. */
#ifndef PURRGO_HWTEST_SD_RANDOM_READS
#define PURRGO_HWTEST_SD_RANDOM_READS 512U
#endif

/* Первый сектор области сырого чтения (после MBR и загрузочных структур). */
#ifndef PURRGO_HWTEST_SD_START_SECTOR
#define PURRGO_HWTEST_SD_START_SECTOR 8192U
#endif

/* Сколько байт файла читать в последовательном замере FatFs. */
#ifndef PURRGO_HWTEST_SD_FILE_BYTES
#define PURRGO_HWTEST_SD_FILE_BYTES (1024UL * 1024UL)
#endif

#ifndef PURRGO_HWTEST_SD_DIR
#define PURRGO_HWTEST_SD_DIR "0:/PURRGO/MAPS"
#endif

/* Ограничение числа просматриваемых записей каталога. */
#ifndef PURRGO_HWTEST_SD_FILE_SCAN_MAX
#define PURRGO_HWTEST_SD_FILE_SCAN_MAX 64U
#endif

#define HT(...) purrgo_logger_write(__VA_ARGS__)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define BUF_BYTES   16384U
#define BUF_SECTORS (BUF_BYTES / 512U)

static uint8_t s_buf[BUF_BYTES] __attribute__((aligned(4)));
static DWORD s_clmt[256];
static FILINFO s_fno;

/* ---- измерение времени -------------------------------------------------- */

static void cyc_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t cyc(void)
{
    return DWT->CYCCNT;
}

/* Один замер не должен превышать 2^32 тактов (53 с при 80 МГц). */
static uint32_t cycles_to_us(uint32_t cycles)
{
    return (uint32_t)(((uint64_t)cycles * 1000000ULL) / SystemCoreClock);
}

static uint32_t spi_hz(void)
{
    /* SPI2 тактируется от PCLK1; прескалер: 0x00000000 = /2, 0x10000000 = /4, ... */
    const uint32_t div = 2UL << (hspi2.Init.BaudRatePrescaler >> 28);

    return HAL_RCC_GetPCLK1Freq() / div;
}

static void report_rate(const char *name, uint32_t bytes, uint32_t cycles)
{
    const uint32_t us = cycles_to_us(cycles);

    if (us == 0U)
    {
        HT("HWTEST SD %s: too fast to measure\r\n", name);
        return;
    }

    /* байт/мкс = МБ/с; в тысячных долях получаем кБ/с. */
    const uint32_t kBps = (uint32_t)(((uint64_t)bytes * 1000ULL) / us);
    const uint32_t limit_kBps = spi_hz() / 8U / 1000U;
    const uint32_t pct = (limit_kBps != 0U) ? (kBps * 100U) / limit_kBps : 0U;

    HT("HWTEST SD %-28s %7lu B in %7lu us = %lu.%03lu MB/s (%lu%% of SPI limit)\r\n",
       name, (unsigned long)bytes, (unsigned long)us,
       (unsigned long)(kBps / 1000U), (unsigned long)(kBps % 1000U),
       (unsigned long)pct);
}

static uint32_t s_rng = 0x2545F491U;

static uint32_t rnd(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

/* ---- A. сырое последовательное чтение ----------------------------------- */

static void test_raw_sequential(uint32_t sector_count)
{
    static const uint8_t chunks[] = { 1U, 2U, 8U, 32U };

    for (size_t i = 0; i < ARRAY_SIZE(chunks); i++)
    {
        const uint32_t chunk = chunks[i];
        const uint32_t total = PURRGO_HWTEST_SD_SEQ_SECTORS;
        const uint32_t sector = PURRGO_HWTEST_SD_START_SECTOR;
        char name[32];

        if ((sector + total) > sector_count)
        {
            HT("HWTEST SD raw seq skipped: card too small\r\n");
            return;
        }

        /* Прогрев: первая команда после паузы может быть медленнее. */
        (void)disk_read(0, s_buf, sector, 1U);

        const uint32_t t0 = cyc();
        bool ok = true;

        for (uint32_t done = 0U; done < total; done += chunk)
        {
            if (disk_read(0, s_buf, sector + done, chunk) != RES_OK)
            {
                ok = false;
                break;
            }
        }
        const uint32_t dt = cyc() - t0;

        (void)snprintf(name, sizeof(name), "raw seq %2lu sect/cmd",
                       (unsigned long)chunk);
        if (ok)
        {
            report_rate(name, total * 512U, dt);
        }
        else
        {
            HT("HWTEST SD %s: READ ERROR\r\n", name);
        }
    }
}

/* ---- B. сырое случайное чтение одного сектора --------------------------- */

static void test_raw_random(uint32_t sector_count)
{
    const uint32_t first = PURRGO_HWTEST_SD_START_SECTOR;
    const uint32_t span = sector_count - first;
    const uint32_t n = PURRGO_HWTEST_SD_RANDOM_READS;
    uint32_t sum = 0U;
    uint32_t tmin = UINT32_MAX;
    uint32_t tmax = 0U;

    if (sector_count <= first)
    {
        return;
    }

    for (uint32_t i = 0U; i < n; i++)
    {
        const uint32_t sector = first + (rnd() % span);
        const uint32_t t0 = cyc();

        if (disk_read(0, s_buf, sector, 1U) != RES_OK)
        {
            HT("HWTEST SD raw random: READ ERROR at sector %lu\r\n",
               (unsigned long)sector);
            return;
        }
        const uint32_t dt = cyc() - t0;

        sum += cycles_to_us(dt);
        if (dt < tmin) { tmin = dt; }
        if (dt > tmax) { tmax = dt; }
    }

    const uint32_t avg_us = sum / n;

    HT("HWTEST SD raw random 1 sect: n=%lu avg=%lu us min=%lu us max=%lu us (%lu reads/s)\r\n",
       (unsigned long)n, (unsigned long)avg_us,
       (unsigned long)cycles_to_us(tmin), (unsigned long)cycles_to_us(tmax),
       (unsigned long)((avg_us != 0U) ? (1000000UL / avg_us) : 0UL));
}

/* ---- C. FatFs ----------------------------------------------------------- */

/* Самый большой обычный файл каталога dir (один уровень). */
static bool find_file(const char *dir, char *path, size_t path_size, FSIZE_t *size)
{
    DIR d;
    FSIZE_t best = 0;
    unsigned scanned = 0U;

    if (f_opendir(&d, dir) != FR_OK)
    {
        return false;
    }

    while ((scanned++ < PURRGO_HWTEST_SD_FILE_SCAN_MAX) &&
           (f_readdir(&d, &s_fno) == FR_OK) && (s_fno.fname[0] != '\0'))
    {
        const size_t dl = strlen(dir);
        const size_t nl = strlen(s_fno.fname);

        if (((s_fno.fattrib & AM_DIR) == 0U) && (s_fno.fsize > best) &&
            ((dl + 1U + nl + 1U) <= path_size))
        {
            best = s_fno.fsize;
            memcpy(path, dir, dl);
            path[dl] = '/';
            memcpy(&path[dl + 1U], s_fno.fname, nl + 1U);
        }
    }
    (void)f_closedir(&d);

    *size = best;
    return (best != 0U);
}

static void fatfs_random(FIL *f, FSIZE_t size, const char *label)
{
    const uint32_t n = PURRGO_HWTEST_SD_RANDOM_READS;
    const uint32_t slots = (uint32_t)((size - 512U) / 512U);
    uint32_t sum = 0U;
    uint32_t tmin = UINT32_MAX;
    uint32_t tmax = 0U;

    if (size < 1024U)
    {
        return;
    }

    for (uint32_t i = 0U; i < n; i++)
    {
        const FSIZE_t pos = (FSIZE_t)(rnd() % slots) * 512U;
        UINT br = 0U;
        const uint32_t t0 = cyc();

        if ((f_lseek(f, pos) != FR_OK) || (f_read(f, s_buf, 512U, &br) != FR_OK) ||
            (br != 512U))
        {
            HT("HWTEST SD FatFs random (%s): ERROR\r\n", label);
            return;
        }
        const uint32_t dt = cyc() - t0;

        sum += cycles_to_us(dt);
        if (dt < tmin) { tmin = dt; }
        if (dt > tmax) { tmax = dt; }
    }

    const uint32_t avg_us = sum / n;

    HT("HWTEST SD FatFs seek+read 512 (%s): n=%lu avg=%lu us min=%lu us max=%lu us\r\n",
       label, (unsigned long)n, (unsigned long)avg_us,
       (unsigned long)cycles_to_us(tmin), (unsigned long)cycles_to_us(tmax));
}

static void test_fatfs(void)
{
    static char path[96];
    FSIZE_t size = 0;
    FIL f;

#ifdef PURRGO_HWTEST_SD_FILE
    (void)snprintf(path, sizeof(path), "%s", PURRGO_HWTEST_SD_FILE);
    if ((f_stat(path, &s_fno) != FR_OK) || (s_fno.fsize == 0U))
    {
        HT("HWTEST SD FatFs: file %s not found\r\n", path);
        return;
    }
    size = s_fno.fsize;
#else
    if (!find_file(PURRGO_HWTEST_SD_DIR, path, sizeof(path), &size))
    {
        HT("HWTEST SD FatFs: no files in %s, skipped\r\n", PURRGO_HWTEST_SD_DIR);
        return;
    }
#endif

    HT("HWTEST SD FatFs file: %s, %lu bytes\r\n", path, (unsigned long)size);

    if (f_open(&f, path, FA_READ) != FR_OK)
    {
        HT("HWTEST SD FatFs: open failed\r\n");
        return;
    }

    /* Последовательное чтение. */
    static const uint16_t blocks[] = { 512U, 4096U, 16384U };
    const uint32_t want = (size < PURRGO_HWTEST_SD_FILE_BYTES)
                              ? (uint32_t)size : PURRGO_HWTEST_SD_FILE_BYTES;

    for (size_t i = 0; i < ARRAY_SIZE(blocks); i++)
    {
        const UINT bs = blocks[i];
        uint32_t got = 0U;
        bool ok = (f_lseek(&f, 0U) == FR_OK);
        char name[32];

        const uint32_t t0 = cyc();

        while (ok && (got < want))
        {
            UINT br = 0U;

            if ((f_read(&f, s_buf, bs, &br) != FR_OK) || (br == 0U))
            {
                ok = false;
                break;
            }
            got += br;
        }
        const uint32_t dt = cyc() - t0;

        (void)snprintf(name, sizeof(name), "FatFs seq f_read %5u B", (unsigned)bs);
        if (ok || (got >= want))
        {
            report_rate(name, got, dt);
        }
        else
        {
            HT("HWTEST SD %s: READ ERROR\r\n", name);
        }
    }

    /* Случайный доступ: без CLMT (цепочка FAT проходится заново) и с CLMT. */
    f.cltbl = NULL;
    fatfs_random(&f, size, "no CLMT");

    s_clmt[0] = (DWORD)(ARRAY_SIZE(s_clmt));
    f.cltbl = s_clmt;
    if (f_lseek(&f, CREATE_LINKMAP) == FR_OK)
    {
        HT("HWTEST SD FatFs CLMT ok: %lu entries used\r\n",
           (unsigned long)s_clmt[0]);
        fatfs_random(&f, size, "CLMT");
    }
    else
    {
        HT("HWTEST SD FatFs CLMT off: need %lu entries (have %lu), file fragmented\r\n",
           (unsigned long)s_clmt[0],
           (unsigned long)(ARRAY_SIZE(s_clmt)));
    }

    (void)f_close(&f);
}

/* ---- D. сверка быстрого обмена с HAL ------------------------------------ */

static uint8_t s_ref[32U * 512U] __attribute__((aligned(4)));

static void test_xfer_compare(uint32_t sector_count)
{
    const uint32_t sector = PURRGO_HWTEST_SD_START_SECTOR;
    const uint32_t count = 32U;
    static const uint8_t chunks[] = { 1U, 2U, 8U, 32U };
    bool ok = true;

    if ((sector + count) > sector_count)
    {
        HT("HWTEST SD xfer compare skipped: card too small\r\n");
        return;
    }

    purrgo_sd_spi_force_hal_xfer(true);
    const DRESULT r_ref = disk_read(0, s_ref, sector, count);
    purrgo_sd_spi_force_hal_xfer(false);

    if (r_ref != RES_OK)
    {
        HT("HWTEST SD xfer compare: HAL read ERROR\r\n");
        return;
    }

    for (size_t i = 0; i < ARRAY_SIZE(chunks); i++)
    {
        for (uint32_t done = 0U; done < count; done += chunks[i])
        {
            if (disk_read(0, &s_buf[done * 512U], sector + done, chunks[i]) != RES_OK)
            {
                HT("HWTEST SD xfer compare: fast read ERROR (%u sect/cmd)\r\n",
                   (unsigned)chunks[i]);
                ok = false;
                break;
            }
        }
        if (ok && (memcmp(s_buf, s_ref, sizeof s_ref) != 0))
        {
            HT("HWTEST SD xfer compare: MISMATCH (%u sect/cmd)\r\n", (unsigned)chunks[i]);
            ok = false;
        }
        if (!ok)
        {
            return;
        }
        memset(s_buf, 0, sizeof s_ref);
    }
    HT("HWTEST SD xfer compare: OK (fast == HAL, %lu sectors from %lu)\r\n",
       (unsigned long)count, (unsigned long)sector);
}

/* ---- сам тест ----------------------------------------------------------- */

static void run_sd_speed(void)
{
    DWORD sectors = 0U;

    HT("HWTEST SD_SPEED start\r\n");
    cyc_init();

    if (!purrgo_sd_init())
    {
        HT("HWTEST SD: card is not mounted (state %d), aborted\r\n",
           (int)purrgo_sd_state());
        return;
    }

    if (disk_ioctl(0, GET_SECTOR_COUNT, &sectors) != RES_OK)
    {
        HT("HWTEST SD: GET_SECTOR_COUNT failed, aborted\r\n");
        return;
    }

    HT("HWTEST SD config: SYSCLK=%lu Hz, SPI2=%lu Hz (prescaler 0x%08lX), "
       "limit=%lu kB/s, card=%lu MB\r\n",
       (unsigned long)SystemCoreClock, (unsigned long)spi_hz(),
       (unsigned long)hspi2.Init.BaudRatePrescaler,
       (unsigned long)(spi_hz() / 8U / 1000U), (unsigned long)(sectors >> 11));

    test_xfer_compare((uint32_t)sectors);
    test_raw_sequential((uint32_t)sectors);
    test_raw_random((uint32_t)sectors);
    test_fatfs();

    HT("HWTEST SD_SPEED done\r\n");
}

#else
#error "PURRGO_HW_TEST_ENABLED is set, but no PURRGO_HW_TEST_<NAME> is defined"
#endif

void purrgo_hw_test_run(void)
{
    purrgo_logger_init();

#if defined(PURRGO_HW_TEST_SD_SPEED)
    run_sd_speed();
#endif

    for (;;)
    {
        __WFI();
    }
}

#endif /* PURRGO_HW_TEST_ENABLED */
