#ifndef PURRGO_SD_SPI_H
#define PURRGO_SD_SPI_H

/*
 * Протокол microSD-карты в SPI-режиме (SD Physical Layer Spec, глава 7;
 * последовательность инициализации - как в справочной реализации ChaN).
 *
 * Модуль не зависит от HAL и МК: обмен по SPI, управление CS и источник
 * времени передаются через sd_spi_port_t. Поэтому он проверяется host-тестом
 * (tests/host/test_sd_spi.c) на программной модели карты. Платформенная
 * привязка к HAL SPI2 - в stm32/port/storage/sd_card.c проекта U585.
 *
 * Поддерживаются карты SDSC (v1), SDHC и SDXC (v2). MMC не поддерживается.
 * CRC обмена не используется (в SPI-режиме по умолчанию выключен).
 *
 * Вызовы блокирующие, но каждое ожидание ограничено по времени
 * (tick_ms): зависание или извлечение карты возвращает ошибку.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SD_SPI_BLOCK_SIZE 512U

typedef struct
{
    /* CS = SD_CS: select == true - низкий уровень (карта выбрана). */
    void (*cs)(bool select, void *ctx);

    /*
     * Полнодуплексный обмен len байт. tx == NULL - передавать 0xFF,
     * rx == NULL - принятое отбросить. true - обмен выполнен.
     */
    bool (*xfer)(const uint8_t *tx, uint8_t *rx, size_t len, void *ctx);

    /* Монотонный счётчик миллисекунд (переполнение uint32_t допустимо). */
    uint32_t (*tick_ms)(void *ctx);

    /*
     * Вызывается один раз после успешной идентификации: перейти с частоты
     * инициализации (<= 400 кГц) на рабочую. Может быть NULL.
     */
    void (*set_fast)(void *ctx);

    void *ctx;

    /*
     * Необязательный счётчик тактов ЦП (stm32: DWT->CYCCNT). Если не NULL, sd_spi_read()
     * накапливает в sd_spi_t.stats разбор времени чтения (диагностика, см. sd_spi_stats_t).
     * Добавлен в конец структуры, прежние инициализаторы остаются верными.
     */
    uint32_t (*cycles)(void *ctx);

    /*
     * Необязательное переключение частоты SPI для приёма блоков данных: on = true - быстрая
     * (High Speed, 40 МГц), false - обычная рабочая. Вызывается только при sd_spi_t.hs и
     * только вокруг приёма данных одной команды чтения; команды, CMD12 и запись идут на обычной
     * частоте. NULL - режим High Speed не используется.
     */
    void (*set_hs)(void *ctx, bool on);
} sd_spi_port_t;

/*
 * Разбор времени sd_spi_read() по стадиям, такты ЦП (накопительно, переполнение uint32_t
 * допустимо, разности считаются беззнаковым вычитанием). Считается только в sd_spi_read()
 * и только при port.cycles != NULL.
 *   pre   - перед командой: отпускание CS, выбор карты, ожидание готовности (wait_ready);
 *   cmd   - отправка 6 байт команды и ожидание R1;
 *   tok   - ожидание токена начала данных (задержка самой карты), включая токены
 *           следующих блоков при CMD18;
 *   data  - приём 512 байт блока и двух байт CRC (чистый обмен);
 *   stop  - CMD12 после многоблочного чтения (с ожиданием R1);
 *   post  - заключительное отпускание CS;
 *   calls - число вызовов port.xfer(); polls - число опросов токена данных;
 *   reads - число sd_spi_read(); multi - из них многоблочных (CMD18); blocks - секторов.
 */
typedef struct
{
    uint32_t pre, cmd, tok, data, stop, post;
    uint32_t calls, polls, reads, multi, blocks;
} sd_spi_stats_t;

typedef enum
{
    SD_SPI_OK = 0,
    SD_SPI_ERR_PARAM,     /* неверные аргументы */
    SD_SPI_ERR_NOINIT,    /* карта не инициализирована */
    SD_SPI_ERR_NOCARD,    /* карта не отвечает на CMD0 */
    SD_SPI_ERR_UNSUPPORTED, /* MMC или напряжение карты не подходит */
    SD_SPI_ERR_TIMEOUT,   /* превышено время ожидания (занята/не отвечает) */
    SD_SPI_ERR_IO         /* ошибка обмена или отказ карты */
} sd_spi_res_t;

typedef struct
{
    sd_spi_port_t port;
    bool initialized;
    bool block_addressing;   /* SDHC/SDXC: адрес в блоках, иначе в байтах */
    uint32_t sector_count;   /* 0, если CSD прочитать не удалось */
    bool v2;                 /* карта SD v2+ (ответила на CMD8) */
    bool hs;                 /* чтение блоков данных на быстрой частоте включено */
    sd_spi_stats_t stats;    /* диагностика, см. sd_spi_stats_t */
    bool prof_on;            /* внутренний флаг: идёт замер sd_spi_read() */
} sd_spi_t;

/**
 * @brief Идентификация и инициализация карты (CMD0, CMD8, ACMD41, CMD58, CMD16).
 *
 * SPI на момент вызова должен работать на частоте <= 400 кГц. После успеха
 * вызывается port.set_fast().
 */
sd_spi_res_t sd_spi_init(sd_spi_t *sd, const sd_spi_port_t *port);

/**
 * @brief Прочитать count секторов по 512 байт, начиная с sector.
 *
 * Если включён sd_spi_t.hs и приём данных не удался (таймаут или неверный токен), быстрый
 * режим выключается насовсем, и чтение один раз повторяется на обычной частоте.
 */
sd_spi_res_t sd_spi_read(sd_spi_t *sd, uint8_t *buf, uint32_t sector, uint32_t count);

/** @brief Записать count секторов по 512 байт, начиная с sector. */
sd_spi_res_t sd_spi_write(sd_spi_t *sd, const uint8_t *buf, uint32_t sector, uint32_t count);

/** @brief Дождаться окончания внутренней записи карты. */
sd_spi_res_t sd_spi_sync(sd_spi_t *sd);

/** @brief Сбросить состояние (карта извлечена); линия CS отпускается. */
void sd_spi_deinit(sd_spi_t *sd);

#endif /* PURRGO_SD_SPI_H */
