/*
 * microSD-карта PurrGO: SDIO (4 бита) + FatFs. См. sd_card.h.
 *
 * Пины SDIO (CubeMX):
 *
 *   PB15 SDIO_CK   PA6 SDIO_CMD
 *   PB4  SDIO_D0   PA8 SDIO_D1   PA9 SDIO_D2   PB5 SDIO_D3
 *   PB12 Detect_SDIO (обычный GPIO-вход)
 */

#include "sd_card.h"

#include "main.h"
#include "fatfs.h"
#include "sdio.h"
#include "bsp_driver_sd.h"
#include "purrgo/logger.h"

#include <stddef.h>
#include <string.h>

/* Размер блока SD (SDHC/SDXC - всегда 512). */
#define PURRGO_SD_BLOCK_SIZE 512U

/* Период опроса вывода Detect и состояния карты. Изменение уровня Detect
 * принимается после двух одинаковых отсчётов подряд (антидребезг). */
#ifndef PURRGO_SD_POLL_PERIOD_MS
#define PURRGO_SD_POLL_PERIOD_MS 250U
#endif

/* Период повторных попыток монтирования, если карта вставлена, но не
 * монтируется. Каждая неудачная попытка блокирует цикл на сотни мс. */
#ifndef PURRGO_SD_RETRY_PERIOD_MS
#define PURRGO_SD_RETRY_PERIOD_MS 10000U
#endif

/* Делитель тактовой частоты SDIO после идентификации карты:
 * SDIO_CK = SDIOCLK / (div + 2). При SDIOCLK = 42 МГц: 0 -> 21 МГц,
 * 2 -> 10.5 МГц, 4 -> 7 МГц. Если на проводном макете 21 МГц даёт сбои
 * чтения/записи, задайте 2 в compile definitions. */
#ifndef PURRGO_SD_CLOCK_DIV
#define PURRGO_SD_CLOCK_DIV 0U
#endif

/*
 * Верхняя граница времени одного обмена, мс: база + на каждый блок.
 * Значение, пришедшее из sd_diskio.c, ограничивается этой границей
 * (там оно ~27 часов).
 */
#ifndef PURRGO_SD_READ_TIMEOUT_BASE_MS
#define PURRGO_SD_READ_TIMEOUT_BASE_MS   1000U
#endif
#ifndef PURRGO_SD_READ_TIMEOUT_PER_BLOCK_MS
#define PURRGO_SD_READ_TIMEOUT_PER_BLOCK_MS 5U
#endif
#ifndef PURRGO_SD_WRITE_TIMEOUT_BASE_MS
#define PURRGO_SD_WRITE_TIMEOUT_BASE_MS  2000U
#endif
#ifndef PURRGO_SD_WRITE_TIMEOUT_PER_BLOCK_MS
#define PURRGO_SD_WRITE_TIMEOUT_PER_BLOCK_MS 20U
#endif

/* Сколько карта может непрерывно оставаться "занятой" (программирование
 * flash), прежде чем она будет считаться потерянной. */
#ifndef PURRGO_SD_BUSY_TIMEOUT_MS
#define PURRGO_SD_BUSY_TIMEOUT_MS 3000U
#endif

/* Пауза между вызовами BSP_SD_GetCardState(), после которой отсчёт
 * "непрерывной занятости" начинается заново. */
#define PURRGO_SD_BUSY_GAP_MS 100U

static FATFS s_fs;
static purrgo_sd_state_t s_state = PURRGO_SD_NO_CARD;

/* Карта потеряна (извлечена или не отвечает). Сбрасывается при монтировании. */
static volatile bool s_card_lost;
static volatile bool s_lost_released;

/* Антидребезг Detect. */
static bool s_present_candidate;
static bool s_present_stable;
static bool s_present_prev;

static uint32_t s_last_poll_ms;
static bool s_retry_armed;
static uint32_t s_retry_at_ms;

/* Учёт непрерывной занятости карты (BSP_SD_GetCardState). */
static bool s_busy_active;
static uint32_t s_busy_since_ms;
static uint32_t s_busy_last_call_ms;

/* Выровненный буфер для обмена невыровненными буферами FatFs. */
static uint32_t s_bounce[PURRGO_SD_BLOCK_SIZE / 4U];


/*
 * --------------------------------------------------------------------------
 * Detect
 * --------------------------------------------------------------------------
 */

static bool card_inserted(void)
{
#if PURRGO_SD_DETECT_MODE == PURRGO_SD_DETECT_NONE
    return true;
#else
    const GPIO_PinState level =
        HAL_GPIO_ReadPin(Detect_SDIO_GPIO_Port, Detect_SDIO_Pin);

#if PURRGO_SD_DETECT_MODE == PURRGO_SD_DETECT_ACTIVE_LOW
    return (level == GPIO_PIN_RESET);
#else
    return (level == GPIO_PIN_SET);
#endif
#endif
}

static void card_lost(const char *reason)
{
    if (!s_card_lost)
    {
        s_card_lost = true;
        s_lost_released = false;
        PURRGO_LOG("SD: card lost (%s)\r\n", reason);
    }
}


/*
 * --------------------------------------------------------------------------
 * Переопределения слабых функций BSP (FATFS/Target/bsp_driver_sd.c)
 * --------------------------------------------------------------------------
 *
 * Прототипы - в bsp_driver_sd.h. Сильные определения здесь заменяют __weak
 * версии CubeMX при линковке; при повторной генерации кода они сохраняются.
 */

uint8_t BSP_SD_IsDetected(void)
{
    return card_inserted() ? SD_PRESENT : SD_NOT_PRESENT;
}

/*
 * Замена слабой BSP_SD_Init() с диагностикой: в логе видно, на каком этапе
 * (идентификация карты или переключение на 4 бита) и с каким кодом ошибки
 * HAL (hsd.ErrorCode, биты HAL_SD_ERROR_*) не удалось инициализировать карту.
 *
 * Если определить PURRGO_SD_FORCE_1BIT, шина остаётся 1-битной (проверка
 * проводки D1..D3).
 */
uint8_t BSP_SD_Init(void)
{
    if (BSP_SD_IsDetected() != SD_PRESENT)
    {
        PURRGO_LOG("SD: init: card not detected\r\n");
        return MSD_ERROR;
    }

    if (HAL_SD_Init(&hsd) != HAL_OK)
    {
        PURRGO_LOG("SD: HAL_SD_Init FAILED, state %lu, err 0x%08lX\r\n",
                   (unsigned long)hsd.State,
                   (unsigned long)HAL_SD_GetError(&hsd));
        return MSD_ERROR;
    }

#ifndef PURRGO_SD_FORCE_1BIT
    if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) != HAL_OK)
    {
        PURRGO_LOG("SD: 4-bit bus FAILED, state %lu, err 0x%08lX\r\n",
                   (unsigned long)hsd.State,
                   (unsigned long)HAL_SD_GetError(&hsd));
        return MSD_ERROR;
    }
#endif

    return MSD_OK;
}

/* Ожидание возврата карты в состояние TRANSFER с таймаутом. */
static bool wait_card_ready(uint32_t timeout_ms)
{
    const uint32_t start = HAL_GetTick();

    while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER)
    {
        if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms)
        {
            return false;
        }
    }
    return true;
}

/*
 * Обмен блоками в режиме опроса с ограничением по времени и с поддержкой
 * невыровненных буферов.
 */
static uint8_t sd_transfer(
    bool write,
    uint8_t *buf,
    uint32_t block,
    uint32_t count,
    uint32_t caller_timeout_ms
)
{
    if (s_card_lost || hsd.State == HAL_SD_STATE_RESET || count == 0U || buf == NULL)
    {
        return MSD_ERROR;
    }

    const uint32_t limit = write
        ? (PURRGO_SD_WRITE_TIMEOUT_BASE_MS + count * PURRGO_SD_WRITE_TIMEOUT_PER_BLOCK_MS)
        : (PURRGO_SD_READ_TIMEOUT_BASE_MS  + count * PURRGO_SD_READ_TIMEOUT_PER_BLOCK_MS);
    const uint32_t timeout = (caller_timeout_ms < limit) ? caller_timeout_ms : limit;

    HAL_StatusTypeDef status = HAL_OK;

    if ((((uintptr_t)buf) & 3U) == 0U)
    {
        status = write
            ? HAL_SD_WriteBlocks(&hsd, buf, block, count, timeout)
            : HAL_SD_ReadBlocks(&hsd, buf, block, count, timeout);
    }
    else
    {
        /* Буфер не выровнен по 4 байтам: по одному блоку через s_bounce. */
        for (uint32_t i = 0U; (i < count) && (status == HAL_OK); i++)
        {
            uint8_t *p = buf + ((size_t)i * PURRGO_SD_BLOCK_SIZE);

            if (write)
            {
                memcpy(s_bounce, p, PURRGO_SD_BLOCK_SIZE);
                status = HAL_SD_WriteBlocks(&hsd, (uint8_t *)s_bounce, block + i, 1U, timeout);

                /* Следующая команда записи возможна только в состоянии TRANSFER. */
                if ((status == HAL_OK) && ((i + 1U) < count) && !wait_card_ready(timeout))
                {
                    status = HAL_TIMEOUT;
                }
            }
            else
            {
                status = HAL_SD_ReadBlocks(&hsd, (uint8_t *)s_bounce, block + i, 1U, timeout);
                if (status == HAL_OK)
                {
                    memcpy(p, s_bounce, PURRGO_SD_BLOCK_SIZE);
                }
            }
        }
    }

    if (status != HAL_OK)
    {
        if (!card_inserted())
        {
            card_lost("removed during transfer");
        }
        return MSD_ERROR;
    }

    return MSD_OK;
}

uint8_t BSP_SD_ReadBlocks(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks, uint32_t Timeout)
{
    return sd_transfer(false, (uint8_t *)pData, ReadAddr, NumOfBlocks, Timeout);
}

uint8_t BSP_SD_WriteBlocks(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks, uint32_t Timeout)
{
    return sd_transfer(true, (uint8_t *)pData, WriteAddr, NumOfBlocks, Timeout);
}

/*
 * sd_diskio.c ждёт готовности карты циклом
 *
 *     while (BSP_SD_GetCardState() != MSD_OK) { }
 *
 * без таймаута. Если карту извлекли (или она зависла) во время записи,
 * цикл не завершится никогда. Поэтому здесь отслеживается, как долго карта
 * непрерывно "занята", и при превышении PURRGO_SD_BUSY_TIMEOUT_MS (или при
 * пропадании сигнала Detect) карта объявляется потерянной.
 *
 * Потеря фиксируется (s_card_lost). Первый вызов после этого возвращает OK,
 * чтобы выйти из цикла ожидания в sd_diskio.c; все дальнейшие обмены сразу
 * возвращают ошибку, а состояние диска - "не готов", пока purrgo_sd_poll()
 * не размонтирует FatFs и не смонтирует карту заново.
 */
uint8_t BSP_SD_GetCardState(void)
{
    const uint32_t now = HAL_GetTick();

    if (s_card_lost)
    {
        if (!s_lost_released)
        {
            s_lost_released = true;
            return SD_TRANSFER_OK;
        }
        return SD_TRANSFER_BUSY;
    }

    /* SDIO не инициализирован (карты нет): команда CMD13 зависла бы надолго. */
    if (hsd.State == HAL_SD_STATE_RESET)
    {
        return SD_TRANSFER_BUSY;
    }

    if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER)
    {
        s_busy_active = false;
        return SD_TRANSFER_OK;
    }

    if (!s_busy_active ||
        ((uint32_t)(now - s_busy_last_call_ms) > PURRGO_SD_BUSY_GAP_MS))
    {
        s_busy_active = true;
        s_busy_since_ms = now;
    }
    s_busy_last_call_ms = now;

    if (!card_inserted())
    {
        card_lost("removed");
    }
    else if ((uint32_t)(now - s_busy_since_ms) >= PURRGO_SD_BUSY_TIMEOUT_MS)
    {
        card_lost("busy timeout");
    }

    return SD_TRANSFER_BUSY;
}


/*
 * --------------------------------------------------------------------------
 * Монтирование
 * --------------------------------------------------------------------------
 */

static void log_card_info(void)
{
    HAL_SD_CardInfoTypeDef info;

    if (HAL_SD_GetCardInfo(&hsd, &info) != HAL_OK)
    {
        return;
    }

    const uint32_t capacity_mb =
        (uint32_t)(((uint64_t)info.LogBlockNbr * info.LogBlockSize) >> 20);

    /* hsd.Init.BusWide HAL не обновляет при переключении на 4 бита, поэтому
     * реальные ширину шины и делитель читаем из регистра CLKCR. */
    const uint32_t clkcr = SDIO->CLKCR;
    const uint32_t wid = clkcr & SDIO_CLKCR_WIDBUS;
    const uint32_t clkdiv = clkcr & SDIO_CLKCR_CLKDIV;

    PURRGO_LOG(
        "SD: %s, %lu MB, block %lu, bus %s, SDIO clkdiv %lu\r\n",
        (info.CardType == CARD_SDHC_SDXC) ? "SDHC/SDXC" : "SDSC",
        (unsigned long)capacity_mb,
        (unsigned long)info.LogBlockSize,
        (wid == SDIO_BUS_WIDE_4B) ? "4-bit" :
            ((wid == SDIO_BUS_WIDE_8B) ? "8-bit" : "1-bit"),
        (unsigned long)clkdiv
    );
}

static void make_dir(const char *path)
{
    const FRESULT res = f_mkdir(path);

    if ((res != FR_OK) && (res != FR_EXIST))
    {
        PURRGO_LOG("SD: mkdir %s error: %d\r\n", path, (int)res);
    }
}

static void unmount(void)
{
    (void)f_mount(NULL, "0:", 0);

    /* Выключает SDIO (питание, тактирование, пины, DMA) и сбрасывает HAL. */
    (void)HAL_SD_DeInit(&hsd);

    s_busy_active = false;
    s_state = PURRGO_SD_NO_CARD;
}

static bool mount_card(void)
{
    s_card_lost = false;
    s_lost_released = false;
    s_busy_active = false;

    /*
     * Идентификация карты всегда идёт по 1 биту; на 4 бита шину переключает
     * BSP_SD_Init() после HAL_SD_Init().
     */
    hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
    hsd.Init.ClockDiv = PURRGO_SD_CLOCK_DIV;

    /* opt = 1: смонтировать немедленно (инициализация диска + чтение FAT). */
    const FRESULT res = f_mount(&s_fs, "0:", 1);

    if (res != FR_OK)
    {
        PURRGO_LOG("SD: mount ERROR: %d (FatFs FRESULT), hsd state %lu, err 0x%08lX\r\n",
                   (int)res,
                   (unsigned long)hsd.State,
                   (unsigned long)HAL_SD_GetError(&hsd));
        unmount();
        s_state = PURRGO_SD_MOUNT_ERROR;
        return false;
    }

    log_card_info();

    /* Базовая структура каталогов. */
    make_dir("0:/PURRGO");
    make_dir("0:/PURRGO/MAPS");
    make_dir("0:/PURRGO/TRACKS");
    make_dir("0:/PURRGO/WAYPTS");

    s_state = PURRGO_SD_READY;
    PURRGO_LOG("SD: mount OK, tree verified\r\n");
    return true;
}


/*
 * --------------------------------------------------------------------------
 * Публичный API
 * --------------------------------------------------------------------------
 */

bool purrgo_sd_init(void)
{
#if PURRGO_SD_DETECT_MODE != PURRGO_SD_DETECT_NONE
    PURRGO_LOG("SD: Detect_SDIO level %d\r\n",
               (int)HAL_GPIO_ReadPin(Detect_SDIO_GPIO_Port, Detect_SDIO_Pin));
#endif

    s_present_stable = card_inserted();
    s_present_candidate = s_present_stable;
    s_present_prev = s_present_stable;
    s_retry_armed = false;

    if (!s_present_stable)
    {
        s_state = PURRGO_SD_NO_CARD;
        PURRGO_LOG("SD: no card (Detect_SDIO)\r\n");
        return false;
    }

    const bool ok = mount_card();

    if (!ok)
    {
        s_retry_armed = true;
        s_retry_at_ms = HAL_GetTick() + PURRGO_SD_RETRY_PERIOD_MS;
    }

    return ok;
}

void purrgo_sd_poll(uint32_t now_ms)
{
    if ((uint32_t)(now_ms - s_last_poll_ms) < PURRGO_SD_POLL_PERIOD_MS)
    {
        return;
    }
    s_last_poll_ms = now_ms;

#if PURRGO_SD_DETECT_MODE != PURRGO_SD_DETECT_NONE
    /* Антидребезг: новое значение принимается со второго одинакового отсчёта. */
    const bool raw = card_inserted();
    if (raw == s_present_candidate)
    {
        s_present_stable = raw;
    }
    s_present_candidate = raw;

    const bool inserted_event = s_present_stable && !s_present_prev;
    s_present_prev = s_present_stable;

    if (s_state == PURRGO_SD_READY)
    {
        if (!s_present_stable)
        {
            PURRGO_LOG("SD: card removed\r\n");
            unmount();
        }
        else if (s_card_lost)
        {
            PURRGO_LOG("SD: I/O failure, remounting later\r\n");
            unmount();
            s_state = PURRGO_SD_MOUNT_ERROR;
            s_retry_armed = true;
            s_retry_at_ms = now_ms + PURRGO_SD_RETRY_PERIOD_MS;
        }
        return;
    }

    /* Файловая система не смонтирована. */
    if (!s_present_stable)
    {
        s_state = PURRGO_SD_NO_CARD;
        s_retry_armed = false;
        return;
    }

    if (inserted_event)
    {
        PURRGO_LOG("SD: card inserted\r\n");
    }

    if (inserted_event ||
        (s_retry_armed && ((int32_t)(now_ms - s_retry_at_ms) >= 0)))
    {
        if (mount_card())
        {
            s_retry_armed = false;
        }
        else
        {
            s_retry_armed = true;
            s_retry_at_ms = now_ms + PURRGO_SD_RETRY_PERIOD_MS;
        }
    }
#else
    /*
     * Нет вывода Detect: вставка не отслеживается, но неудачное монтирование
     * (и потеря карты при обмене) повторяется каждые PURRGO_SD_RETRY_PERIOD_MS.
     */
    if (s_state == PURRGO_SD_READY)
    {
        if (s_card_lost)
        {
            PURRGO_LOG("SD: I/O failure, remounting later\r\n");
            unmount();
            s_state = PURRGO_SD_MOUNT_ERROR;
            s_retry_armed = true;
            s_retry_at_ms = now_ms + PURRGO_SD_RETRY_PERIOD_MS;
        }
        return;
    }

    if (s_retry_armed && ((int32_t)(now_ms - s_retry_at_ms) >= 0))
    {
        if (mount_card())
        {
            s_retry_armed = false;
        }
        else
        {
            s_retry_at_ms = now_ms + PURRGO_SD_RETRY_PERIOD_MS;
        }
    }
#endif
}

purrgo_sd_state_t purrgo_sd_state(void)
{
    return s_state;
}

bool purrgo_sd_is_ready(void)
{
    return (s_state == PURRGO_SD_READY);
}
