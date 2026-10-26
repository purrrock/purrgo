/*
 * Host-тест протокола microSD в SPI-режиме (apps/stm32/common/sd_spi).
 *
 * Карта моделируется на уровне байтов SPI: разбор кадров команд, R1/R7,
 * токены данных, ответ на запись, занятость. Проверяются инициализация SDHC,
 * SDSC v2 и SDSC v1, одно- и многоблочные чтение/запись, адресация,
 * объём по CSD и поведение при сбоях (нет карты, вечная занятость,
 * ошибка чтения, отказ записи).
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sd_spi.h"
#include "test_common.h"

#define BLK SD_SPI_BLOCK_SIZE
#define MODEL_SECTORS 32768U /* 16 МБ */

typedef enum
{
    ST_CMD = 0,
    ST_WR_TOKEN,
    ST_WR_DATA,
    ST_RD_MULTI
} model_state_t;

typedef struct
{
    /* Конфигурация. */
    bool present;
    bool v2;                  /* отвечает на CMD8 */
    bool sdhc;                /* CCS = 1, адресация блоками */
    unsigned acmd41_polls;    /* сколько раз ACMD41 отвечает "idle" */
    bool stuck_busy;          /* DO удерживается в 0 */
    bool read_error;          /* вместо токена данных - токен ошибки */
    bool reject_write;        /* ответ на запись: CRC error */
    unsigned busy_len;        /* байт занятости после блока записи */

    /* Состояние. */
    bool cs;
    bool idle;
    bool app_next;
    unsigned acmd41_seen;
    model_state_t st;
    uint8_t cmd[6];
    unsigned cmd_len;
    uint8_t out[BLK + 16];
    size_t out_len, out_pos;
    uint32_t busy_left;
    uint32_t cur_sector;
    uint8_t wr_token;
    size_t wr_pos;
    uint8_t wr_buf[BLK];
    bool multi_write;

    uint8_t *data;
    uint32_t sectors;

    /* Наблюдения. */
    unsigned ff_clocks_before_cmd0;
    bool cmd0_seen;
    uint32_t last_addr_arg;
    unsigned set_fast_calls;
    uint32_t tick;
} model_t;

static void q_byte(model_t *m, uint8_t b)
{
    if (m->out_len < sizeof m->out)
    {
        m->out[m->out_len++] = b;
    }
}

static void q_reset(model_t *m)
{
    m->out_len = 0U;
    m->out_pos = 0U;
}

static void q_block(model_t *m, uint32_t sector)
{
    q_byte(m, m->read_error ? 0x01U : 0xFEU);
    for (size_t i = 0U; i < BLK && m->out_len < sizeof m->out; i++)
    {
        m->out[m->out_len++] = m->data[(size_t)sector * BLK + i];
    }
    q_byte(m, 0x12U);
    q_byte(m, 0x34U);
}

static void make_csd(const model_t *m, uint8_t csd[16])
{
    memset(csd, 0, 16);
    if (m->sdhc || m->v2)
    {
        /* CSD v2: C_SIZE = sectors / 1024 - 1 */
        const uint32_t c = (m->sectors >> 10) - 1U;

        csd[0] = 0x40U;
        csd[7] = (uint8_t)((c >> 16) & 0x3FU);
        csd[8] = (uint8_t)(c >> 8);
        csd[9] = (uint8_t)c;
    }
    else
    {
        /* CSD v1: READ_BL_LEN = 9, C_SIZE_MULT = 7, C_SIZE = sectors / 512 - 1 */
        const uint32_t c = (m->sectors >> 9) - 1U;

        csd[5] = 9U;
        csd[6] = (uint8_t)((c >> 10) & 3U);
        csd[7] = (uint8_t)(c >> 2);
        csd[8] = (uint8_t)((c & 3U) << 6);
        csd[9] = 3U;
        csd[10] = 0x80U;
    }
}

static uint32_t sector_from_arg(const model_t *m, uint32_t arg)
{
    return m->sdhc ? arg : (arg / BLK);
}

static void handle_cmd(model_t *m)
{
    const uint8_t idx = (uint8_t)(m->cmd[0] & 0x3FU);
    const uint32_t arg = ((uint32_t)m->cmd[1] << 24) | ((uint32_t)m->cmd[2] << 16) |
                         ((uint32_t)m->cmd[3] << 8) | (uint32_t)m->cmd[4];
    const bool app = m->app_next;

    m->app_next = false;
    q_reset(m);
    q_byte(m, 0xFFU); /* Ncr */

    if (idx == 0U)
    {
        m->cmd0_seen = true;
        m->idle = true;
        q_byte(m, 0x01U);
    }
    else if (idx == 8U)
    {
        if (m->v2)
        {
            q_byte(m, 0x01U);
            q_byte(m, 0x00U);
            q_byte(m, 0x00U);
            q_byte(m, 0x01U);
            q_byte(m, 0xAAU);
        }
        else
        {
            q_byte(m, 0x05U); /* illegal command */
        }
    }
    else if (idx == 55U)
    {
        m->app_next = true;
        q_byte(m, m->idle ? 0x01U : 0x00U);
    }
    else if (idx == 41U && app)
    {
        m->acmd41_seen++;
        if (m->acmd41_seen > m->acmd41_polls)
        {
            m->idle = false;
        }
        q_byte(m, m->idle ? 0x01U : 0x00U);
    }
    else if (idx == 23U && app)
    {
        q_byte(m, 0x00U);
    }
    else if (idx == 58U)
    {
        q_byte(m, 0x00U);
        q_byte(m, m->sdhc ? 0xC0U : 0x80U);
        q_byte(m, 0xFFU);
        q_byte(m, 0x80U);
        q_byte(m, 0x00U);
    }
    else if (idx == 16U)
    {
        q_byte(m, (arg == BLK) ? 0x00U : 0x04U);
    }
    else if (idx == 9U)
    {
        uint8_t csd[16];

        make_csd(m, csd);
        q_byte(m, 0x00U);
        q_byte(m, 0xFFU);
        q_byte(m, 0xFEU);
        for (size_t i = 0U; i < 16U; i++)
        {
            q_byte(m, csd[i]);
        }
        q_byte(m, 0x00U);
        q_byte(m, 0x00U);
    }
    else if (idx == 17U || idx == 18U)
    {
        m->last_addr_arg = arg;
        m->cur_sector = sector_from_arg(m, arg);
        if (m->cur_sector >= m->sectors)
        {
            q_byte(m, 0x04U);
        }
        else
        {
            q_byte(m, 0x00U);
            q_byte(m, 0xFFU);
            q_block(m, m->cur_sector);
            if (idx == 18U)
            {
                m->cur_sector++;
                m->st = ST_RD_MULTI;
            }
        }
    }
    else if (idx == 12U)
    {
        m->st = ST_CMD;
        q_reset(m);
        q_byte(m, 0xFFU); /* stuff byte */
        q_byte(m, 0x00U);
        m->busy_left = 2U;
    }
    else if (idx == 24U || idx == 25U)
    {
        m->last_addr_arg = arg;
        m->cur_sector = sector_from_arg(m, arg);
        m->multi_write = (idx == 25U);
        q_byte(m, (m->cur_sector < m->sectors) ? 0x00U : 0x04U);
        if (m->cur_sector < m->sectors)
        {
            m->st = ST_WR_TOKEN;
        }
    }
    else
    {
        q_byte(m, 0x04U);
    }
}

/* Один байт SPI: возвращает байт на линии MISO, обрабатывает MOSI. */
static uint8_t model_byte(model_t *m, uint8_t tx)
{
    uint8_t rx = 0xFFU;

    if (!m->present)
    {
        return 0xFFU;
    }

    if (!m->cs)
    {
        /* CS = 1: карта слышит только такты (пробуждение, 74 такта). */
        if ((tx == 0xFFU) && !m->cmd0_seen)
        {
            m->ff_clocks_before_cmd0++;
        }
        return 0xFFU;
    }

    /* Что карта выдаёт на этом такте. */
    if (m->stuck_busy)
    {
        rx = 0x00U;
    }
    else if (m->out_pos < m->out_len)
    {
        rx = m->out[m->out_pos++];
    }
    else if (m->busy_left > 0U)
    {
        rx = 0x00U;
        m->busy_left--;
    }
    else if (m->st == ST_RD_MULTI)
    {
        q_reset(m);
        if (m->cur_sector < m->sectors)
        {
            q_block(m, m->cur_sector++);
            rx = m->out[m->out_pos++];
        }
    }

    /* Что карта делает с принятым байтом. */
    switch (m->st)
    {
    case ST_WR_TOKEN:
        if (tx == 0xFEU || tx == 0xFCU)
        {
            m->wr_token = tx;
            m->wr_pos = 0U;
            m->st = ST_WR_DATA;
        }
        else if (tx == 0xFDU)
        {
            m->st = ST_CMD;
            m->busy_left = m->busy_len;
        }
        break;

    case ST_WR_DATA:
        if (m->wr_pos < BLK)
        {
            m->wr_buf[m->wr_pos++] = tx;
        }
        else
        {
            m->wr_pos++;
            if (m->wr_pos == BLK + 2U)
            {
                q_reset(m);
                if (m->reject_write)
                {
                    q_byte(m, 0x0DU);
                }
                else
                {
                    q_byte(m, 0x05U);
                    memcpy(&m->data[(size_t)m->cur_sector * BLK], m->wr_buf, BLK);
                    m->cur_sector++;
                }
                m->busy_left = m->busy_len;
                m->st = m->multi_write ? ST_WR_TOKEN : ST_CMD;
            }
        }
        break;

    case ST_RD_MULTI:
    case ST_CMD:
    default:
        if (m->cmd_len > 0U)
        {
            m->cmd[m->cmd_len++] = tx;
            if (m->cmd_len == 6U)
            {
                m->cmd_len = 0U;
                handle_cmd(m);
            }
        }
        else if ((tx & 0xC0U) == 0x40U)
        {
            m->cmd[0] = tx;
            m->cmd_len = 1U;
            if (m->st == ST_RD_MULTI)
            {
                q_reset(m);
            }
        }
        break;
    }

    return rx;
}

/* ------------------------------------------------------------------ port --- */

static void port_cs(bool select, void *ctx)
{
    ((model_t *)ctx)->cs = select;
}

static bool port_xfer(const uint8_t *tx, uint8_t *rx, size_t len, void *ctx)
{
    model_t *m = (model_t *)ctx;

    for (size_t i = 0U; i < len; i++)
    {
        const uint8_t r = model_byte(m, (tx != NULL) ? tx[i] : 0xFFU);

        if (rx != NULL)
        {
            rx[i] = r;
        }
    }
    return true;
}

static uint32_t port_tick(void *ctx)
{
    return ((model_t *)ctx)->tick++;
}

static void port_fast(void *ctx)
{
    ((model_t *)ctx)->set_fast_calls++;
}

static void model_setup(model_t *m, bool v2, bool sdhc)
{
    memset(m, 0, sizeof *m);
    m->present = true;
    m->v2 = v2;
    m->sdhc = sdhc;
    m->acmd41_polls = 3U;
    m->busy_len = 3U;
    m->sectors = MODEL_SECTORS;
    m->data = (uint8_t *)malloc((size_t)MODEL_SECTORS * BLK);
    for (size_t i = 0U; i < (size_t)MODEL_SECTORS * BLK; i++)
    {
        m->data[i] = (uint8_t)((i * 7U + (i >> 9)) & 0xFFU);
    }
}

static void model_free(model_t *m)
{
    free(m->data);
    m->data = NULL;
}

static sd_spi_port_t make_port(model_t *m)
{
    sd_spi_port_t p;

    p.cs = port_cs;
    p.xfer = port_xfer;
    p.tick_ms = port_tick;
    p.set_fast = port_fast;
    p.ctx = m;
    return p;
}

static bool sectors_equal(const model_t *m, uint32_t first, uint32_t n, const uint8_t *buf)
{
    return memcmp(&m->data[(size_t)first * BLK], buf, (size_t)n * BLK) == 0;
}

/* ----------------------------------------------------------------- tests --- */

static void test_init_variants(void)
{
    sd_spi_t sd;
    model_t m;
    sd_spi_port_t p;

    /* SDHC. */
    model_setup(&m, true, true);
    p = make_port(&m);
    CHECK(sd_spi_init(&sd, &p) == SD_SPI_OK);
    CHECK(sd.initialized && sd.block_addressing);
    CHECK(sd.sector_count == MODEL_SECTORS);
    CHECK(m.set_fast_calls == 1U);
    CHECK(m.ff_clocks_before_cmd0 >= 10U);   /* >= 74 тактов */
    CHECK(!m.cs);                            /* CS отпущен */
    model_free(&m);

    /* SDSC v2 (CCS = 0): байтовая адресация. */
    model_setup(&m, true, false);
    p = make_port(&m);
    CHECK(sd_spi_init(&sd, &p) == SD_SPI_OK);
    CHECK(sd.initialized && !sd.block_addressing);
    CHECK(sd.sector_count == MODEL_SECTORS);
    model_free(&m);

    /* SDSC v1 (на CMD8 - illegal command). */
    model_setup(&m, false, false);
    p = make_port(&m);
    CHECK(sd_spi_init(&sd, &p) == SD_SPI_OK);
    CHECK(sd.initialized && !sd.block_addressing);
    CHECK(sd.sector_count == MODEL_SECTORS);
    model_free(&m);
}

static void test_init_failures(void)
{
    sd_spi_t sd;
    model_t m;
    sd_spi_port_t p;

    /* Карты нет. */
    model_setup(&m, true, true);
    m.present = false;
    p = make_port(&m);
    CHECK(sd_spi_init(&sd, &p) == SD_SPI_ERR_NOCARD);
    CHECK(!sd.initialized);
    CHECK(m.set_fast_calls == 0U);
    CHECK(!m.cs);
    model_free(&m);

    /* Карта никогда не выходит из idle. */
    model_setup(&m, true, true);
    m.acmd41_polls = 1000000U;
    p = make_port(&m);
    CHECK(sd_spi_init(&sd, &p) == SD_SPI_ERR_TIMEOUT);
    CHECK(!sd.initialized);
    CHECK(m.set_fast_calls == 0U);
    model_free(&m);

    /* Неверные аргументы. */
    CHECK(sd_spi_init(NULL, &p) == SD_SPI_ERR_PARAM);
    CHECK(sd_spi_init(&sd, NULL) == SD_SPI_ERR_PARAM);
    sd_spi_port_t bad = p;
    bad.xfer = NULL;
    CHECK(sd_spi_init(&sd, &bad) == SD_SPI_ERR_PARAM);
}

static void test_read(void)
{
    static uint8_t buf[BLK * 5U];
    const bool addressing[2] = { true, false };

    for (unsigned a = 0U; a < 2U; a++)
    {
        sd_spi_t sd;
        model_t m;

        model_setup(&m, true, addressing[a]);
        sd_spi_port_t p = make_port(&m);
        CHECK(sd_spi_init(&sd, &p) == SD_SPI_OK);

        memset(buf, 0, sizeof buf);
        CHECK(sd_spi_read(&sd, buf, 123U, 1U) == SD_SPI_OK);
        CHECK(sectors_equal(&m, 123U, 1U, buf));
        CHECK(m.last_addr_arg == (addressing[a] ? 123U : 123U * BLK));
        CHECK(!m.cs);

        memset(buf, 0, sizeof buf);
        CHECK(sd_spi_read(&sd, buf, 1000U, 5U) == SD_SPI_OK);
        CHECK(sectors_equal(&m, 1000U, 5U, buf));
        CHECK(m.st == ST_CMD);               /* CMD12 принят */
        CHECK(!m.cs);

        /* Последний сектор карты и выход за границу. */
        CHECK(sd_spi_read(&sd, buf, MODEL_SECTORS - 1U, 1U) == SD_SPI_OK);
        CHECK(sd_spi_read(&sd, buf, MODEL_SECTORS, 1U) == SD_SPI_ERR_IO);
        CHECK(!m.cs);
        model_free(&m);
    }
}

static void test_write(void)
{
    static uint8_t buf[BLK * 4U];
    const bool addressing[2] = { true, false };

    for (unsigned a = 0U; a < 2U; a++)
    {
        sd_spi_t sd;
        model_t m;

        model_setup(&m, true, addressing[a]);
        sd_spi_port_t p = make_port(&m);
        CHECK(sd_spi_init(&sd, &p) == SD_SPI_OK);

        for (size_t i = 0U; i < sizeof buf; i++)
        {
            buf[i] = (uint8_t)(0xA5U ^ (i * 3U));
        }

        CHECK(sd_spi_write(&sd, buf, 77U, 1U) == SD_SPI_OK);
        CHECK(sectors_equal(&m, 77U, 1U, buf));
        CHECK(m.last_addr_arg == (addressing[a] ? 77U : 77U * BLK));

        CHECK(sd_spi_write(&sd, buf, 2000U, 4U) == SD_SPI_OK);
        CHECK(sectors_equal(&m, 2000U, 4U, buf));
        CHECK(m.st == ST_CMD);
        CHECK(!m.cs);

        /* Запись и повторное чтение дают те же данные. */
        static uint8_t back[BLK * 4U];
        CHECK(sd_spi_read(&sd, back, 2000U, 4U) == SD_SPI_OK);
        CHECK(memcmp(back, buf, sizeof buf) == 0);

        CHECK(sd_spi_sync(&sd) == SD_SPI_OK);
        model_free(&m);
    }
}

static void test_faults(void)
{
    static uint8_t buf[BLK * 2U];
    sd_spi_t sd;
    model_t m;

    /* Ошибка чтения (токен ошибки вместо данных). */
    model_setup(&m, true, true);
    sd_spi_port_t p = make_port(&m);
    CHECK(sd_spi_init(&sd, &p) == SD_SPI_OK);
    m.read_error = true;
    CHECK(sd_spi_read(&sd, buf, 5U, 1U) == SD_SPI_ERR_IO);
    CHECK(!m.cs);
    m.read_error = false;
    CHECK(sd_spi_read(&sd, buf, 5U, 1U) == SD_SPI_OK);   /* карта жива */
    model_free(&m);

    /* Карта отклонила блок записи. */
    model_setup(&m, true, true);
    p = make_port(&m);
    CHECK(sd_spi_init(&sd, &p) == SD_SPI_OK);
    m.reject_write = true;
    CHECK(sd_spi_write(&sd, buf, 5U, 1U) == SD_SPI_ERR_IO);
    CHECK(sd_spi_write(&sd, buf, 5U, 2U) == SD_SPI_ERR_IO);
    CHECK(!m.cs);
    model_free(&m);

    /* Карта зависла в состоянии "занята": все вызовы возвращаются с ошибкой. */
    model_setup(&m, true, true);
    p = make_port(&m);
    CHECK(sd_spi_init(&sd, &p) == SD_SPI_OK);
    m.stuck_busy = true;
    CHECK(sd_spi_read(&sd, buf, 5U, 1U) != SD_SPI_OK);
    CHECK(sd_spi_write(&sd, buf, 5U, 1U) != SD_SPI_OK);
    CHECK(sd_spi_sync(&sd) == SD_SPI_ERR_TIMEOUT);
    CHECK(!m.cs);
    model_free(&m);

    /* Карта "исчезла" (линия DO = 0xFF, ответов нет). */
    model_setup(&m, true, true);
    p = make_port(&m);
    CHECK(sd_spi_init(&sd, &p) == SD_SPI_OK);
    m.present = false;
    CHECK(sd_spi_read(&sd, buf, 5U, 1U) != SD_SPI_OK);
    CHECK(sd_spi_write(&sd, buf, 5U, 1U) != SD_SPI_OK);
    CHECK(!m.cs);
    model_free(&m);
}

static void test_state_and_params(void)
{
    static uint8_t buf[BLK];
    sd_spi_t sd;
    model_t m;

    model_setup(&m, true, true);
    sd_spi_port_t p = make_port(&m);

    memset(&sd, 0, sizeof sd);
    sd.port = p;
    CHECK(sd_spi_read(&sd, buf, 0U, 1U) == SD_SPI_ERR_NOINIT);
    CHECK(sd_spi_write(&sd, buf, 0U, 1U) == SD_SPI_ERR_NOINIT);
    CHECK(sd_spi_sync(&sd) == SD_SPI_ERR_NOINIT);

    CHECK(sd_spi_init(&sd, &p) == SD_SPI_OK);
    CHECK(sd_spi_read(&sd, NULL, 0U, 1U) == SD_SPI_ERR_PARAM);
    CHECK(sd_spi_read(&sd, buf, 0U, 0U) == SD_SPI_ERR_PARAM);
    CHECK(sd_spi_write(&sd, NULL, 0U, 1U) == SD_SPI_ERR_PARAM);
    CHECK(sd_spi_write(&sd, buf, 0U, 0U) == SD_SPI_ERR_PARAM);
    CHECK(sd_spi_read(NULL, buf, 0U, 1U) == SD_SPI_ERR_PARAM);

    sd_spi_deinit(&sd);
    CHECK(!sd.initialized);
    CHECK(!m.cs);
    CHECK(sd_spi_read(&sd, buf, 0U, 1U) == SD_SPI_ERR_NOINIT);

    /* Повторная инициализация после deinit. */
    CHECK(sd_spi_init(&sd, &p) == SD_SPI_OK);
    CHECK(sd_spi_read(&sd, buf, 9U, 1U) == SD_SPI_OK);
    sd_spi_deinit(NULL);
    model_free(&m);
}

int main(void)
{
    test_init_variants();
    test_init_failures();
    test_read();
    test_write();
    test_faults();
    test_state_and_params();
    return test_finish("test_sd_spi");
}
