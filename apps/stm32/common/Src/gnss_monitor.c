/*
 * Монитор связи с GNSS: периодический отчёт в лог. См. gnss_monitor.h.
 */

#include "gnss_monitor.h"

#if PURRGO_GNSS_MONITOR

#include "gnss_diag.h"
#include "main.h"
#include "purrgo/logger.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Первый отчёт - через столько мс после запуска, затем с периодом. */
#define MONITOR_FIRST_REPORT_MS   3000U
#define MONITOR_PERIOD_FAST_MS    5000U    /* первые 60 с */
#define MONITOR_PERIOD_SLOW_MS   30000U
#define MONITOR_FAST_PHASE_MS    60000U

static gnss_diag_t s_diag;
static bool s_started;
static uint32_t s_t_start;
static uint32_t s_t_next;
static uint32_t s_bytes_at_last;
static gnss_diag_state_t s_last_state = GNSS_DIAG_NO_DATA;
static uint32_t s_ubx_ack_reported = 0U;
static uint32_t s_ubx_nak_reported = 0U;
static bool s_last_reported_fix;

void purrgo_gnss_monitor_init(void)
{
    gnss_diag_init(&s_diag);
    s_t_start = HAL_GetTick();
    s_t_next = s_t_start + MONITOR_FIRST_REPORT_MS;
    s_bytes_at_last = 0U;
    s_last_state = GNSS_DIAG_NO_DATA;
    s_last_reported_fix = false;
    s_ubx_ack_reported = 0U;
    s_ubx_nak_reported = 0U;
    s_started = true;
}

void purrgo_gnss_monitor_feed(uint8_t byte)
{
    gnss_diag_feed(&s_diag, byte);
}

void purrgo_gnss_monitor_uart_error(uint32_t error_code)
{
    /* Значения HAL_UART_ERROR_* совпадают с GNSS_DIAG_UART_ERR_* (PE, NE, FE, ORE, DMA). */
    gnss_diag_uart_error(&s_diag, error_code);
}

static void seen_text(uint8_t mask, char *out, size_t size)
{
    static const struct { uint8_t bit; const char *name; } names[] = {
        { GNSS_DIAG_SEEN_GGA, "GGA " }, { GNSS_DIAG_SEEN_RMC, "RMC " },
        { GNSS_DIAG_SEEN_GSA, "GSA " }, { GNSS_DIAG_SEEN_GSV, "GSV " },
        { GNSS_DIAG_SEEN_VTG, "VTG " }, { GNSS_DIAG_SEEN_GLL, "GLL " },
        { GNSS_DIAG_SEEN_OTHER, "other " },
    };
    size_t n = 0U;

    out[0] = '\0';
    for (size_t i = 0U; i < (sizeof names / sizeof names[0]); i++)
    {
        if ((mask & names[i].bit) == 0U)
        {
            continue;
        }
        for (const char *p = names[i].name; (*p != '\0') && (n + 1U < size); p++)
        {
            out[n++] = *p;
        }
    }
    out[n] = '\0';
}

void purrgo_gnss_monitor_process(void)
{
    if (!s_started)
    {
        return;
    }

    const uint32_t now = HAL_GetTick();

    if ((int32_t)(now - s_t_next) < 0)
    {
        return;
    }

    const uint32_t uptime_ms = now - s_t_start;

    s_t_next = now + ((uptime_ms < MONITOR_FAST_PHASE_MS) ? MONITOR_PERIOD_FAST_MS
                                                          : MONITOR_PERIOD_SLOW_MS);

    const gnss_diag_state_t state = gnss_diag_state(&s_diag);
    const uint32_t delta = s_diag.bytes - s_bytes_at_last;
    const unsigned long up_s = (unsigned long)(uptime_ms / 1000U);

    s_bytes_at_last = s_diag.bytes;

    /* Ответы модуля на UBX-настройку (ACK/NAK): сообщаем при каждом изменении. */
    if ((s_diag.ubx_ack != s_ubx_ack_reported) || (s_diag.ubx_nak != s_ubx_nak_reported))
    {
        s_ubx_ack_reported = s_diag.ubx_ack;
        s_ubx_nak_reported = s_diag.ubx_nak;
        PURRGO_LOG("GNSS: UBX replies: ACK=%lu NAK=%lu (last: %s class 0x%02X id 0x%02X)\r\n",
                   (unsigned long)s_diag.ubx_ack, (unsigned long)s_diag.ubx_nak,
                   s_diag.ubx_last_was_nak ? "NAK" : "ACK",
                   (unsigned)s_diag.ubx_last_cls, (unsigned)s_diag.ubx_last_id);
    }

    /* Решение получено: сообщить один раз и дальше молчать, пока оно не пропадёт. */
    if (state == GNSS_DIAG_FIX)
    {
        if (!s_last_reported_fix || (s_last_state != GNSS_DIAG_FIX))
        {
            PURRGO_LOG("GNSS: FIX after %lu s, sats used=%u in view=%u (bytes=%lu, nmea ok=%lu)\r\n",
                       up_s, (unsigned)s_diag.gga_sats_used,
                       (unsigned)gnss_diag_sats_in_view(&s_diag),
                       (unsigned long)s_diag.bytes, (unsigned long)s_diag.sentences_ok);
            s_last_reported_fix = true;
        }
        s_last_state = state;
        return;
    }
    s_last_reported_fix = false;

    switch (state)
    {
    case GNSS_DIAG_NO_DATA:
        PURRGO_LOG("GNSS: NO DATA for %lu s (0 bytes; FE=%lu NE=%lu ORE=%lu). Check: module TX -> PA3, "
                   "module RX -> PA2, VCC/GND, module powered\r\n",
                   up_s, (unsigned long)s_diag.err_fe, (unsigned long)s_diag.err_ne,
                   (unsigned long)s_diag.err_ore);
        break;

    case GNSS_DIAG_NOT_NMEA:
    {
        const unsigned long pct = (s_diag.bytes == 0U) ? 0UL
            : (unsigned long)(((uint64_t)s_diag.printable * 100U) / s_diag.bytes);

        PURRGO_LOG("GNSS: NOT NMEA after %lu s: bytes=%lu (+%lu) printable=%lu%% bad=%lu FE=%lu NE=%lu "
                   "ORE=%lu. Wrong baud (expected 38400), noise or bad wiring\r\n",
                   up_s, (unsigned long)s_diag.bytes, (unsigned long)delta, pct,
                   (unsigned long)s_diag.sentences_bad, (unsigned long)s_diag.err_fe,
                   (unsigned long)s_diag.err_ne, (unsigned long)s_diag.err_ore);
        break;
    }

    case GNSS_DIAG_NMEA_NO_FIX:
    default:
    {
        char seen[40];

        seen_text(s_diag.seen_interval, seen, sizeof seen);
        gnss_diag_end_interval(&s_diag);
        if (delta == 0U)
        {
            PURRGO_LOG("GNSS: DATA STOPPED: no bytes in the last report period (total %lu bytes)\r\n",
                       (unsigned long)s_diag.bytes);
        }
        PURRGO_LOG("GNSS: NMEA OK, no fix yet (%lu s): bytes=%lu (+%lu) ok=%lu bad=%lu sentences (last period): %s"
                   "in view=%u used=%u fix quality=%u FE=%lu NE=%lu ORE=%lu DMA=%lu\r\n",
                   up_s, (unsigned long)s_diag.bytes, (unsigned long)delta,
                   (unsigned long)s_diag.sentences_ok, (unsigned long)s_diag.sentences_bad, seen,
                   (unsigned)gnss_diag_sats_in_view(&s_diag), (unsigned)s_diag.gga_sats_used,
                   (unsigned)s_diag.gga_quality, (unsigned long)s_diag.err_fe,
                   (unsigned long)s_diag.err_ne, (unsigned long)s_diag.err_ore,
                   (unsigned long)s_diag.err_dma);
        break;
    }
    }

    s_last_state = state;
}

#endif /* PURRGO_GNSS_MONITOR */
