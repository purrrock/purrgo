/*
 * Host-тест диагностики связи с GNSS (apps/stm32/common/gnss_diag):
 * разбор NMEA, контрольная сумма, ошибки UART, определение состояния.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "gnss_diag.h"
#include "test_common.h"

/* Подать в диагностику предложение с вычисленной контрольной суммой. */
static void feed_sentence(gnss_diag_t *d, const char *body)
{
    uint8_t x = 0U;
    char tail[8];

    gnss_diag_feed(d, (uint8_t)'$');
    for (const char *p = body; *p != '\0'; p++)
    {
        gnss_diag_feed(d, (uint8_t)*p);
        x ^= (uint8_t)*p;
    }
    (void)snprintf(tail, sizeof tail, "*%02X\r\n", (unsigned)x);
    for (const char *p = tail; *p != '\0'; p++)
    {
        gnss_diag_feed(d, (uint8_t)*p);
    }
}

static void feed_text(gnss_diag_t *d, const char *s)
{
    for (; *s != '\0'; s++)
    {
        gnss_diag_feed(d, (uint8_t)*s);
    }
}

static void test_no_data(void)
{
    gnss_diag_t d;

    gnss_diag_init(&d);
    CHECK(gnss_diag_state(&d) == GNSS_DIAG_NO_DATA);
    CHECK(gnss_diag_state(NULL) == GNSS_DIAG_NO_DATA);
    gnss_diag_feed(NULL, 0U);           /* не падает */
    gnss_diag_uart_error(NULL, 1U);
}

static void test_not_nmea(void)
{
    gnss_diag_t d;

    gnss_diag_init(&d);
    for (unsigned i = 0U; i < 200U; i++)
    {
        gnss_diag_feed(&d, (uint8_t)(0x80U | (i & 0x7FU)));   /* мусор при неверной скорости */
    }
    CHECK(d.bytes == 200U);
    CHECK(d.printable < d.bytes);
    CHECK(gnss_diag_state(&d) == GNSS_DIAG_NOT_NMEA);

    /* Предложение с неверной контрольной суммой не считается. */
    feed_text(&d, "$GPGGA,1,2,3*00\r\n");
    CHECK(d.sentences_ok == 0U);
    CHECK(d.sentences_bad >= 1U);
    CHECK(gnss_diag_state(&d) == GNSS_DIAG_NOT_NMEA);
}

static void test_search_then_fix(void)
{
    gnss_diag_t d;

    gnss_diag_init(&d);

    /* Модуль шлёт NMEA, но решения нет: GGA quality 0, RMC статус V, спутники в видимости. */
    feed_sentence(&d, "GNRMC,000001.00,V,,,,,,,,,,N");
    feed_sentence(&d, "GNGGA,000001.00,,,,,0,00,99.99,,,,,,");
    feed_sentence(&d, "GPGSV,2,1,07,01,40,083,,02,17,308,,03,12,113,,04,40,200,");
    feed_sentence(&d, "GLGSV,1,1,03,65,40,083,,66,17,308,,67,12,113,");
    feed_sentence(&d, "GNGSA,A,1,,,,,,,,,,,,,99.99,99.99,99.99");

    CHECK(d.sentences_ok == 5U);
    CHECK(d.sentences_bad == 0U);
    CHECK(d.printable == d.bytes);
    CHECK(gnss_diag_state(&d) == GNSS_DIAG_NMEA_NO_FIX);
    CHECK(d.gga_seen && d.gga_quality == 0U && d.gga_sats_used == 0U);
    CHECK(d.rmc_seen && !d.rmc_valid);
    CHECK(gnss_diag_sats_in_view(&d) == 10U);   /* 7 GPS + 3 GLONASS */
    CHECK((d.seen_mask & (GNSS_DIAG_SEEN_GGA | GNSS_DIAG_SEEN_RMC | GNSS_DIAG_SEEN_GSV |
                          GNSS_DIAG_SEEN_GSA)) ==
          (GNSS_DIAG_SEEN_GGA | GNSS_DIAG_SEEN_RMC | GNSS_DIAG_SEEN_GSV | GNSS_DIAG_SEEN_GSA));

    /* Решение получено. */
    feed_sentence(&d, "GNGGA,123519.00,5354.123,N,02734.456,E,1,08,0.90,250.0,M,25.0,M,,");
    CHECK(d.gga_quality == 1U);
    CHECK(d.gga_sats_used == 8U);
    CHECK(gnss_diag_state(&d) == GNSS_DIAG_FIX);

    /* Только RMC = A тоже считается решением. */
    gnss_diag_init(&d);
    feed_sentence(&d, "GPRMC,123519.00,A,5354.123,N,02734.456,E,0.1,10.0,261026,,,A");
    CHECK(d.rmc_valid);
    CHECK(gnss_diag_state(&d) == GNSS_DIAG_FIX);
}

static void test_resync_and_noise(void)
{
    gnss_diag_t d;

    gnss_diag_init(&d);

    /* Обрыв предложения и начало нового: первое считается битым, второе разбирается. */
    feed_text(&d, "$GPGGA,123");
    feed_sentence(&d, "GPGGA,123519.00,5354.123,N,02734.456,E,2,07,1.20,250.0,M,25.0,M,,");
    CHECK(d.sentences_bad == 1U);
    CHECK(d.sentences_ok == 1U);
    CHECK(d.gga_quality == 2U);

    /* Слишком длинное предложение (больше буфера) не принимается. */
    gnss_diag_init(&d);
    gnss_diag_feed(&d, (uint8_t)'$');
    for (unsigned i = 0U; i < 150U; i++)
    {
        gnss_diag_feed(&d, (uint8_t)'A');
    }
    feed_text(&d, "*00\r\n");
    CHECK(d.sentences_ok == 0U);
    CHECK(d.sentences_bad == 1U);

    /* Предложение неизвестного типа учитывается как OTHER. */
    gnss_diag_init(&d);
    feed_sentence(&d, "GNTXT,01,01,02,ANTENNA OPEN");
    CHECK(d.sentences_ok == 1U);
    CHECK((d.seen_mask & GNSS_DIAG_SEEN_OTHER) != 0U);
}

static void test_uart_errors(void)
{
    gnss_diag_t d;

    gnss_diag_init(&d);
    gnss_diag_uart_error(&d, GNSS_DIAG_UART_ERR_FE | GNSS_DIAG_UART_ERR_NE);
    gnss_diag_uart_error(&d, GNSS_DIAG_UART_ERR_FE);
    gnss_diag_uart_error(&d, GNSS_DIAG_UART_ERR_ORE | GNSS_DIAG_UART_ERR_PE | GNSS_DIAG_UART_ERR_DMA);
    CHECK(d.err_fe == 2U);
    CHECK(d.err_ne == 1U);
    CHECK(d.err_ore == 1U);
    CHECK(d.err_pe == 1U);
    CHECK(d.err_dma == 1U);
}

int main(void)
{
    test_no_data();
    test_not_nmea();
    test_search_then_fix();
    test_resync_and_noise();
    test_uart_errors();
    return test_finish("test_gnss_diag");
}
