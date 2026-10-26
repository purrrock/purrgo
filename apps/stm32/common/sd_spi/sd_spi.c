/*
 * microSD в SPI-режиме. См. sd_spi.h.
 */

#include "sd_spi.h"

#include <string.h>

/* Команды SPI-режима (индекс; бит 0x80 - команда прикладная, ACMD). */
#define CMD0   0U           /* GO_IDLE_STATE */
#define CMD8   8U           /* SEND_IF_COND */
#define CMD9   9U           /* SEND_CSD */
#define CMD12  12U          /* STOP_TRANSMISSION */
#define CMD16  16U          /* SET_BLOCKLEN */
#define CMD17  17U          /* READ_SINGLE_BLOCK */
#define CMD18  18U          /* READ_MULTIPLE_BLOCK */
#define ACMD23 (0x80U | 23U) /* SET_WR_BLK_ERASE_COUNT */
#define CMD24  24U          /* WRITE_BLOCK */
#define CMD25  25U          /* WRITE_MULTIPLE_BLOCK */
#define ACMD41 (0x80U | 41U) /* SD_SEND_OP_COND */
#define CMD55  55U          /* APP_CMD */
#define CMD58  58U          /* READ_OCR */

#define R1_IDLE 0x01U

#define TOKEN_START_SINGLE 0xFEU   /* начало блока: чтение, CMD17/CMD24/CMD18 */
#define TOKEN_START_MULTI  0xFCU   /* начало блока CMD25 */
#define TOKEN_STOP_MULTI   0xFDU   /* конец передачи CMD25 */

#define INIT_TIMEOUT_MS      1000U /* ACMD41 до выхода из idle */
#define READY_TIMEOUT_MS      500U /* ожидание 0xFF (карта не занята) */
#define READ_TOKEN_TIMEOUT_MS 200U /* ожидание токена данных при чтении */
#define WRITE_BUSY_TIMEOUT_MS 500U /* окончание программирования блока */

/* ------------------------------------------------------------------------ */
/* Низкоуровневый обмен                                                      */
/* ------------------------------------------------------------------------ */

static bool spi_rx(sd_spi_t *sd, uint8_t *dst, size_t len)
{
    return sd->port.xfer(NULL, dst, len, sd->port.ctx);
}

static bool spi_tx(sd_spi_t *sd, const uint8_t *src, size_t len)
{
    return sd->port.xfer(src, NULL, len, sd->port.ctx);
}

static uint8_t spi_rx_byte(sd_spi_t *sd)
{
    uint8_t b = 0xFFU;

    if (!spi_rx(sd, &b, 1U))
    {
        return 0xFFU;
    }
    return b;
}

static bool spi_tx_byte(sd_spi_t *sd, uint8_t b)
{
    return spi_tx(sd, &b, 1U);
}

static uint32_t now_ms(sd_spi_t *sd)
{
    return sd->port.tick_ms(sd->port.ctx);
}

static bool elapsed(sd_spi_t *sd, uint32_t start, uint32_t timeout_ms)
{
    return (uint32_t)(now_ms(sd) - start) >= timeout_ms;
}

static void select_card(sd_spi_t *sd)
{
    sd->port.cs(true, sd->port.ctx);
}

/* Отпустить CS и дать карте 8 тактов для освобождения линии DO. */
static void deselect_card(sd_spi_t *sd)
{
    sd->port.cs(false, sd->port.ctx);
    (void)spi_rx_byte(sd);
}

/* Ждать 0xFF (карта не занята). */
static bool wait_ready(sd_spi_t *sd, uint32_t timeout_ms)
{
    const uint32_t start = now_ms(sd);

    do
    {
        if (spi_rx_byte(sd) == 0xFFU)
        {
            return true;
        }
    } while (!elapsed(sd, start, timeout_ms));

    return false;
}

/*
 * Послать команду, вернуть R1 (0xFF - ответа нет). CS остаётся низким:
 * дальше идут данные или ответ R3/R7. Вызывающий отпускает CS сам.
 */
static uint8_t send_cmd(sd_spi_t *sd, uint8_t cmd, uint32_t arg)
{
    uint8_t r1;

    if ((cmd & 0x80U) != 0U)
    {
        cmd &= 0x7FU;
        r1 = send_cmd(sd, CMD55, 0U);
        if (r1 > R1_IDLE)
        {
            return r1;
        }
    }

    /* Каждая команда, кроме CMD12, начинается с выбора карты и ожидания. */
    if (cmd != CMD12)
    {
        deselect_card(sd);
        select_card(sd);
        if (!wait_ready(sd, READY_TIMEOUT_MS))
        {
            return 0xFFU;
        }
    }

    uint8_t frame[6];

    frame[0] = (uint8_t)(0x40U | cmd);
    frame[1] = (uint8_t)(arg >> 24);
    frame[2] = (uint8_t)(arg >> 16);
    frame[3] = (uint8_t)(arg >> 8);
    frame[4] = (uint8_t)arg;
    /* CRC проверяется картой только для CMD0 и CMD8. */
    frame[5] = (cmd == CMD0) ? 0x95U : ((cmd == CMD8) ? 0x87U : 0x01U);

    if (!spi_tx(sd, frame, sizeof frame))
    {
        return 0xFFU;
    }

    /* После CMD12 первый принятый байт - лишний (stuff byte). */
    if (cmd == CMD12)
    {
        (void)spi_rx_byte(sd);
    }

    /* Ответ приходит в течение 8 байт (Ncr). */
    uint8_t n = 10U;

    do
    {
        r1 = spi_rx_byte(sd);
        n--;
    } while (((r1 & 0x80U) != 0U) && (n != 0U));

    return r1;
}

/* ------------------------------------------------------------------------ */
/* Блоки данных                                                              */
/* ------------------------------------------------------------------------ */

static sd_spi_res_t rx_data_block(sd_spi_t *sd, uint8_t *buf, size_t len)
{
    const uint32_t start = now_ms(sd);
    uint8_t token;

    do
    {
        token = spi_rx_byte(sd);
    } while ((token == 0xFFU) && !elapsed(sd, start, READ_TOKEN_TIMEOUT_MS));

    if (token == 0xFFU)
    {
        return SD_SPI_ERR_TIMEOUT;
    }
    if (token != TOKEN_START_SINGLE)
    {
        return SD_SPI_ERR_IO;
    }

    if (!spi_rx(sd, buf, len))
    {
        return SD_SPI_ERR_IO;
    }

    /* Два байта CRC отбрасываются. */
    (void)spi_rx_byte(sd);
    (void)spi_rx_byte(sd);
    return SD_SPI_OK;
}

static sd_spi_res_t tx_data_block(sd_spi_t *sd, const uint8_t *buf, uint8_t token)
{
    if (!wait_ready(sd, READY_TIMEOUT_MS))
    {
        return SD_SPI_ERR_TIMEOUT;
    }

    if (!spi_tx_byte(sd, token))
    {
        return SD_SPI_ERR_IO;
    }

    if (token == TOKEN_STOP_MULTI)
    {
        return SD_SPI_OK;
    }

    if (!spi_tx(sd, buf, SD_SPI_BLOCK_SIZE))
    {
        return SD_SPI_ERR_IO;
    }

    /* CRC (игнорируется картой) и токен ответа: xxx00101 - принято. */
    (void)spi_tx_byte(sd, 0xFFU);
    (void)spi_tx_byte(sd, 0xFFU);

    const uint8_t resp = spi_rx_byte(sd);

    if ((resp & 0x1FU) != 0x05U)
    {
        return SD_SPI_ERR_IO;
    }
    return SD_SPI_OK;
}

static uint32_t card_addr(const sd_spi_t *sd, uint32_t sector)
{
    return sd->block_addressing ? sector : (sector * SD_SPI_BLOCK_SIZE);
}

/* ------------------------------------------------------------------------ */
/* Объём карты (CSD)                                                         */
/* ------------------------------------------------------------------------ */

static uint32_t read_sector_count(sd_spi_t *sd)
{
    uint8_t csd[16];
    uint32_t sectors = 0U;

    if ((send_cmd(sd, CMD9, 0U) == 0U) &&
        (rx_data_block(sd, csd, sizeof csd) == SD_SPI_OK))
    {
        if ((csd[0] >> 6) == 1U)
        {
            /* CSD v2.0 (SDHC/SDXC): (C_SIZE + 1) * 512 КБ. */
            const uint32_t c_size = ((uint32_t)(csd[7] & 0x3FU) << 16) |
                                    ((uint32_t)csd[8] << 8) |
                                    (uint32_t)csd[9];
            sectors = (c_size + 1U) << 10;
        }
        else
        {
            /* CSD v1.0 (SDSC). */
            const uint32_t n = (uint32_t)(csd[5] & 15U) +
                               (uint32_t)((csd[10] & 0x80U) >> 7) +
                               (uint32_t)((csd[9] & 3U) << 1) + 2U;
            const uint32_t c_size = (uint32_t)(csd[8] >> 6) +
                                    ((uint32_t)csd[7] << 2) +
                                    ((uint32_t)(csd[6] & 3U) << 10) + 1U;
            sectors = c_size << (n - 9U);
        }
    }

    deselect_card(sd);
    return sectors;
}

/* ------------------------------------------------------------------------ */
/* Публичный API                                                             */
/* ------------------------------------------------------------------------ */

sd_spi_res_t sd_spi_init(sd_spi_t *sd, const sd_spi_port_t *port)
{
    if ((sd == NULL) || (port == NULL) || (port->cs == NULL) ||
        (port->xfer == NULL) || (port->tick_ms == NULL))
    {
        return SD_SPI_ERR_PARAM;
    }

    memset(sd, 0, sizeof *sd);
    sd->port = *port;

    /* Не менее 74 тактов при высоком CS: карта переходит в SPI-режим. */
    sd->port.cs(false, sd->port.ctx);
    for (unsigned i = 0U; i < 10U; i++)
    {
        (void)spi_tx_byte(sd, 0xFFU);
    }

    if (send_cmd(sd, CMD0, 0U) != R1_IDLE)
    {
        deselect_card(sd);
        return SD_SPI_ERR_NOCARD;
    }

    bool block = false;
    uint8_t r1 = send_cmd(sd, CMD8, 0x1AAU);

    if (r1 == R1_IDLE)
    {
        /* SD v2: R7 = 4 байта, ждём эхо 0x1AA (питание 2.7-3.6 В). */
        uint8_t r7[4];

        (void)spi_rx(sd, r7, sizeof r7);
        if ((r7[2] != 0x01U) || (r7[3] != 0xAAU))
        {
            deselect_card(sd);
            return SD_SPI_ERR_UNSUPPORTED;
        }

        const uint32_t start = now_ms(sd);

        while (send_cmd(sd, ACMD41, 1UL << 30) != 0U)
        {
            if (elapsed(sd, start, INIT_TIMEOUT_MS))
            {
                deselect_card(sd);
                return SD_SPI_ERR_TIMEOUT;
            }
        }

        if (send_cmd(sd, CMD58, 0U) != 0U)
        {
            deselect_card(sd);
            return SD_SPI_ERR_IO;
        }

        uint8_t ocr[4];

        (void)spi_rx(sd, ocr, sizeof ocr);
        block = ((ocr[0] & 0x40U) != 0U);     /* бит CCS */
    }
    else
    {
        if (r1 == 0xFFU)
        {
            deselect_card(sd);
            return SD_SPI_ERR_NOCARD;
        }

        /* SD v1 (SDSC) либо MMC: ACMD41 принимает только SD. */
        if (send_cmd(sd, ACMD41, 0U) > R1_IDLE)
        {
            deselect_card(sd);
            return SD_SPI_ERR_UNSUPPORTED;
        }

        const uint32_t start = now_ms(sd);

        while (send_cmd(sd, ACMD41, 0U) != 0U)
        {
            if (elapsed(sd, start, INIT_TIMEOUT_MS))
            {
                deselect_card(sd);
                return SD_SPI_ERR_TIMEOUT;
            }
        }

        if (send_cmd(sd, CMD16, SD_SPI_BLOCK_SIZE) != 0U)
        {
            deselect_card(sd);
            return SD_SPI_ERR_IO;
        }
    }

    deselect_card(sd);

    sd->block_addressing = block;
    sd->initialized = true;

    if (sd->port.set_fast != NULL)
    {
        sd->port.set_fast(sd->port.ctx);
    }

    sd->sector_count = read_sector_count(sd);
    return SD_SPI_OK;
}

sd_spi_res_t sd_spi_read(sd_spi_t *sd, uint8_t *buf, uint32_t sector, uint32_t count)
{
    if ((sd == NULL) || (buf == NULL) || (count == 0U))
    {
        return SD_SPI_ERR_PARAM;
    }
    if (!sd->initialized)
    {
        return SD_SPI_ERR_NOINIT;
    }

    sd_spi_res_t res = SD_SPI_OK;
    const uint8_t cmd = (count == 1U) ? CMD17 : CMD18;

    if (send_cmd(sd, cmd, card_addr(sd, sector)) != 0U)
    {
        deselect_card(sd);
        return SD_SPI_ERR_IO;
    }

    for (uint32_t i = 0U; i < count; i++)
    {
        res = rx_data_block(sd, buf + ((size_t)i * SD_SPI_BLOCK_SIZE), SD_SPI_BLOCK_SIZE);
        if (res != SD_SPI_OK)
        {
            break;
        }
    }

    if (count > 1U)
    {
        (void)send_cmd(sd, CMD12, 0U);
    }

    deselect_card(sd);
    return res;
}

sd_spi_res_t sd_spi_write(sd_spi_t *sd, const uint8_t *buf, uint32_t sector, uint32_t count)
{
    if ((sd == NULL) || (buf == NULL) || (count == 0U))
    {
        return SD_SPI_ERR_PARAM;
    }
    if (!sd->initialized)
    {
        return SD_SPI_ERR_NOINIT;
    }

    sd_spi_res_t res = SD_SPI_OK;

    if (count == 1U)
    {
        if (send_cmd(sd, CMD24, card_addr(sd, sector)) != 0U)
        {
            deselect_card(sd);
            return SD_SPI_ERR_IO;
        }
        res = tx_data_block(sd, buf, TOKEN_START_SINGLE);
    }
    else
    {
        /* Предварительное стирание ускоряет запись; ошибка не критична. */
        (void)send_cmd(sd, ACMD23, count);

        if (send_cmd(sd, CMD25, card_addr(sd, sector)) != 0U)
        {
            deselect_card(sd);
            return SD_SPI_ERR_IO;
        }

        for (uint32_t i = 0U; i < count; i++)
        {
            res = tx_data_block(sd, buf + ((size_t)i * SD_SPI_BLOCK_SIZE), TOKEN_START_MULTI);
            if (res != SD_SPI_OK)
            {
                break;
            }
        }

        if (res == SD_SPI_OK)
        {
            res = tx_data_block(sd, NULL, TOKEN_STOP_MULTI);
        }
    }

    /* Блок принят картой, но программируется ещё: ждём конца записи. */
    if ((res == SD_SPI_OK) && !wait_ready(sd, WRITE_BUSY_TIMEOUT_MS))
    {
        res = SD_SPI_ERR_TIMEOUT;
    }

    deselect_card(sd);
    return res;
}

sd_spi_res_t sd_spi_sync(sd_spi_t *sd)
{
    if (sd == NULL)
    {
        return SD_SPI_ERR_PARAM;
    }
    if (!sd->initialized)
    {
        return SD_SPI_ERR_NOINIT;
    }

    select_card(sd);
    const bool ready = wait_ready(sd, WRITE_BUSY_TIMEOUT_MS);

    deselect_card(sd);
    return ready ? SD_SPI_OK : SD_SPI_ERR_TIMEOUT;
}

void sd_spi_deinit(sd_spi_t *sd)
{
    if (sd == NULL)
    {
        return;
    }

    if (sd->port.cs != NULL)
    {
        sd->port.cs(false, sd->port.ctx);
    }
    sd->initialized = false;
}
