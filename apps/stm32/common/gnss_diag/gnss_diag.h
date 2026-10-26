#ifndef PURRGO_GNSS_DIAG_H
#define PURRGO_GNSS_DIAG_H

/*
 * Диагностика связи с GNSS-приёмником: считает принятые байты, ошибки UART,
 * разбирает поток как NMEA (проверка контрольной суммы) и по нему определяет,
 * что происходит с модулем: не отвечает, шлёт не NMEA (скорость/шум),
 * шлёт NMEA, но решения ещё нет (ищет спутники), решение есть.
 *
 * Модуль не зависит от HAL и ядра PurrGO, проверяется host-тестом
 * (tests/host/test_gnss_diag.c). Вывод в лог и время - в gnss_monitor.c.
 */

#include <stdbool.h>
#include <stdint.h>

/* Биты ошибок UART для gnss_diag_uart_error(): значения совпадают с HAL_UART_ERROR_*. */
#define GNSS_DIAG_UART_ERR_PE  0x01U  /* чётность */
#define GNSS_DIAG_UART_ERR_NE  0x02U  /* шум */
#define GNSS_DIAG_UART_ERR_FE  0x04U  /* кадр (стоп-бит): неверная скорость / нет сигнала */
#define GNSS_DIAG_UART_ERR_ORE 0x08U  /* переполнение приёмника */
#define GNSS_DIAG_UART_ERR_DMA 0x10U  /* ошибка DMA */

/* Типы предложений NMEA, которые были приняты (с верной контрольной суммой). */
#define GNSS_DIAG_SEEN_GGA 0x01U
#define GNSS_DIAG_SEEN_RMC 0x02U
#define GNSS_DIAG_SEEN_GSA 0x04U
#define GNSS_DIAG_SEEN_GSV 0x08U
#define GNSS_DIAG_SEEN_VTG 0x10U
#define GNSS_DIAG_SEEN_GLL 0x20U
#define GNSS_DIAG_SEEN_OTHER 0x40U

#define GNSS_DIAG_TALKERS 6U
#define GNSS_DIAG_BODY_MAX 96U

typedef enum
{
    GNSS_DIAG_NO_DATA = 0,   /* ни одного байта */
    GNSS_DIAG_NOT_NMEA,      /* байты есть, но ни одного корректного предложения */
    GNSS_DIAG_NMEA_NO_FIX,   /* NMEA идёт, решения нет (поиск спутников) */
    GNSS_DIAG_FIX            /* есть решение (GGA quality > 0 или RMC = A) */
} gnss_diag_state_t;

typedef struct
{
    /* Счётчики. */
    uint32_t bytes;
    uint32_t printable;        /* печатные ASCII + CR/LF */
    uint32_t sentences_ok;
    uint32_t sentences_bad;    /* неверная контрольная сумма, обрыв, переполнение */
    uint32_t err_pe, err_ne, err_fe, err_ore, err_dma;

    /* Что видели. */
    uint8_t seen_mask;
    bool gga_seen;
    uint8_t gga_quality;       /* поле 6 GGA */
    uint8_t gga_sats_used;     /* поле 7 GGA */
    bool rmc_seen;
    bool rmc_valid;            /* статус A */
    uint8_t gsv_in_view[GNSS_DIAG_TALKERS];
    bool gsv_seen;

    /* Состояние разбора. */
    uint8_t parse_state;
    uint8_t cs_hi;
    uint8_t xor_acc;
    uint8_t body_len;
    bool body_overflow;
    char body[GNSS_DIAG_BODY_MAX];
} gnss_diag_t;

void gnss_diag_init(gnss_diag_t *d);

/** @brief Обработать один принятый байт. */
void gnss_diag_feed(gnss_diag_t *d, uint8_t byte);

/** @brief Учесть ошибку UART (набор битов GNSS_DIAG_UART_ERR_*). */
void gnss_diag_uart_error(gnss_diag_t *d, uint32_t err_bits);

/** @brief Общее число спутников в видимости (сумма по системам из GSV). */
uint16_t gnss_diag_sats_in_view(const gnss_diag_t *d);

gnss_diag_state_t gnss_diag_state(const gnss_diag_t *d);

#endif /* PURRGO_GNSS_DIAG_H */
