/*
 * Монитор связи с GNSS: периодический отчёт в лог. См. gnss_monitor.h.
 */

#include "gnss_monitor.h"

#if PURRGO_GNSS_MONITOR

#include "gnss_diag.h"
#include "main.h"
#include "purrgo/gnss_ubx.h"
#include "purrgo/hardware_config.h"
#include "purrgo/hw_fault.h"
#include "purrgo/logger.h"
#include "purrgo/system_time.h"

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
static bool s_expect_gaps;   /* приёмник в PSMOO: между фиксами тишина - штатно */
static uint8_t s_bad_reports;   /* подряд отчётов "нет данных" / "не NMEA": сбой железа с 2-го (холодный старт не в счёт) */

#define MONITOR_BAD_REPORTS_FAULT 2U

/*
 * Отчёт по периодам трекера (только сборка с уровнем DEBUG и PURRGO_GNSS_LOG_PERIODIC = 1): раз в PURRGO_GNSS_TRACKER_PERIOD_S строка
 * "GNSS: tracker #N: FIX/NO FIX ..." - было ли за период действительное решение (PVT с fixOK). Окно
 * отсчитывается от входа в трекер: период приёмника (PSMOO/PSMCT) считает тот же кварц с точностью до ppm,
 * так что окно держится в фазе с циклами приёмника, но сдвиг от начала цикла произвольный.
 */
#if PURRGO_GNSS_TRACKER_ACTIVE && PURRGO_GNSS_LOG_PERIODIC && (PURRGO_LOG_LEVEL >= PURRGO_LOG_LEVEL_DEBUG)
#define MONITOR_TRACKER_REPORT 1
#else
#define MONITOR_TRACKER_REPORT 0
#endif

#if MONITOR_TRACKER_REPORT
static uint32_t s_trk_next;          /* конец текущего окна */
static uint32_t s_trk_n;             /* номер окна */
static uint32_t s_trk_pvt_base;      /* счётчики на начало окна */
static uint32_t s_trk_fix_base;
static uint32_t s_trk_bytes_base;
static uint32_t s_trk_err_base;

static uint32_t trk_errors(void)
{
    return s_diag.err_fe + s_diag.err_ne + s_diag.err_ore + s_diag.err_dma + s_diag.err_pe;
}

static uint32_t trk_delta(uint32_t now, uint32_t base)
{
    return (now >= base) ? (now - base) : now;   /* счётчик обнулился (init/смена скорости) */
}

static void tracker_window_reset(uint32_t now)
{
    const purrgo_gnss_ubx_status_t *ubx = purrgo_gnss_ubx_status();

    s_trk_next = now + (uint32_t)PURRGO_GNSS_TRACKER_PERIOD_S * 1000U;
    s_trk_n = 0U;
    s_trk_pvt_base = ubx->pvt_count;
    s_trk_fix_base = ubx->pvt_fix_count;
    s_trk_bytes_base = s_diag.bytes;
    s_trk_err_base = trk_errors();
}

static void tracker_report(uint32_t now)
{
    if ((int32_t)(now - s_trk_next) < 0)
    {
        return;
    }
    s_trk_next += (uint32_t)PURRGO_GNSS_TRACKER_PERIOD_S * 1000U;
    if ((int32_t)(now - s_trk_next) >= 0)
    {
        s_trk_next = now + (uint32_t)PURRGO_GNSS_TRACKER_PERIOD_S * 1000U;   /* главный цикл долго спал */
    }

    const purrgo_gnss_ubx_status_t *ubx = purrgo_gnss_ubx_status();
    const uint32_t pvt = trk_delta(ubx->pvt_count, s_trk_pvt_base);
    const uint32_t fix = trk_delta(ubx->pvt_fix_count, s_trk_fix_base);
    const unsigned long bytes = (unsigned long)trk_delta(s_diag.bytes, s_trk_bytes_base);
    const unsigned long err = (unsigned long)trk_delta(trk_errors(), s_trk_err_base);

    s_trk_n++;
    s_trk_pvt_base = ubx->pvt_count;
    s_trk_fix_base = ubx->pvt_fix_count;
    s_trk_bytes_base = s_diag.bytes;
    s_trk_err_base = trk_errors();

    if (fix != 0U)
    {
        PURRGO_LOGD("GNSS: tracker #%lu: FIX sv=%u hAcc=%lu mm (PVT %lu, with fix %lu) psm=%u bytes=+%lu err=+%lu\r\n",
                    (unsigned long)s_trk_n, (unsigned)ubx->pvt_num_sv, (unsigned long)ubx->pvt_hacc_mm,
                    (unsigned long)pvt, (unsigned long)fix, (unsigned)ubx->psm_state, bytes, err);
    }
    else if (pvt != 0U)
    {
        PURRGO_LOGD("GNSS: tracker #%lu: NO FIX (PVT %lu) fixType=%u sv=%u validTime=%u psm=%u bytes=+%lu err=+%lu%s\r\n",
                    (unsigned long)s_trk_n, (unsigned long)pvt, (unsigned)ubx->pvt_fix_type,
                    (unsigned)ubx->pvt_num_sv, (unsigned)ubx->pvt_time_valid, (unsigned)ubx->psm_state,
                    bytes, err, (ubx->pvt_fix_count != 0U) ? "" : " (no fix yet since start)");
    }
    else
    {
        PURRGO_LOGD("GNSS: tracker #%lu: NO FIX, no PVT from receiver (psm=%u bytes=+%lu err=+%lu)\r\n",
                    (unsigned long)s_trk_n, (unsigned)ubx->psm_state, bytes, err);
    }
}
#endif /* MONITOR_TRACKER_REPORT */

/* Связь с модулем (gnss_link): скорость UART и признак "поиск скорости закончен". */
static uint32_t s_baud = PURRGO_GNSS_BAUD_DEFAULT;
static bool s_link_ready = true;

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
    s_bad_reports = 0U;
    s_started = true;
#if MONITOR_TRACKER_REPORT
    tracker_window_reset(s_t_start);
#endif
}

void purrgo_gnss_monitor_link(uint32_t baud, bool ready)
{
    s_baud = baud;
    s_link_ready = ready;
    if (!ready)
    {
        return;
    }

    /*
     * Всё, что пришло во время поиска скорости (мусор на неверной скорости,
     * ошибки кадра), в диагностику не входит: счётчики с нуля, первый отчёт
     * через MONITOR_FIRST_REPORT_MS. Время отсчёта s_t_start не меняется:
     * "FIX after N s" по-прежнему считается от запуска GNSS.
     */
    gnss_diag_init(&s_diag);
    s_bytes_at_last = 0U;
    s_last_state = GNSS_DIAG_NO_DATA;
    s_last_reported_fix = false;
    s_ubx_ack_reported = 0U;
    s_ubx_nak_reported = 0U;
    s_bad_reports = 0U;
    s_t_next = HAL_GetTick() + MONITOR_FIRST_REPORT_MS;
#if MONITOR_TRACKER_REPORT
    tracker_window_reset(HAL_GetTick());
#endif
}

void purrgo_gnss_monitor_stop(void)
{
    s_started = false;
    s_expect_gaps = false;
}

void purrgo_gnss_monitor_feed(uint8_t byte)
{
    gnss_diag_feed(&s_diag, byte);
}

#if PURRGO_GNSS_NMEA_OFF
/* NMEA выключен: решение действительно, если свежий PVT сообщает fixOK (то же, что solution.valid). */
static bool monitor_pvt_fix(void)
{
    const purrgo_gnss_ubx_status_t *ubx = purrgo_gnss_ubx_status();

    return ubx->pvt_valid && ubx->pvt_fix_ok &&
           purrgo_gnss_ubx_fresh(ubx->pvt_ms, purrgo_system_time_ms());
}
#endif

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

void purrgo_gnss_monitor_expect_gaps(bool expect)
{
#if MONITOR_TRACKER_REPORT
    if (expect && !s_expect_gaps)
    {
        tracker_window_reset(HAL_GetTick());   /* вход в трекер: окно отсчитывается от этого момента */
    }
#endif
    s_expect_gaps = expect;
}

void purrgo_gnss_monitor_process(void)
{
    if (!s_started)
    {
        return;
    }

    const uint32_t now = HAL_GetTick();

    if (!s_link_ready)
    {
        return;
    }

#if MONITOR_TRACKER_REPORT
    if (s_expect_gaps)
    {
        tracker_report(now);
    }
#endif

    if ((int32_t)(now - s_t_next) < 0)
    {
        return;
    }

    const uint32_t uptime_ms = now - s_t_start;

    s_t_next = now + ((uptime_ms < MONITOR_FAST_PHASE_MS) ? MONITOR_PERIOD_FAST_MS
                                                          : MONITOR_PERIOD_SLOW_MS);

    gnss_diag_state_t state = gnss_diag_state(&s_diag);
#if PURRGO_GNSS_NMEA_OFF
    /* Связь есть (кадры UBX идут), а есть ли решение, говорит PVT: NMEA нет. */
    if ((state == GNSS_DIAG_FIX) || (state == GNSS_DIAG_NMEA_NO_FIX))
    {
        state = monitor_pvt_fix() ? GNSS_DIAG_FIX : GNSS_DIAG_NMEA_NO_FIX;
    }
#endif
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

#if (PURRGO_GNSS_UBX != 0) && PURRGO_GNSS_PSM_ANY
    /* Состояние энергосбережения приёмника (NAV-PVT.psmState): сообщаем при смене. */
    {
        static const char *const psm_names[] = {
            "off", "enabled", "acquisition", "tracking", "power-optimized tracking", "inactive",
        };
        static int16_t s_psm_reported = -1;
        const purrgo_gnss_ubx_status_t *ubx = purrgo_gnss_ubx_status();

        if (ubx->psm_valid && ((int16_t)ubx->psm_state != s_psm_reported))
        {
            s_psm_reported = (int16_t)ubx->psm_state;
            PURRGO_LOG("GNSS: PSM state %u (%s)\r\n", (unsigned)ubx->psm_state,
                       (ubx->psm_state < (sizeof psm_names / sizeof psm_names[0]))
                           ? psm_names[ubx->psm_state] : "?");
        }
    }
#endif

    /*
     * PSMOO (трекер): приёмник молчит между фиксами, "нет данных", "нет фикса" и "связь есть, решения
     * нет" здесь штатны. Состояние PSM и ответы приёмника выше сообщаются, остальное - нет.
     */
    if (s_expect_gaps)
    {
        s_last_state = state;
        s_bad_reports = 0U;
        return;
    }

    /* Данные пошли (NMEA/UBX, решение есть или ещё нет): модуль жив, эпизод сбоя связи закончен. */
    if ((state == GNSS_DIAG_FIX) || (state == GNSS_DIAG_NMEA_NO_FIX))
    {
        s_bad_reports = 0U;
        purrgo_hw_fault_clear(PURRGO_HWF_GNSS);
    }

    /* Решение получено: сообщить один раз и дальше молчать, пока оно не пропадёт. */
    if (state == GNSS_DIAG_FIX)
    {
        if (!s_last_reported_fix || (s_last_state != GNSS_DIAG_FIX))
        {
#if PURRGO_GNSS_LOG_PERIODIC
#if PURRGO_GNSS_NMEA_OFF
            PURRGO_LOG("GNSS: FIX after %lu s, sats used=%u (bytes=%lu, ubx frames=%lu)\r\n",
                       up_s, (unsigned)purrgo_gnss_ubx_status()->pvt_num_sv,
                       (unsigned long)s_diag.bytes, (unsigned long)s_diag.ubx_frames_ok);
#else
            PURRGO_LOG("GNSS: FIX after %lu s, sats used=%u in view=%u (bytes=%lu, nmea ok=%lu)\r\n",
                       up_s, (unsigned)s_diag.gga_sats_used,
                       (unsigned)gnss_diag_sats_in_view(&s_diag),
                       (unsigned long)s_diag.bytes, (unsigned long)s_diag.sentences_ok);
#endif
#endif
            s_last_reported_fix = true;
        }
        s_last_state = state;
        return;
    }
    s_last_reported_fix = false;

    switch (state)
    {
    case GNSS_DIAG_NO_DATA:
        PURRGO_LOGW("GNSS: NO DATA for %lu s (0 bytes; FE=%lu NE=%lu ORE=%lu). Check: module TX -> PA3, "
                   "module RX -> PA2, VCC/GND, module powered\r\n",
                   up_s, (unsigned long)s_diag.err_fe, (unsigned long)s_diag.err_ne,
                   (unsigned long)s_diag.err_ore);
        if ((++s_bad_reports >= MONITOR_BAD_REPORTS_FAULT) && !purrgo_hw_fault_active(PURRGO_HWF_GNSS))
        {
            purrgo_hw_fault(PURRGO_HWF_GNSS, "module sends no data for %lu s (check wiring and power)", up_s);
        }
        break;

    case GNSS_DIAG_NOT_NMEA:
    {
        const unsigned long pct = (s_diag.bytes == 0U) ? 0UL
            : (unsigned long)(((uint64_t)s_diag.printable * 100U) / s_diag.bytes);

        PURRGO_LOGW("GNSS: NO VALID NMEA/UBX after %lu s: bytes=%lu (+%lu) printable=%lu%% bad=%lu FE=%lu NE=%lu "
                   "ORE=%lu. Wrong baud (UART at %lu), noise or bad wiring\r\n",
                   up_s, (unsigned long)s_diag.bytes, (unsigned long)delta, pct,
                   (unsigned long)s_diag.sentences_bad, (unsigned long)s_diag.err_fe,
                   (unsigned long)s_diag.err_ne, (unsigned long)s_diag.err_ore,
                   (unsigned long)s_baud);
        if ((++s_bad_reports >= MONITOR_BAD_REPORTS_FAULT) && !purrgo_hw_fault_active(PURRGO_HWF_GNSS))
        {
            purrgo_hw_fault(PURRGO_HWF_GNSS, "module data is not valid NMEA/UBX (%lu s): wrong baud, noise or wiring", up_s);
        }
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
            PURRGO_LOGW("GNSS: DATA STOPPED: no bytes in the last report period (total %lu bytes)\r\n",
                       (unsigned long)s_diag.bytes);
        }
#if PURRGO_GNSS_LOG_PERIODIC
#if PURRGO_GNSS_NMEA_OFF
        {
            const purrgo_gnss_ubx_status_t *ubx = purrgo_gnss_ubx_status();

            PURRGO_LOGW("GNSS: UBX OK, no fix yet (%lu s): bytes=%lu (+%lu) frames=%lu nmea ok=%lu "
                       "PVT fixType=%u sv=%u validTime=%u FE=%lu NE=%lu ORE=%lu DMA=%lu\r\n",
                       up_s, (unsigned long)s_diag.bytes, (unsigned long)delta,
                       (unsigned long)s_diag.ubx_frames_ok, (unsigned long)s_diag.sentences_ok,
                       (unsigned)(ubx->pvt_valid ? ubx->pvt_fix_type : 0U), (unsigned)ubx->pvt_num_sv,
                       (unsigned)ubx->pvt_time_valid, (unsigned long)s_diag.err_fe,
                       (unsigned long)s_diag.err_ne, (unsigned long)s_diag.err_ore,
                       (unsigned long)s_diag.err_dma);
        }
#else
        PURRGO_LOGW("GNSS: NMEA OK, no fix yet (%lu s): bytes=%lu (+%lu) ok=%lu bad=%lu sentences (last period): %s"
                   "in view=%u used=%u fix quality=%u FE=%lu NE=%lu ORE=%lu DMA=%lu\r\n",
                   up_s, (unsigned long)s_diag.bytes, (unsigned long)delta,
                   (unsigned long)s_diag.sentences_ok, (unsigned long)s_diag.sentences_bad, seen,
                   (unsigned)gnss_diag_sats_in_view(&s_diag), (unsigned)s_diag.gga_sats_used,
                   (unsigned)s_diag.gga_quality, (unsigned long)s_diag.err_fe,
                   (unsigned long)s_diag.err_ne, (unsigned long)s_diag.err_ore,
                   (unsigned long)s_diag.err_dma);
#endif
#else
        (void)seen;
#endif
        break;
    }
    }

    s_last_state = state;
}

#endif /* PURRGO_GNSS_MONITOR */
