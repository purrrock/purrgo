/*
 * microSD-карта PurrGO на STM32U585: SPI2 + FatFs. См. sd_card.h.
 *
 * Пины (CubeMX):
 *   PB13 SPI2_SCK   PB14 SPI2_MISO   PB15 SPI2_MOSI
 *   PB12 SD_CS (GPIO, активный уровень низкий)
 *   PB10 Detect_SD (GPIO-вход с подтяжкой)
 *
 * Слои:
 *   sd_spi (stm32/port/storage) - протокол карты, без HAL;
 *   этот файл - привязка к HAL SPI2/GPIO, диск FatFs (disk_*, интерфейс diskio.h),
 *     монтирование, горячая замена, защита от зависаний.
 */

#include "sd_card.h"

#include "main.h"
#include "spi.h"
#include "ff.h"
#include "diskio.h"
#include "sd_spi.h"
#include "sd_power.h"
#include "purrgo/logger.h"
#include "purrgo/hw_fault.h"
#include "purrgo/supply.h"
#include "purrgo/render_prof.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Делители SPI2 (SYSCLK = 80 МГц, SPI2 тактируется от PCLK1 = 80 МГц).
 * Инициализация карты - не быстрее 400 кГц: /256 = 312.5 кГц (как в CubeMX).
 * Рабочая частота: /8 = 10 МГц, после проверки на плате можно перейти на
 * SPI_BAUDRATEPRESCALER_4 (20 МГц), максимум по спецификации - 25 МГц.
 */
#define PURRGO_SD_SPI_INIT_PRESCALER SPI_BAUDRATEPRESCALER_256

#ifndef PURRGO_SD_SPI_RUN_PRESCALER
// #define PURRGO_SD_SPI_RUN_PRESCALER SPI_BAUDRATEPRESCALER_8
#define PURRGO_SD_SPI_RUN_PRESCALER SPI_BAUDRATEPRESCALER_4
#endif

/*
 * Чтение блоков данных на 40 МГц (SPI_BAUDRATEPRESCALER_2) после CMD6 (High Speed) и проверки
 * чтением при инициализации (sd_spi.c, verify_high_speed). Команды и запись остаются на
 * PURRGO_SD_SPI_RUN_PRESCALER. Даташит STM32U585 (таблица SPI characteristics): для range 2
 * значения fSCK нет, tv(MO) для PB15 до 12,5 нс; на чтении MOSI держится в 0xFF и не
 * переключается. 0 - выключить режим, остаётся 20 МГц.
 */
#ifndef PURRGO_SD_SPI_HS
#define PURRGO_SD_SPI_HS 1
#endif

/* Быстрый обмен по регистрам SPI2 (см. xfer_fast). 0 - только HAL. */
#ifndef PURRGO_SD_SPI_FAST_XFER
#define PURRGO_SD_SPI_FAST_XFER 1
#endif

/* Таймаут одного обмена (максимум 512 байт), мс. */
#define PURRGO_SD_SPI_TIMEOUT_MS 1000U

/* Период опроса вывода Detect. Изменение уровня принимается после двух
 * одинаковых отсчётов подряд (антидребезг). */
#ifndef PURRGO_SD_POLL_PERIOD_MS
#define PURRGO_SD_POLL_PERIOD_MS 250U
#endif

/* Период повторных попыток монтирования, если карта вставлена, но не
 * монтируется. Каждая неудачная попытка блокирует цикл. */
#ifndef PURRGO_SD_RETRY_PERIOD_MS
#define PURRGO_SD_RETRY_PERIOD_MS 10000U
#endif

/*
 * Питание карты.
 *
 * Аппаратный ключ питания (WeAct Mini Load Switch MT9700-N) управляется выводом CubeMX с
 * меткой SD_PWR (GPIO_Output; начальный уровень - тот, что выключает ключ): макрос SD_PWR_Pin в
 * main.h включает управление автоматически. Без метки (ключа в схеме нет) карта запитана
 * всегда, а состояние OFF означает только «карта не выбрана, SPI2 выключен»: пауз при
 * включении нет. Принудительно отключить: -DPURRGO_SD_POWER_SWITCH=0.
 */
#ifndef PURRGO_SD_POWER_SWITCH
#if defined(SD_PWR_Pin) && defined(SD_PWR_GPIO_Port)
#define PURRGO_SD_POWER_SWITCH 1
#else
#define PURRGO_SD_POWER_SWITCH 0
#endif
#endif

/* Уровень на SD_PWR, включающий ключ: 1 - High (EN активный высокий), 0 - Low. */
#ifndef PURRGO_SD_POWER_ACTIVE_HIGH
#define PURRGO_SD_POWER_ACTIVE_HIGH 1
#endif

/*
 * Пауза без питания перед повторным включением, мс. Карта должна разрядиться ниже 0,5 В
 * не менее чем на 1 мс (SD Physical Layer Spec, power cycle), иначе она остаётся в
 * прежнем состоянии, а не сбрасывается. Ключ без разряда выхода оставляет напряжение
 * на карте на десятки - сотни мс: подобрать по осциллографу (VCC карты после выключения).
 */
#ifndef PURRGO_SD_POWER_OFF_MIN_MS
#define PURRGO_SD_POWER_OFF_MIN_MS 250U
#endif

/* Пауза после включения ключа до первого такта: нарастание напряжения (<= 35 мс по
 * спецификации, у ключа с плавным пуском обычно единицы мс) и запуск карты. */
#ifndef PURRGO_SD_POWER_RAMP_MS
#define PURRGO_SD_POWER_RAMP_MS 10U
#endif

/* Выводы SPI2 (CubeMX: PB13 SCK, PB14 MISO, PB15 MOSI); меток в main.h у них нет. */
#define SD_SPI_PORT GPIOB
#define SD_SCK_PIN  GPIO_PIN_13
#define SD_MISO_PIN GPIO_PIN_14
#define SD_MOSI_PIN GPIO_PIN_15

static FATFS s_fs;
static sd_spi_t s_card;
static sd_power_t s_pwr;
static bool s_pwr_inited;
static volatile DSTATUS s_stat = STA_NOINIT;
static purrgo_sd_state_t s_state = PURRGO_SD_OFF;

/* Карта потеряна (извлечена или не отвечает). Сбрасывается при монтировании. */
static volatile bool s_card_lost;

/* Антидребезг Detect. */
static bool s_present_candidate;
static bool s_present_stable;
static bool s_present_prev;

static uint32_t s_last_poll_ms;
static bool s_retry_armed;
static uint32_t s_retry_at_ms;

/* Буфер из 0xFF для чтения (MOSI удерживается в 1) и приёмник лишних байт. */
static uint8_t s_tx_ff[SD_SPI_BLOCK_SIZE] = { [0 ... (SD_SPI_BLOCK_SIZE - 1U)] = 0xFFU };
static uint8_t s_rx_scratch[SD_SPI_BLOCK_SIZE];


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
    const GPIO_PinState level = HAL_GPIO_ReadPin(Detect_SD_GPIO_Port, Detect_SD_Pin);

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
        PURRGO_LOGE("SD: card lost (%s)\r\n", reason);
    }
}


/*
 * --------------------------------------------------------------------------
 * Привязка sd_spi к HAL
 * --------------------------------------------------------------------------
 */

static void spi_release(void)
{
    /* Выключает SPI2 (такты и выводы PB13..PB15 в analog кодом CubeMX). Повторный вызов
     * и вызов до первого Init безопасны: с выключенными тактами HAL к регистрам не лезет. */
    if (hspi2.State != HAL_SPI_STATE_RESET)
    {
        (void)HAL_SPI_DeInit(&hspi2);
    }
}

static void spi_set_prescaler(uint32_t prescaler)
{
    if (hspi2.State != HAL_SPI_STATE_RESET)
    {
        if (hspi2.Init.BaudRatePrescaler == prescaler)
        {
            return;
        }

        /* Выводы PB13..PB15 при DeInit/Init перенастраиваются кодом CubeMX;
         * SD_CS - отдельный GPIO и остаётся высоким. */
        spi_release();
    }

    hspi2.Init.BaudRatePrescaler = prescaler;
    if (HAL_SPI_Init(&hspi2) != HAL_OK)
    {
        purrgo_hw_fault(PURRGO_HWF_SD, "SPI2 reinit failed");
    }
}

static void port_cs(bool select, void *ctx)
{
    (void)ctx;
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, select ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/*
 * Обмен через HAL_SPI_TransmitReceive: при 8-битном кадре HAL гоняет данные
 * по одному байту (32-битный путь есть только для DataSize > 16 бит), это
 * ~70 тактов ядра на байт. Оставлен как эталон для сверки (hw_test) и как
 * запасной вариант (PURRGO_SD_SPI_FAST_XFER=0).
 */
static bool xfer_hal(const uint8_t *tx, uint8_t *rx, size_t len)
{
    while (len > 0U)
    {
        const uint16_t n = (len > SD_SPI_BLOCK_SIZE) ? (uint16_t)SD_SPI_BLOCK_SIZE : (uint16_t)len;
        const uint8_t *t = (tx != NULL) ? tx : s_tx_ff;
        uint8_t *r = (rx != NULL) ? rx : s_rx_scratch;

        if (HAL_SPI_TransmitReceive(&hspi2, (uint8_t *)(uintptr_t)t, r, n,
                                    PURRGO_SD_SPI_TIMEOUT_MS) != HAL_OK)
        {
            return false;
        }

        if (tx != NULL)
        {
            tx += n;
        }
        if (rx != NULL)
        {
            rx += n;
        }
        len -= n;
    }
    return true;
}

#if PURRGO_SD_SPI_FAST_XFER

/*
 * Быстрый обмен напрямую через регистры SPI2 (SPI v2 в STM32U5).
 *
 * Блок len >= 16 и кратный 4 идёт 32-битными обращениями к TXDR/RXDR
 * (порог FIFO 4 кадра: в слове 4 байта), остальное - побайтно. Одна
 * транзакция = одна установка TSIZE + SPE + CSTART; конец - флаг EOT.
 * В полёте держится не больше 12 байт (FIFO 16 байт у SPI2), поэтому
 * переполнения RX нет, а передатчик не простаивает: следующее слово
 * кладётся сразу, как только прочитано предыдущее.
 *
 * Прерывания и DMA не используются, HAL-состояние hspi2 не трогается
 * (остаётся READY), поэтому spi_set_prescaler() (DeInit/Init) работает как
 * раньше.
 */
static bool xfer_fast(const uint8_t *tx, uint8_t *rx, size_t len)
{
    SPI_TypeDef *const spi = hspi2.Instance;
    const bool wide = (len >= 16U) && ((len & 3U) == 0U);
    const size_t total = wide ? (len >> 2) : len;
    const size_t max_inflight = wide ? 3U : 8U;
    size_t sent = 0U;
    size_t got = 0U;
    bool ok = true;
    const uint32_t t0 = HAL_GetTick();

    /* Порог FIFO и размер транзакции меняются только при выключенном SPI. */
    MODIFY_REG(spi->CFG1, SPI_CFG1_FTHLV,
               wide ? SPI_FIFO_THRESHOLD_04DATA : SPI_FIFO_THRESHOLD_01DATA);
    MODIFY_REG(spi->CR2, SPI_CR2_TSIZE, (uint32_t)len);
    SET_BIT(spi->CR1, SPI_CR1_SPE);
    SET_BIT(spi->CR1, SPI_CR1_CSTART);

    while (got < total)
    {
        const uint32_t sr = spi->SR;
        bool progress = false;

        if ((sent < total) && ((sent - got) < max_inflight) && ((sr & SPI_SR_TXP) != 0U))
        {
            if (wide)
            {
                uint32_t w = 0xFFFFFFFFUL;

                if (tx != NULL)
                {
                    memcpy(&w, tx + (sent << 2), sizeof w);
                }
                *(__IO uint32_t *)&spi->TXDR = w;
            }
            else
            {
                *(__IO uint8_t *)&spi->TXDR = (tx != NULL) ? tx[sent] : 0xFFU;
            }
            sent++;
            progress = true;
        }

        if ((sr & SPI_SR_RXP) != 0U)
        {
            if (wide)
            {
                const uint32_t w = *(__IO uint32_t *)&spi->RXDR;

                if (rx != NULL)
                {
                    memcpy(rx + (got << 2), &w, sizeof w);
                }
            }
            else
            {
                const uint8_t b = *(__IO uint8_t *)&spi->RXDR;

                if (rx != NULL)
                {
                    rx[got] = b;
                }
            }
            got++;
            progress = true;
        }

        if (!progress && ((HAL_GetTick() - t0) >= PURRGO_SD_SPI_TIMEOUT_MS))
        {
            ok = false;
            break;
        }
    }

    if (ok)
    {
        while ((spi->SR & SPI_SR_EOT) == 0U)
        {
            if ((HAL_GetTick() - t0) >= PURRGO_SD_SPI_TIMEOUT_MS)
            {
                ok = false;
                break;
            }
        }
    }

    if ((spi->SR & (SPI_SR_OVR | SPI_SR_UDR)) != 0U)
    {
        ok = false;
    }

    spi->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC | SPI_IFCR_OVRC | SPI_IFCR_UDRC |
                SPI_IFCR_MODFC | SPI_IFCR_SUSPC;
    CLEAR_BIT(spi->CR1, SPI_CR1_SPE);
    return ok;
}

#endif /* PURRGO_SD_SPI_FAST_XFER */

static volatile bool s_force_hal_xfer;

void purrgo_sd_spi_force_hal_xfer(bool force)
{
    s_force_hal_xfer = force;
}

static bool port_xfer(const uint8_t *tx, uint8_t *rx, size_t len, void *ctx)
{
    (void)ctx;

#if PURRGO_SD_SPI_FAST_XFER
    if (!s_force_hal_xfer)
    {
        bool ok = true;

        while (ok && (len > 0U))
        {
            const size_t n = (len > SD_SPI_BLOCK_SIZE) ? (size_t)SD_SPI_BLOCK_SIZE : len;

            ok = xfer_fast(tx, rx, n);
            if (tx != NULL)
            {
                tx += n;
            }
            if (rx != NULL)
            {
                rx += n;
            }
            len -= n;
        }
        return ok;
    }
#endif

    return xfer_hal(tx, rx, len);
}

static uint32_t port_tick(void *ctx)
{
    (void)ctx;
    return HAL_GetTick();
}

static void port_fast(void *ctx)
{
    (void)ctx;
    spi_set_prescaler(PURRGO_SD_SPI_RUN_PRESCALER);
}

#if PURRGO_SD_SPI_HS
/*
 * Смена делителя без переинициализации HAL: SPE между обменами сброшен (xfer_fast),
 * hspi2.Init.BaudRatePrescaler остаётся рабочим, поэтому spi_set_prescaler() не страдает.
 */
static void port_hs(void *ctx, bool on)
{
    (void)ctx;
    MODIFY_REG(hspi2.Instance->CFG1, SPI_CFG1_MBR,
               on ? SPI_BAUDRATEPRESCALER_2 : PURRGO_SD_SPI_RUN_PRESCALER);
}
#endif

/*
 * --------------------------------------------------------------------------
 * Привязка sd_power к GPIO
 * --------------------------------------------------------------------------
 */

static void gpio_out(GPIO_TypeDef *port, uint32_t pin, GPIO_PinState level)
{
    GPIO_InitTypeDef gi = {0};

    /* Уровень - до включения выхода: без выброса на линии. */
    HAL_GPIO_WritePin(port, (uint16_t)pin, level);
    gi.Pin = pin;
    gi.Mode = GPIO_MODE_OUTPUT_PP;
    gi.Pull = GPIO_NOPULL;
    gi.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(port, &gi);
}

static void gpio_hiz(GPIO_TypeDef *port, uint32_t pin)
{
    GPIO_InitTypeDef gi = {0};

    gi.Pin = pin;
    gi.Mode = GPIO_MODE_ANALOG;
    gi.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(port, &gi);
}

#if PURRGO_SD_POWER_SWITCH && (PURRGO_SD_DETECT_MODE != PURRGO_SD_DETECT_NONE)
#define PURRGO_SD_DETECT_PARK 1
#else
#define PURRGO_SD_DETECT_PARK 0
#endif

#if PURRGO_SD_DETECT_PARK
/* Detect_SD - вход с подтяжкой к питанию МК (как в MX_GPIO_Init). */
static void gpio_detect_input(void)
{
    GPIO_InitTypeDef gi = {0};

    gi.Pin = Detect_SD_Pin;
    gi.Mode = GPIO_MODE_INPUT;
    gi.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(Detect_SD_GPIO_Port, &gi);
}
#endif

#if PURRGO_SD_POWER_SWITCH
static void pwr_rail(bool on, void *ctx)
{
    (void)ctx;
    const bool high = on ? (PURRGO_SD_POWER_ACTIVE_HIGH != 0) : (PURRGO_SD_POWER_ACTIVE_HIGH == 0);

    HAL_GPIO_WritePin(SD_PWR_GPIO_Port, SD_PWR_Pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
#endif

static void pwr_pins(sd_pins_t mode, void *ctx)
{
    (void)ctx;

    /* Выводы переходят из режима SPI2 в GPIO: сначала выключить сам SPI2. */
    spi_release();

    if (mode == SD_PINS_IDLE)
    {
        gpio_out(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
        gpio_out(SD_SPI_PORT, SD_MOSI_PIN, GPIO_PIN_SET);
        gpio_out(SD_SPI_PORT, SD_SCK_PIN, GPIO_PIN_RESET);
        gpio_hiz(SD_SPI_PORT, SD_MISO_PIN);
#if PURRGO_SD_DETECT_PARK
        gpio_detect_input();
#endif
        return;
    }

    gpio_out(SD_SPI_PORT, SD_SCK_PIN, GPIO_PIN_RESET);
    gpio_out(SD_SPI_PORT, SD_MOSI_PIN, GPIO_PIN_RESET);
    gpio_hiz(SD_SPI_PORT, SD_MISO_PIN);
#if PURRGO_SD_POWER_SWITCH
    /* Карта обесточена: CS в Hi-Z, высокий уровень с вывода МК запитал бы её через
     * подтяжку DAT3 на модуле. При включении подтяжка сама поднимает CS. */
    gpio_hiz(SD_CS_GPIO_Port, SD_CS_Pin);
#else
    gpio_out(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
#endif
#if PURRGO_SD_DETECT_PARK
    /* Внутренняя подтяжка PB10 к 3V3 через вывод CD модуля подпитывает VCC обесточенной
     * карты (на плате: ~1,9 В на VO ключа при вставленной карте). В OFF - Hi-Z; при
     * включении pwr_pins(SD_PINS_IDLE) возвращает вход с подтяжкой. */
    gpio_hiz(Detect_SD_GPIO_Port, Detect_SD_Pin);
#endif
}

static void pwr_delay(uint32_t ms, void *ctx)
{
    (void)ctx;
    HAL_Delay(ms);
}

static uint32_t pwr_tick(void *ctx)
{
    (void)ctx;
    return HAL_GetTick();
}

static const sd_power_port_t s_pwr_port = {
#if PURRGO_SD_POWER_SWITCH
    .rail = pwr_rail,
#else
    .rail = NULL,
#endif
    .pins = pwr_pins,
    .delay_ms = pwr_delay,
    .tick_ms = pwr_tick,
    .off_min_ms = PURRGO_SD_POWER_OFF_MIN_MS,
    .ramp_ms = PURRGO_SD_POWER_RAMP_MS,
    .ctx = NULL,
};

#if PURRGO_RENDER_PROF
static uint32_t port_cycles(void *ctx)
{
    (void)ctx;
    return DWT->CYCCNT;
}
#endif

static const sd_spi_port_t s_port = {
    .cs = port_cs,
    .xfer = port_xfer,
    .tick_ms = port_tick,
    .set_fast = port_fast,
    .ctx = NULL,
#if PURRGO_RENDER_PROF
    .cycles = port_cycles,
#endif
#if PURRGO_SD_SPI_HS
    .set_hs = port_hs,
#endif
};


/*
 * --------------------------------------------------------------------------
 * Диск FatFs (disk_*, вызываются из FatFs; диск один, pdrv = 0)
 * --------------------------------------------------------------------------
 */

/* Реакция на сбой обмена: извлечённая или зависшая карта объявляется потерянной. */
static DRESULT io_failed(sd_spi_res_t res)
{
    if (!card_inserted())
    {
        /* Карту вынули во время обмена: действие пользователя, не сбой железа. */
        PURRGO_LOGE("SD: I/O error %d\r\n", (int)res);
        card_lost("removed during transfer");
    }
    else
    {
        /* Карта на месте, а обмен не удался: сбой карты, контакта или шины SPI. */
        purrgo_hw_fault(PURRGO_HWF_SD, "I/O error %d", (int)res);
        if ((res == SD_SPI_ERR_TIMEOUT) || (res == SD_SPI_ERR_NOCARD))
        {
            card_lost("no response");
        }
    }
    return RES_ERROR;
}

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0U)
    {
        return (DSTATUS)STA_NOINIT;
    }
    return s_card_lost ? (DSTATUS)(STA_NOINIT) : s_stat;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0U)
    {
        return (DSTATUS)STA_NOINIT;
    }
    if (sd_power_state(&s_pwr) != SD_POWER_ON)
    {
        s_stat = (DSTATUS)STA_NOINIT;   /* питание карты снято: к ней не обращаемся */
        return s_stat;
    }
    if (!card_inserted())
    {
        s_stat = (DSTATUS)(STA_NOINIT | STA_NODISK);
        return s_stat;
    }

    /* Идентификация - на пониженной частоте. */
    spi_set_prescaler(PURRGO_SD_SPI_INIT_PRESCALER);

    const sd_spi_res_t res = sd_spi_init(&s_card, &s_port);

    if (res != SD_SPI_OK)
    {
        PURRGO_LOGE("SD: init FAILED, code %d\r\n", (int)res);
        spi_set_prescaler(PURRGO_SD_SPI_INIT_PRESCALER);
        s_stat = (DSTATUS)STA_NOINIT;
        return s_stat;
    }

    PURRGO_LOGI("SD: read data at %s\r\n", s_card.hs ? "40 MHz (High Speed)" : "20 MHz");
    s_stat = 0;
    return s_stat;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0U)
    {
        return RES_PARERR;
    }
    if ((buff == NULL) || (count == 0U))
    {
        return RES_PARERR;
    }
    if (s_card_lost || ((s_stat & STA_NOINIT) != 0))
    {
        return RES_NOTRDY;
    }

#if PURRGO_RENDER_PROF
    const sd_spi_stats_t st0 = s_card.stats;
#endif
    const sd_spi_res_t res = sd_spi_read(&s_card, buff, (uint32_t)sector, count);
#if PURRGO_RENDER_PROF
    {
        /* Разбор времени SD по стадиям попадает в PROF5 (render_prof.h). */
        const sd_spi_stats_t *st = &s_card.stats;

        purrgo_render_prof_count(PURRGO_PROF_CNT_SDX_PRE, st->pre - st0.pre);
        purrgo_render_prof_count(PURRGO_PROF_CNT_SDX_CMD, st->cmd - st0.cmd);
        purrgo_render_prof_count(PURRGO_PROF_CNT_SDX_TOK, st->tok - st0.tok);
        purrgo_render_prof_count(PURRGO_PROF_CNT_SDX_DATA, st->data - st0.data);
        purrgo_render_prof_count(PURRGO_PROF_CNT_SDX_STOP, st->stop - st0.stop);
        purrgo_render_prof_count(PURRGO_PROF_CNT_SDX_POST, st->post - st0.post);
        purrgo_render_prof_count(PURRGO_PROF_CNT_SDX_CALLS, st->calls - st0.calls);
        purrgo_render_prof_count(PURRGO_PROF_CNT_SDX_POLLS, st->polls - st0.polls);
        purrgo_render_prof_count(PURRGO_PROF_CNT_SDX_MULTI, st->multi - st0.multi);
    }
#endif

    return (res == SD_SPI_OK) ? RES_OK : io_failed(res);
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0U)
    {
        return RES_PARERR;
    }
    if ((buff == NULL) || (count == 0U))
    {
        return RES_PARERR;
    }
    if (s_card_lost || ((s_stat & STA_NOINIT) != 0))
    {
        return RES_NOTRDY;
    }

    const sd_spi_res_t res = sd_spi_write(&s_card, buff, (uint32_t)sector, count);

    return (res == SD_SPI_OK) ? RES_OK : io_failed(res);
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0U)
    {
        return RES_PARERR;
    }
    if (s_card_lost || ((s_stat & STA_NOINIT) != 0))
    {
        return RES_NOTRDY;
    }

    switch (cmd)
    {
    case CTRL_SYNC:
    {
        const sd_spi_res_t res = sd_spi_sync(&s_card);

        return (res == SD_SPI_OK) ? RES_OK : io_failed(res);
    }

    case GET_SECTOR_COUNT:
        if ((buff == NULL) || (s_card.sector_count == 0U))
        {
            return RES_ERROR;
        }
        *(DWORD *)buff = (DWORD)s_card.sector_count;
        return RES_OK;

    case GET_SECTOR_SIZE:
        if (buff == NULL)
        {
            return RES_PARERR;
        }
        *(WORD *)buff = (WORD)SD_SPI_BLOCK_SIZE;
        return RES_OK;

    case GET_BLOCK_SIZE:
        if (buff == NULL)
        {
            return RES_PARERR;
        }
        *(DWORD *)buff = 1U;      /* размер стираемого блока неизвестен */
        return RES_OK;

    default:
        return RES_PARERR;
    }
}


/*
 * --------------------------------------------------------------------------
 * Монтирование
 * --------------------------------------------------------------------------
 */

static void log_card_info(void)
{
    const uint32_t capacity_mb = s_card.sector_count >> 11;   /* 2048 секторов = 1 МБ */

    PURRGO_LOG("SD: %s, %lu MB, SPI2 prescaler 0x%08lX\r\n",
               s_card.block_addressing ? "SDHC/SDXC" : "SDSC",
               (unsigned long)capacity_mb,
               (unsigned long)hspi2.Init.BaudRatePrescaler);
}

static void make_dir(const char *path)
{
    const FRESULT res = f_mkdir(path);

    if ((res != FR_OK) && (res != FR_EXIST))
    {
        PURRGO_LOGE("SD: mkdir %s error: %d\r\n", path, (int)res);
    }
}

static void unmount(void)
{
    (void)f_mount(NULL, "0:", 0);

    sd_spi_deinit(&s_card);
    s_stat = (DSTATUS)STA_NOINIT;
    s_state = PURRGO_SD_NO_CARD;

    /* Следующая инициализация снова начнётся с низкой частоты. */
    spi_set_prescaler(PURRGO_SD_SPI_INIT_PRESCALER);
}

static bool mount_card(void)
{
    /*
     * Напряжение ниже порога безопасной записи: карту не трогаем (запуск от
     * севших батарей, возврат после просадки до восстановления питания).
     * Вызывающий код повторит попытку через PURRGO_SD_RETRY_PERIOD_MS.
     */
    if (purrgo_platform_supply_low())
    {
        PURRGO_LOGW("SD: mount postponed, VDD low\r\n");
        s_state = PURRGO_SD_MOUNT_ERROR;
        return false;
    }

    s_card_lost = false;
    s_stat = (DSTATUS)STA_NOINIT;

    /* opt = 1: смонтировать немедленно (инициализация диска + чтение FAT). */
    const FRESULT res = f_mount(&s_fs, "0:", 1);

    if (res != FR_OK)
    {
        purrgo_hw_fault(PURRGO_HWF_SD, "mount error %d (FatFs FRESULT)", (int)res);
        unmount();
        s_state = PURRGO_SD_MOUNT_ERROR;
        return false;
    }

    log_card_info();

    /* Базовая структура каталогов. */
    make_dir("0:/PURRGO");
    make_dir("0:/PURRGO/MAPS");
    make_dir("0:/PURRGO/TRACKS");
    make_dir("0:/PURRGO/BOOKS");
    make_dir("0:/PURRGO/WAYPTS");
    make_dir("0:/PURRGO/ROUTES");
    make_dir("0:/PURRGO/LOGS");
    s_state = PURRGO_SD_READY;
    purrgo_hw_fault_clear(PURRGO_HWF_SD);   /* карта снова работает: следующий сбой прозвучит заново */
    PURRGO_LOG("SD: mount OK, tree verified\r\n");
    return true;
}


/*
 * --------------------------------------------------------------------------
 * Публичный API (см. sd_card.h)
 * --------------------------------------------------------------------------
 */

/* Перевести питание карты в известное состояние OFF (один раз за запуск). */
static void power_ensure_init(void)
{
    if (!s_pwr_inited)
    {
        /* Состояние ключа после сброса МК неизвестно: карту обесточиваем явно. */
        (void)sd_power_init(&s_pwr, &s_pwr_port);
        s_pwr_inited = true;
        s_state = PURRGO_SD_OFF;
    }
}

void purrgo_sd_park(void)
{
    power_ensure_init();
}

/* Подать питание (если снято), определить наличие карты и смонтировать ФС. */
static bool power_up_and_mount(void)
{
    const uint32_t t0 = HAL_GetTick();

    power_ensure_init();

    if (!sd_power_up(&s_pwr))
    {
        purrgo_hw_fault(PURRGO_HWF_SD, "card power-up failed");
        s_state = PURRGO_SD_MOUNT_ERROR;
        return false;
    }

#if PURRGO_SD_DETECT_MODE != PURRGO_SD_DETECT_NONE
    PURRGO_LOG("SD: Detect_SD level %d\r\n",
               (int)HAL_GPIO_ReadPin(Detect_SD_GPIO_Port, Detect_SD_Pin));
#endif

    s_present_stable = card_inserted();
    s_present_candidate = s_present_stable;
    s_present_prev = s_present_stable;
    s_retry_armed = false;

    if (!s_present_stable)
    {
        s_state = PURRGO_SD_NO_CARD;
        PURRGO_LOGW("SD: no card (Detect_SD)\r\n");
        return false;
    }

    const bool ok = mount_card();

    if (!ok)
    {
        s_retry_armed = true;
        s_retry_at_ms = HAL_GetTick() + PURRGO_SD_RETRY_PERIOD_MS;
    }
    else
    {
        PURRGO_LOG("SD: power-up to ready %lu ms\r\n", (unsigned long)(HAL_GetTick() - t0));
    }

    return ok;
}

bool purrgo_sd_init(void)
{
    return power_up_and_mount();
}

bool purrgo_sd_power_up(void)
{
    if ((s_state == PURRGO_SD_READY) && !s_card_lost)
    {
        return true;
    }
    return power_up_and_mount();
}

void purrgo_sd_power_down(void)
{
    power_ensure_init();

    if (sd_power_state(&s_pwr) == SD_POWER_OFF)
    {
        return;
    }

    /*
     * Все файлы закрывает вызывающий код (FatFs при f_mount(NULL) их не сбрасывает).
     * Здесь - дождаться конца внутренней записи карты: отключение питания посреди
     * записи блока портит его.
     */
    if ((s_state == PURRGO_SD_READY) && !s_card_lost)
    {
        (void)disk_ioctl(0, CTRL_SYNC, NULL);
    }

    unmount();
    spi_release();
    sd_power_down(&s_pwr);

    s_state = PURRGO_SD_OFF;
    s_retry_armed = false;
    s_card_lost = false;
    PURRGO_LOG("SD: power off%s\r\n", sd_power_has_switch(&s_pwr) ? "" : " (no switch: card deselected)");
}

bool purrgo_sd_is_powered(void)
{
    return (sd_power_state(&s_pwr) == SD_POWER_ON);
}

void purrgo_sd_standby_hold(void)
{
#if PURRGO_SD_POWER_SWITCH
    uint32_t port;

    if (SD_PWR_GPIO_Port == GPIOA)      { port = PWR_GPIO_A; }
    else if (SD_PWR_GPIO_Port == GPIOB) { port = PWR_GPIO_B; }
    else if (SD_PWR_GPIO_Port == GPIOC) { port = PWR_GPIO_C; }
    else { return; }   /* других портов на UFQFPN48 с выведенным SD_PWR нет */

    /*
     * В Standby выводы МК без PWR-подтяжки уходят в Hi-Z: состояние EN ключа определяет
     * подтяжка на модуле (какая - проверить на железе). PWR удерживает вывод в уровне
     * «выключено», как для GNSS_EN. Вызывать до HAL_PWREx_EnablePullUpPullDownConfig().
     */
#if PURRGO_SD_POWER_ACTIVE_HIGH
    (void)HAL_PWREx_EnableGPIOPullDown(port, SD_PWR_Pin);
#else
    (void)HAL_PWREx_EnableGPIOPullUp(port, SD_PWR_Pin);
#endif
#endif
}

void purrgo_sd_poll(uint32_t now_ms)
{
    if ((uint32_t)(now_ms - s_last_poll_ms) < PURRGO_SD_POLL_PERIOD_MS)
    {
        return;
    }
    s_last_poll_ms = now_ms;

    /* Питание карты снято (purrgo_sd_power_down()): вставка и монтирование не отслеживаются. */
    if (sd_power_state(&s_pwr) == SD_POWER_OFF)
    {
        return;
    }

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
            PURRGO_LOGW("SD: card removed\r\n");
            purrgo_hw_fault_clear(PURRGO_HWF_SD);   /* вынули карту: эпизод сбоя закончен */
            unmount();
        }
        else if (s_card_lost)
        {
            purrgo_hw_fault(PURRGO_HWF_SD, "I/O failure, remounting later");
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
            purrgo_hw_fault(PURRGO_HWF_SD, "I/O failure, remounting later");
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
