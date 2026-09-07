#define TRUE  1
#define FALSE 0
#define bool BYTE

#include "stm32f4xx_hal.h"

#include "diskio.h"
#include "fatfs_sd.h"
#include "purrgo_logger.h"

static volatile DSTATUS Stat = STA_NOINIT;  /* Disk Status */
static uint8_t CardType;                    /* Type 0:MMC, 1:SDC, 2:Block addressing */
static uint8_t PowerFlag = 0;               /* Power flag */


/***************************************
 * Diagnostic helpers
 **************************************/

/*
 * Convert DSTATUS flags to a human-readable UART message.
 *
 * This function is used only for diagnostics.
 * It does not change the driver state.
 */
static void SD_LogStatus(const char *prefix, DSTATUS status)
{
    purrgo_logger_write(
        "%s 0x%02X ["
        "%s%s%s%s]\r\n",
        prefix,
        status,
        (status & STA_NOINIT) ? " NOINIT" : "",
        (status & STA_NODISK) ? " NODISK" : "",
        (status & STA_PROTECT) ? " PROTECT" : "",
        (status == 0) ? " READY" : ""
    );
}


/*
 * Convert DRESULT to a human-readable UART message.
 *
 * This function is used only for diagnostics.
 */
static void SD_LogResult(const char *operation, DRESULT result)
{
    const char *name;

    switch (result)
    {
    case RES_OK:
        name = "RES_OK";
        break;

    case RES_ERROR:
        name = "RES_ERROR";
        break;

    case RES_WRPRT:
        name = "RES_WRPRT";
        break;

    case RES_NOTRDY:
        name = "RES_NOTRDY";
        break;

    case RES_PARERR:
        name = "RES_PARERR";
        break;

    default:
        name = "UNKNOWN";
        break;
    }

    purrgo_logger_write(
        "SD: %s -> %s (%d)\r\n",
        operation,
        name,
        result
    );
}


/*
 * Convert FatFs disk status to a readable message.
 *
 * Used only if the caller wants to inspect Stat.
 */
static void SD_LogCardType(uint8_t type)
{
    purrgo_logger_write("SD: CardType = 0x%02X", type);

    if (type == 0)
    {
        purrgo_logger_write(" UNKNOWN");
    }
    else
    {
        if (type & CT_MMC)
        {
            purrgo_logger_write(" CT_MMC");
        }

        if (type & CT_SD1)
        {
            purrgo_logger_write(" CT_SD1");
        }

        if (type & CT_SD2)
        {
            purrgo_logger_write(" CT_SD2");
        }

        if (type & CT_SDC)
        {
            purrgo_logger_write(" CT_SDC");
        }

        if (type & CT_BLOCK)
        {
            purrgo_logger_write(" CT_BLOCK");
        }
    }

    purrgo_logger_write("\r\n");
}


/***************************************
 * SPI functions
 **************************************/

/* slave select */
static void SELECT(void)
{
    purrgo_logger_write("SD: CS -> LOW\r\n");

    HAL_GPIO_WritePin(
        SD_CS_PORT,
        SD_CS_PIN,
        GPIO_PIN_RESET
    );

    HAL_Delay(1);
}


/* slave deselect */
static void DESELECT(void)
{
    HAL_GPIO_WritePin(
        SD_CS_PORT,
        SD_CS_PIN,
        GPIO_PIN_SET
    );

    purrgo_logger_write("SD: CS -> HIGH\r\n");

    HAL_Delay(1);
}


/*
 * SPI transmit a byte.
 *
 * The function keeps the original driver behaviour.
 * Diagnostic output is intentionally not generated for
 * every transmitted byte.
 */
static void SPI_TxByte(uint8_t data)
{
    uint32_t tickstart = HAL_GetTick();

    while (!__HAL_SPI_GET_FLAG(HSPI_SDCARD, SPI_FLAG_TXE))
    {
        if ((HAL_GetTick() - tickstart) >= SPI_TIMEOUT)
        {
            purrgo_logger_write(
                "SD: SPI TXE timeout, TX byte = 0x%02X\r\n",
                data
            );

            break;
        }
    }

    if (HAL_SPI_Transmit(
            HSPI_SDCARD,
            &data,
            1,
            SPI_TIMEOUT) != HAL_OK)
    {
        purrgo_logger_write(
            "SD: HAL_SPI_Transmit ERROR, byte = 0x%02X\r\n",
            data
        );
    }
}


/*
 * SPI transmit buffer.
 */
static void SPI_TxBuffer(uint8_t *buffer, uint16_t len)
{
    uint32_t tickstart = HAL_GetTick();

    while (!__HAL_SPI_GET_FLAG(HSPI_SDCARD, SPI_FLAG_TXE))
    {
        if ((HAL_GetTick() - tickstart) >= SPI_TIMEOUT)
        {
            purrgo_logger_write(
                "SD: SPI TXE timeout before buffer TX, len=%u\r\n",
                len
            );

            break;
        }
    }

    if (HAL_SPI_Transmit(
            HSPI_SDCARD,
            buffer,
            len,
            SPI_TIMEOUT) != HAL_OK)
    {
        purrgo_logger_write(
            "SD: HAL_SPI_Transmit buffer ERROR, len=%u\r\n",
            len
        );
    }
}


/*
 * SPI receive a byte.
 *
 * 0xFF is transmitted to generate the SPI clock required
 * to receive one byte from the SD card.
 */
static uint8_t SPI_RxByte(void)
{
    uint8_t dummy;
    uint8_t data;

    dummy = 0xFF;
    data = 0xFF;

    uint32_t tickstart = HAL_GetTick();

    while (!__HAL_SPI_GET_FLAG(HSPI_SDCARD, SPI_FLAG_TXE))
    {
        if ((HAL_GetTick() - tickstart) >= SPI_TIMEOUT)
        {
            purrgo_logger_write(
                "SD: SPI TXE timeout during RX\r\n"
            );

            break;
        }
    }

    if (HAL_SPI_TransmitReceive(
            HSPI_SDCARD,
            &dummy,
            &data,
            1,
            SPI_TIMEOUT) != HAL_OK)
    {
        purrgo_logger_write(
            "SD: HAL_SPI_TransmitReceive ERROR\r\n"
        );
    }

    return data;
}


/* SPI receive a byte via pointer */
static void SPI_RxBytePtr(uint8_t *buff)
{
    *buff = SPI_RxByte();
}


/***************************************
 * SD functions
 **************************************/

/*
 * Wait until the SD card releases the SPI data line.
 *
 * In SPI mode the card indicates that it is ready by
 * returning 0xFF.
 *
 * Timeout: 500 ms.
 */
static uint8_t SD_ReadyWait(void)
{
    uint8_t res;
    uint32_t tickstart = HAL_GetTick();

    do
    {
        res = SPI_RxByte();

    } while ((res != 0xFF) &&
             ((HAL_GetTick() - tickstart) < 500));

    if (res != 0xFF)
    {
        purrgo_logger_write(
            "SD: READY WAIT TIMEOUT, last=0x%02X, elapsed=%lu ms\r\n",
            res,
            (unsigned long)(HAL_GetTick() - tickstart)
        );
    }

    return res;
}


/*
 * Power-on / SPI-mode startup sequence.
 *
 * This does not physically switch SD-card power.
 * It sends the required initial clock cycles and CMD0.
 */
static void SD_PowerOn(void)
{
    uint8_t args[6];
    uint8_t response;
    uint32_t cnt = 0x1FFF;

    purrgo_logger_write(
        "\r\nSD: ===== SD_PowerOn START =====\r\n"
    );

    /*
     * Keep CS inactive and provide at least 74 clock cycles.
     *
     * 10 bytes * 8 clocks = 80 clocks.
     */
    DESELECT();

    purrgo_logger_write(
        "SD: Sending 80 initial SPI clocks (0xFF x 10)\r\n"
    );

    for (int i = 0; i < 10; i++)
    {
        SPI_TxByte(0xFF);
    }

    /*
     * Select the card.
     */
    SELECT();

    /*
     * CMD0 = GO_IDLE_STATE.
     *
     * Argument = 0.
     * CRC = 0x95 is required while CRC checking is enabled
     * during the initial SPI-mode command sequence.
     */
    args[0] = CMD0;
    args[1] = 0;
    args[2] = 0;
    args[3] = 0;
    args[4] = 0;
    args[5] = 0x95;

    purrgo_logger_write(
        "SD: CMD0 -> 40 00 00 00 00 95\r\n"
    );

    SPI_TxBuffer(args, sizeof(args));

    /*
     * Wait for the idle-state response 0x01.
     */
    response = 0xFF;

    while (cnt)
    {
        response = SPI_RxByte();

        if (response == 0x01)
        {
            break;
        }

        cnt--;
    }

    purrgo_logger_write(
        "SD: CMD0 response = 0x%02X, remaining=%lu\r\n",
        response,
        (unsigned long)cnt
    );

    if (response == 0x01)
    {
        purrgo_logger_write(
            "SD: CMD0 SUCCESS - card entered IDLE state\r\n"
        );
    }
    else
    {
        purrgo_logger_write(
            "SD: CMD0 FAILED - card did not return 0x01\r\n"
        );
    }

    DESELECT();

    /*
     * Extra clocks after CMD0.
     */
    SPI_TxByte(0xFF);

    PowerFlag = 1;

    purrgo_logger_write(
        "SD: PowerFlag = %u\r\n",
        PowerFlag
    );

    purrgo_logger_write(
        "SD: ===== SD_PowerOn END =====\r\n"
    );
}


/* power off */
static void SD_PowerOff(void)
{
    PowerFlag = 0;

    purrgo_logger_write(
        "SD: PowerFlag = 0\r\n"
    );
}


/* check power flag */
static uint8_t SD_CheckPower(void)
{
    return PowerFlag;
}


/*
 * Receive a data block.
 *
 * The SD card first sends a data token 0xFE.
 */
static bool SD_RxDataBlock(BYTE *buff, UINT len)
{
    uint8_t token;
    uint32_t tickstart = HAL_GetTick();

    do
    {
        token = SPI_RxByte();

    } while ((token == 0xFF) &&
             ((HAL_GetTick() - tickstart) < 200));

    if (token != 0xFE)
    {
        purrgo_logger_write(
            "SD: RX DATA BLOCK FAILED, token=0x%02X, elapsed=%lu ms\r\n",
            token,
            (unsigned long)(HAL_GetTick() - tickstart)
        );

        return FALSE;
    }

    while (len--)
    {
        SPI_RxBytePtr(buff++);
    }

    /*
     * Discard CRC.
     */
    SPI_RxByte();
    SPI_RxByte();

    return TRUE;
}


/* transmit data block */
#if _USE_WRITE == 1
static bool SD_TxDataBlock(const uint8_t *buff, BYTE token)
{
    uint8_t resp = 0xFF;
    uint8_t i = 0;

    /*
     * Wait until card is ready.
     */
    if (SD_ReadyWait() != 0xFF)
    {
        purrgo_logger_write(
            "SD: TX DATA BLOCK - card not ready\r\n"
        );

        return FALSE;
    }

    /*
     * Send data token.
     */
    SPI_TxByte(token);

    /*
     * Stop transmission token does not have data.
     */
    if (token != 0xFD)
    {
        SPI_TxBuffer((uint8_t *)buff, 512);

        /*
         * Discard CRC.
         */
        SPI_RxByte();
        SPI_RxByte();

        /*
         * Receive data response.
         */
        while (i <= 64)
        {
            resp = SPI_RxByte();

            if ((resp & 0x1F) == 0x05)
            {
                break;
            }

            i++;
        }

        purrgo_logger_write(
            "SD: TX DATA response = 0x%02X after %u polls\r\n",
            resp,
            i
        );

        /*
         * Wait until card is no longer busy.
         */
        uint32_t tickstart = HAL_GetTick();

        while ((SPI_RxByte() == 0) &&
               ((HAL_GetTick() - tickstart) < 200));

        if ((HAL_GetTick() - tickstart) >= 200)
        {
            purrgo_logger_write(
                "SD: TX DATA busy timeout\r\n"
            );
        }
    }

    if ((resp & 0x1F) == 0x05)
    {
        return TRUE;
    }

    purrgo_logger_write(
        "SD: TX DATA BLOCK FAILED, response=0x%02X\r\n",
        resp
    );

    return FALSE;
}
#endif /* _USE_WRITE */


/*
 * Send SD command and return R1 response.
 *
 * Diagnostic output contains:
 *
 *   command number
 *   argument
 *   CRC
 *   response
 *   response wait count
 */
static BYTE SD_SendCmd(BYTE cmd, uint32_t arg)
{
    uint8_t crc;
    uint8_t res;
    uint8_t n = 10;

    /*
     * Wait until the card is ready to accept a command.
     */
    if (SD_ReadyWait() != 0xFF)
    {
        purrgo_logger_write(
            "SD: CMD 0x%02X arg=0x%08lX -> NOT READY\r\n",
            cmd,
            (unsigned long)arg
        );

        return 0xFF;
    }

    /*
     * Select CRC according to command.
     *
     * CMD0 and CMD8 require valid CRC values during
     * the initial card initialization sequence.
     */
    if (cmd == CMD0)
    {
        crc = 0x95;
    }
    else if (cmd == CMD8)
    {
        crc = 0x87;
    }
    else
    {
        crc = 1;
    }

    purrgo_logger_write(
        "SD: CMD 0x%02X arg=0x%08lX CRC=0x%02X\r\n",
        cmd,
        (unsigned long)arg,
        crc
    );

    /*
     * Send command packet.
     */
    SPI_TxByte(cmd);
    SPI_TxByte((uint8_t)(arg >> 24));
    SPI_TxByte((uint8_t)(arg >> 16));
    SPI_TxByte((uint8_t)(arg >> 8));
    SPI_TxByte((uint8_t)arg);

    SPI_TxByte(crc);

    /*
     * CMD12 has one additional stuff byte.
     */
    if (cmd == CMD12)
    {
        SPI_RxByte();
    }

    /*
     * Wait for R1 response.
     *
     * A response with bit 7 cleared is a valid R1 response.
     */
    do
    {
        res = SPI_RxByte();

    } while ((res & 0x80) && --n);

    purrgo_logger_write(
        "SD: CMD 0x%02X R1=0x%02X after %u polls\r\n",
        cmd,
        res,
        (unsigned)(10 - n)
    );

    return res;
}


/***************************************
 * user_diskio.c functions
 **************************************/

/*
 * Initialize SD card.
 */
DSTATUS SD_disk_initialize(BYTE drv)
{
    uint8_t n;
    uint8_t type;
    uint8_t ocr[4];

    purrgo_logger_write(
        "\r\n"
        "========================================\r\n"
        "SD: SD_disk_initialize START\r\n"
        "========================================\r\n"
    );

    purrgo_logger_write(
        "SD: drv = %u\r\n",
        drv
    );

    SD_LogStatus(
        "SD: Initial Stat =",
        Stat
    );

    /*
     * Only drive 0 is supported.
     */
    if (drv)
    {
        purrgo_logger_write(
            "SD: ERROR - unsupported drive number\r\n"
        );

        return STA_NOINIT;
    }

    /*
     * Check whether the disk was previously marked absent.
     */
    if (Stat & STA_NODISK)
    {
        purrgo_logger_write(
            "SD: ERROR - STA_NODISK is already set\r\n"
        );

        return Stat;
    }

    /*
     * Start SD SPI initialization.
     */
    SD_PowerOn();

    /*
     * Select the card.
     */
    SELECT();

    type = 0;

    purrgo_logger_write(
        "SD: Starting card type detection\r\n"
    );

    /*
     * -----------------------------------------
     * CMD0
     * -----------------------------------------
     */
    {
        BYTE response;

        response = SD_SendCmd(CMD0, 0);

        purrgo_logger_write(
            "SD: CMD0 final result = 0x%02X\r\n",
            response
        );

        if (response != 1)
        {
            purrgo_logger_write(
                "SD: INIT FAILED at CMD0\r\n"
            );

            goto init_done;
        }
    }

    /*
     * -----------------------------------------
     * CMD8
     * -----------------------------------------
     *
     * CMD8 with argument 0x1AA is used to detect
     * SD version 2 cards.
     */
    {
        BYTE response;

        response = SD_SendCmd(CMD8, 0x1AA);

        purrgo_logger_write(
            "SD: CMD8 final R1 = 0x%02X\r\n",
            response
        );

        if (response == 1)
        {
            /*
             * CMD8 R7 response consists of:
             *
             * R1 already received by SD_SendCmd()
             * followed by four bytes:
             *
             * [31:24] [23:16] [15:8] [7:0]
             *
             * Expected for argument 0x1AA:
             *
             * 00 00 01 AA
             */
            for (n = 0; n < 4; n++)
            {
                ocr[n] = SPI_RxByte();
            }

            purrgo_logger_write(
                "SD: CMD8 R7 = %02X %02X %02X %02X\r\n",
                ocr[0],
                ocr[1],
                ocr[2],
                ocr[3]
            );

            if (ocr[2] == 0x01 &&
                ocr[3] == 0xAA)
            {
                purrgo_logger_write(
                    "SD: CMD8 voltage/check pattern OK\r\n"
                );

                /*
                 * -----------------------------------------
                 * ACMD41
                 * -----------------------------------------
                 *
                 * HCS bit is set to request SDHC/SDXC
                 * high-capacity operation.
                 */
                uint32_t tickstart = HAL_GetTick();
                uint8_t timeout = 0;
                uint32_t attempts = 0;

                purrgo_logger_write(
                    "SD: Starting ACMD41 initialization\r\n"
                );

                do
                {
                    BYTE cmd55_response;
                    BYTE acmd41_response;

                    attempts++;

                    cmd55_response = SD_SendCmd(CMD55, 0);

                    purrgo_logger_write(
                        "SD: ACMD41 attempt %lu, CMD55 R1=0x%02X\r\n",
                        (unsigned long)attempts,
                        cmd55_response
                    );

                    if (cmd55_response <= 1)
                    {
                        acmd41_response =
                            SD_SendCmd(CMD41, 1UL << 30);

                        purrgo_logger_write(
                            "SD: ACMD41 attempt %lu, CMD41 R1=0x%02X\r\n",
                            (unsigned long)attempts,
                            acmd41_response
                        );

                        if (acmd41_response == 0)
                        {
                            purrgo_logger_write(
                                "SD: ACMD41 SUCCESS - card is ready\r\n"
                            );

                            break;
                        }
                    }
                    else
                    {
                        purrgo_logger_write(
                            "SD: CMD55 unexpected response 0x%02X\r\n",
                            cmd55_response
                        );
                    }

                    if ((HAL_GetTick() - tickstart) >= 1000)
                    {
                        timeout = 1;

                        purrgo_logger_write(
                            "SD: ACMD41 TIMEOUT after %lu ms\r\n",
                            (unsigned long)(
                                HAL_GetTick() - tickstart)
                        );

                        break;
                    }

                } while (1);

                /*
                 * -----------------------------------------
                 * CMD58
                 * -----------------------------------------
                 *
                 * Read OCR to determine CCS / capacity mode.
                 */
                if (!timeout)
                {
                    BYTE response;

                    response = SD_SendCmd(CMD58, 0);

                    purrgo_logger_write(
                        "SD: CMD58 R1 = 0x%02X\r\n",
                        response
                    );

                    if (response == 0)
                    {
                        for (n = 0; n < 4; n++)
                        {
                            ocr[n] = SPI_RxByte();
                        }

                        purrgo_logger_write(
                            "SD: OCR = %02X %02X %02X %02X\r\n",
                            ocr[0],
                            ocr[1],
                            ocr[2],
                            ocr[3]
                        );

                        /*
                         * CCS bit is bit 6 of OCR byte 0.
                         */
                        if (ocr[0] & 0x40)
                        {
                            type = CT_SD2 | CT_BLOCK;

                            purrgo_logger_write(
                                "SD: CCS=1 -> SDHC/SDXC block addressing\r\n"
                            );
                        }
                        else
                        {
                            type = CT_SD2;

                            purrgo_logger_write(
                                "SD: CCS=0 -> SDSC byte addressing\r\n"
                            );
                        }
                    }
                    else
                    {
                        purrgo_logger_write(
                            "SD: INIT FAILED at CMD58\r\n"
                        );
                    }
                }
            }
            else
            {
                purrgo_logger_write(
                    "SD: CMD8 check pattern INVALID\r\n"
                );
            }
        }
        else
        {
            /*
             * CMD8 failed.
             *
             * According to the original driver logic,
             * this enters the SD v1 / MMC detection path.
             */
            purrgo_logger_write(
                "SD: CMD8 did not return R1=0x01\r\n"
            );

            purrgo_logger_write(
                "SD: Trying SD v1 / MMC detection path\r\n"
            );

            BYTE cmd55_response;
            BYTE cmd41_response;

            cmd55_response = SD_SendCmd(CMD55, 0);
            cmd41_response = SD_SendCmd(CMD41, 0);

            purrgo_logger_write(
                "SD: Initial CMD55 R1=0x%02X\r\n",
                cmd55_response
            );

            purrgo_logger_write(
                "SD: Initial CMD41 R1=0x%02X\r\n",
                cmd41_response
            );

            if (cmd55_response <= 1 &&
                cmd41_response <= 1)
            {
                type = CT_SD1;

                purrgo_logger_write(
                    "SD: Detected SD v1\r\n"
                );
            }
            else
            {
                type = CT_MMC;

                purrgo_logger_write(
                    "SD: Falling back to MMC detection\r\n"
                );
            }

            /*
             * Initialize SD v1 / MMC.
             */
            {
                uint32_t tickstart = HAL_GetTick();
                uint8_t timeout = 0;
                uint32_t attempts = 0;

                do
                {
                    attempts++;

                    if (type == CT_SD1)
                    {
                        BYTE cmd55_response;
                        BYTE cmd41_response;

                        cmd55_response = SD_SendCmd(CMD55, 0);

                        if (cmd55_response <= 1)
                        {
                            cmd41_response =
                                SD_SendCmd(CMD41, 0);

                            purrgo_logger_write(
                                "SD: SD1 attempt %lu: CMD55=0x%02X CMD41=0x%02X\r\n",
                                (unsigned long)attempts,
                                cmd55_response,
                                cmd41_response
                            );

                            if (cmd41_response == 0)
                            {
                                break;
                            }
                        }
                        else
                        {
                            purrgo_logger_write(
                                "SD: SD1 attempt %lu: CMD55=0x%02X\r\n",
                                (unsigned long)attempts,
                                cmd55_response
                            );
                        }
                    }
                    else
                    {
                        BYTE cmd1_response;

                        cmd1_response = SD_SendCmd(CMD1, 0);

                        purrgo_logger_write(
                            "SD: MMC attempt %lu: CMD1=0x%02X\r\n",
                            (unsigned long)attempts,
                            cmd1_response
                        );

                        if (cmd1_response == 0)
                        {
                            break;
                        }
                    }

                    if ((HAL_GetTick() - tickstart) >= 1000)
                    {
                        timeout = 1;

                        purrgo_logger_write(
                            "SD: SD1/MMC initialization TIMEOUT\r\n"
                        );

                        break;
                    }

                } while (1);

                /*
                 * For SDSC/MMC the block length must be
                 * explicitly set to 512 bytes.
                 */
                if (!timeout)
                {
                    BYTE response;

                    response = SD_SendCmd(CMD16, 512);

                    purrgo_logger_write(
                        "SD: CMD16(512) R1 = 0x%02X\r\n",
                        response
                    );

                    if (response != 0)
                    {
                        purrgo_logger_write(
                            "SD: CMD16 FAILED\r\n"
                        );

                        type = 0;
                    }
                }
                else
                {
                    type = 0;
                }
            }
        }
    }

init_done:

    CardType = type;

    purrgo_logger_write(
        "SD: Card initialization result:\r\n"
    );

    SD_LogCardType(CardType);

    /*
     * Deselect card.
     */
    DESELECT();

    /*
     * Extra SPI clock after deselect.
     */
    SPI_RxByte();

    /*
     * Clear STA_NOINIT if a valid card type was detected.
     */
    if (type)
    {
        Stat &= ~STA_NOINIT;

        purrgo_logger_write(
            "SD: Initialization SUCCESS\r\n"
        );

        SD_LogStatus(
            "SD: Final Stat =",
            Stat
        );
    }
    else
    {
        /*
         * Initialization failed.
         */
        SD_PowerOff();

        Stat |= STA_NOINIT;

        purrgo_logger_write(
            "SD: Initialization FAILED\r\n"
        );

        SD_LogStatus(
            "SD: Final Stat =",
            Stat
        );
    }

    purrgo_logger_write(
        "========================================\r\n"
        "SD: SD_disk_initialize END\r\n"
        "========================================\r\n"
    );

    return Stat;
}


/* return disk status */
DSTATUS SD_disk_status(BYTE drv)
{
    DSTATUS status;

    if (drv)
    {
        purrgo_logger_write(
            "SD: SD_disk_status invalid drv=%u\r\n",
            drv
        );

        return STA_NOINIT;
    }

    status = Stat;

    SD_LogStatus(
        "SD: SD_disk_status =",
        status
    );

    return status;
}


/*
 * Read one or more sectors.
 */
DRESULT SD_disk_read(
    BYTE pdrv,
    BYTE *buff,
    DWORD sector,
    UINT count)
{
    DRESULT result = RES_ERROR;

    purrgo_logger_write(
        "SD: READ pdrv=%u sector=%lu count=%u\r\n",
        pdrv,
        (unsigned long)sector,
        count
    );

    if (pdrv || !count)
    {
        purrgo_logger_write(
            "SD: READ parameter error\r\n"
        );

        return RES_PARERR;
    }

    if (Stat & STA_NOINIT)
    {
        purrgo_logger_write(
            "SD: READ rejected - disk not initialized\r\n"
        );

        return RES_NOTRDY;
    }

    /*
     * Convert sector number to byte address for
     * cards which do not use block addressing.
     */
    if (!(CardType & CT_BLOCK))
    {
        purrgo_logger_write(
            "SD: READ using byte addressing\r\n"
        );

        sector *= 512;
    }
    else
    {
        purrgo_logger_write(
            "SD: READ using block addressing\r\n"
        );
    }

    SELECT();

    if (count == 1)
    {
        BYTE response;

        response = SD_SendCmd(CMD17, sector);

        purrgo_logger_write(
            "SD: CMD17 R1=0x%02X\r\n",
            response
        );

        if ((response == 0) &&
            SD_RxDataBlock(buff, 512))
        {
            count = 0;
            result = RES_OK;
        }
        else
        {
            purrgo_logger_write(
                "SD: CMD17/read block FAILED\r\n"
            );
        }
    }
    else
    {
        BYTE response;

        response = SD_SendCmd(CMD18, sector);

        purrgo_logger_write(
            "SD: CMD18 R1=0x%02X\r\n",
            response
        );

        if (response == 0)
        {
            do
            {
                if (!SD_RxDataBlock(buff, 512))
                {
                    purrgo_logger_write(
                        "SD: CMD18 data block FAILED\r\n"
                    );

                    break;
                }

                buff += 512;

            } while (--count);

            /*
             * Stop multiple-block transmission.
             */
            {
                BYTE stop_response;

                stop_response = SD_SendCmd(CMD12, 0);

                purrgo_logger_write(
                    "SD: CMD12 R1=0x%02X\r\n",
                    stop_response
                );
            }
        }
    }

    DESELECT();
    SPI_RxByte();

    if (count)
    {
        result = RES_ERROR;
    }

    SD_LogResult(
        "READ",
        result
    );

    return result;
}


/*
 * Write one or more sectors.
 */
#if _USE_WRITE == 1
DRESULT SD_disk_write(
    BYTE pdrv,
    const BYTE *buff,
    DWORD sector,
    UINT count)
{
    DRESULT result = RES_ERROR;

    purrgo_logger_write(
        "SD: WRITE pdrv=%u sector=%lu count=%u\r\n",
        pdrv,
        (unsigned long)sector,
        count
    );

    if (pdrv || !count)
    {
        purrgo_logger_write(
            "SD: WRITE parameter error\r\n"
        );

        return RES_PARERR;
    }

    if (Stat & STA_NOINIT)
    {
        purrgo_logger_write(
            "SD: WRITE rejected - disk not initialized\r\n"
        );

        return RES_NOTRDY;
    }

    if (Stat & STA_PROTECT)
    {
        purrgo_logger_write(
            "SD: WRITE rejected - write protected\r\n"
        );

        return RES_WRPRT;
    }

    /*
     * Convert sector number to byte address when
     * block addressing is not supported.
     */
    if (!(CardType & CT_BLOCK))
    {
        purrgo_logger_write(
            "SD: WRITE using byte addressing\r\n"
        );

        sector *= 512;
    }
    else
    {
        purrgo_logger_write(
            "SD: WRITE using block addressing\r\n"
        );
    }

    SELECT();

    if (count == 1)
    {
        BYTE response;

        response = SD_SendCmd(CMD24, sector);

        purrgo_logger_write(
            "SD: CMD24 R1=0x%02X\r\n",
            response
        );

        if ((response == 0) &&
            SD_TxDataBlock(buff, 0xFE))
        {
            count = 0;
            result = RES_OK;
        }
        else
        {
            purrgo_logger_write(
                "SD: CMD24/write block FAILED\r\n"
            );
        }
    }
    else
    {
        if (CardType & CT_SD1)
        {
            BYTE cmd55_response;
            BYTE cmd23_response;

            cmd55_response = SD_SendCmd(CMD55, 0);
            cmd23_response = SD_SendCmd(CMD23, count);

            purrgo_logger_write(
                "SD: ACMD23: CMD55=0x%02X CMD23=0x%02X\r\n",
                cmd55_response,
                cmd23_response
            );
        }

        {
            BYTE response;

            response = SD_SendCmd(CMD25, sector);

            purrgo_logger_write(
                "SD: CMD25 R1=0x%02X\r\n",
                response
            );

            if (response == 0)
            {
                do
                {
                    if (!SD_TxDataBlock(buff, 0xFC))
                    {
                        purrgo_logger_write(
                            "SD: CMD25 data block FAILED\r\n"
                        );

                        break;
                    }

                    buff += 512;

                } while (--count);

                /*
                 * STOP_TRAN token.
                 */
                if (!SD_TxDataBlock(0, 0xFD))
                {
                    purrgo_logger_write(
                        "SD: CMD25 STOP_TRAN failed\r\n"
                    );

                    count = 1;
                }
            }
        }
    }

    DESELECT();
    SPI_RxByte();

    if (count == 0)
    {
        result = RES_OK;
    }
    else
    {
        result = RES_ERROR;
    }

    SD_LogResult(
        "WRITE",
        result
    );

    return result;
}
#endif /* _USE_WRITE */


/*
 * FatFs ioctl interface.
 */
DRESULT SD_disk_ioctl(
    BYTE drv,
    BYTE ctrl,
    void *buff)
{
    DRESULT res;
    uint8_t n;
    uint8_t csd[16];
    uint8_t *ptr = buff;

    purrgo_logger_write(
        "SD: IOCTL drv=%u ctrl=0x%02X\r\n",
        drv,
        ctrl
    );

    if (drv)
    {
        purrgo_logger_write(
            "SD: IOCTL invalid drive\r\n"
        );

        return RES_PARERR;
    }

    res = RES_ERROR;

    /*
     * Power control is handled separately.
     */
    if (ctrl == CTRL_POWER)
    {
        purrgo_logger_write(
            "SD: IOCTL CTRL_POWER\r\n"
        );

        switch (*ptr)
        {
        case 0:
            purrgo_logger_write(
                "SD: IOCTL power OFF\r\n"
            );

            SD_PowerOff();
            res = RES_OK;
            break;

        case 1:
            purrgo_logger_write(
                "SD: IOCTL power ON\r\n"
            );

            SD_PowerOn();
            res = RES_OK;
            break;

        case 2:
            *(ptr + 1) = SD_CheckPower();

            purrgo_logger_write(
                "SD: IOCTL power CHECK -> %u\r\n",
                SD_CheckPower()
            );

            res = RES_OK;
            break;

        default:
            purrgo_logger_write(
                "SD: IOCTL invalid power command=%u\r\n",
                *ptr
            );

            res = RES_PARERR;
            break;
        }

        return res;
    }

    /*
     * Other ioctl commands require an initialized card.
     */
    if (Stat & STA_NOINIT)
    {
        purrgo_logger_write(
            "SD: IOCTL rejected - disk not initialized\r\n"
        );

        return RES_NOTRDY;
    }

    SELECT();

    switch (ctrl)
    {
    case GET_SECTOR_COUNT:
    {
        BYTE response;

        purrgo_logger_write(
            "SD: IOCTL GET_SECTOR_COUNT\r\n"
        );

        response = SD_SendCmd(CMD9, 0);

        purrgo_logger_write(
            "SD: CMD9 R1=0x%02X\r\n",
            response
        );

        if ((response == 0) &&
            SD_RxDataBlock(csd, 16))
        {
            purrgo_logger_write(
                "SD: CSD = "
                "%02X %02X %02X %02X "
                "%02X %02X %02X %02X "
                "%02X %02X %02X %02X "
                "%02X %02X %02X %02X\r\n",
                csd[0],  csd[1],  csd[2],  csd[3],
                csd[4],  csd[5],  csd[6],  csd[7],
                csd[8],  csd[9],  csd[10], csd[11],
                csd[12], csd[13], csd[14], csd[15]
            );

            if ((csd[0] >> 6) == 1)
            {
                DWORD c_size;

                c_size =
                    (DWORD)(csd[7] & 0x3F) << 16 |
                    (WORD)csd[8] << 8 |
                    csd[9];

                *(DWORD *)buff =
                    (c_size + 1) << 10;

                purrgo_logger_write(
                    "SD: Sector count = %lu\r\n",
                    (unsigned long)*(DWORD *)buff
                );
            }
            else
            {
                WORD csize;

                n =
                    (csd[5] & 15) +
                    ((csd[10] & 128) >> 7) +
                    ((csd[9] & 3) << 1) +
                    2;

                csize =
                    (csd[8] >> 6) +
                    ((WORD)csd[7] << 2) +
                    ((WORD)(csd[6] & 3) << 10) +
                    1;

                *(DWORD *)buff =
                    (DWORD)csize << (n - 9);

                purrgo_logger_write(
                    "SD: Sector count = %lu\r\n",
                    (unsigned long)*(DWORD *)buff
                );
            }

            res = RES_OK;
        }

        break;
    }


    case GET_SECTOR_SIZE:

        purrgo_logger_write(
            "SD: IOCTL GET_SECTOR_SIZE -> 512\r\n"
        );

        *(WORD *)buff = 512;
        res = RES_OK;
        break;


    case CTRL_SYNC:

        purrgo_logger_write(
            "SD: IOCTL CTRL_SYNC\r\n"
        );

        if (SD_ReadyWait() == 0xFF)
        {
            res = RES_OK;

            purrgo_logger_write(
                "SD: CTRL_SYNC -> READY\r\n"
            );
        }
        else
        {
            purrgo_logger_write(
                "SD: CTRL_SYNC -> NOT READY\r\n"
            );
        }

        break;


    case MMC_GET_CSD:
    {
        BYTE response;

        purrgo_logger_write(
            "SD: IOCTL MMC_GET_CSD\r\n"
        );

        response = SD_SendCmd(CMD9, 0);

        if ((response == 0) &&
            SD_RxDataBlock(ptr, 16))
        {
            res = RES_OK;
        }

        break;
    }


    case MMC_GET_CID:
    {
        BYTE response;

        purrgo_logger_write(
            "SD: IOCTL MMC_GET_CID\r\n"
        );

        response = SD_SendCmd(CMD10, 0);

        if ((response == 0) &&
            SD_RxDataBlock(ptr, 16))
        {
            res = RES_OK;
        }

        break;
    }


    case MMC_GET_OCR:
    {
        BYTE response;

        purrgo_logger_write(
            "SD: IOCTL MMC_GET_OCR\r\n"
        );

        response = SD_SendCmd(CMD58, 0);

        if (response == 0)
        {
            for (n = 0; n < 4; n++)
            {
                *ptr++ = SPI_RxByte();
            }

            res = RES_OK;
        }

        break;
    }


    default:

        purrgo_logger_write(
            "SD: IOCTL unsupported command=0x%02X\r\n",
            ctrl
        );

        res = RES_PARERR;
        break;
    }

    DESELECT();
    SPI_RxByte();

    SD_LogResult(
        "IOCTL",
        res
    );

    return res;
}