/*
 * Подсказка приёмнику u-blox: время (UBX-MGA-INI-TIME_UTC) и позиция (UBX-MGA-INI-POS_LLH), если после
 * включения он не знает времени. Описание - purrgo/utc_hint.h; "память о времени" - utc_clock.c.
 *
 * Условие отправки: приёмник присылает UBX-NAV-PVT (жив), но validTime = 0 дольше
 * PURRGO_UTC_HINT_GRACE_MS. Приёмник с работающим RTC (горячий старт) время знает и подсказки не получает.
 * Подсказка уходит один раз за такой период; новый период начинается, когда приёмник замолчал
 * (перезапуск) дольше PURRGO_UTC_HINT_SILENCE_RESET_MS или сообщил время.
 */
#include "purrgo/utc_hint.h"

#include "purrgo/config.h"
#include "purrgo/gnss_io.h"
#include "purrgo/gnss_receiver.h"
#include "purrgo/gnss_ubx.h"
#include "purrgo/hardware_config.h"
#include "purrgo/logger.h"
#include "utc_hint_priv.h"

#if PURRGO_UTC_HINT_ENABLE && PURRGO_GNSS_UBX && (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_UBX)
#define HINT_ACTIVE 1
#else
#define HINT_ACTIVE 0
#endif

#if HINT_ACTIVE

/* Заявляемая точность подсказки внутри сеанса: время - 2 с плюс 1% прошедшего, позиция - 2 км плюс 30 м/с. */
#define SESSION_TIME_ACC_BASE_S   2U
#define SESSION_POS_ACC_BASE_M    2000U
#define SESSION_POS_SPEED_M_S     30U

static bool s_sent;            /* подсказка за этот период уже отправлена */
static bool s_waiting;         /* идёт отсчёт PURRGO_UTC_HINT_GRACE_MS */
static uint32_t s_wait_ms;
static bool s_silent;
static uint32_t s_silent_ms;

static uint32_t clamp_u32(uint32_t v, uint32_t max)
{
    return (v > max) ? max : v;
}

/* Время и точность подсказки: сеанс (точно) или PURRGO.CFG (грубо). false - подсказывать нечем. */
static bool pick_time(uint32_t now_ms, uint32_t *epoch, uint32_t *t_acc_s, bool *from_session)
{
    uint32_t since_s = 0U;

    if (purrgo_utc_hint_priv_session_time(now_ms, epoch, &since_s)) {
        *t_acc_s = SESSION_TIME_ACC_BASE_S + since_s / 100U;
        *from_session = true;
        return true;
    }
#if PURRGO_UTC_HINT_BOOT_TIME
    if (purrgo_utc_hint_floor() != 0U) {
        *epoch = purrgo_utc_hint_floor();
        *t_acc_s = PURRGO_UTC_HINT_BOOT_TACC_S;
        *from_session = false;
        return true;
    }
#endif
    return false;
}

static bool send_hint(uint32_t now_ms)
{
    uint8_t frame[32];
    uint32_t epoch = 0U;
    uint32_t t_acc_s = 0U;
    bool from_session = false;
    bool sent_any = false;

    if (pick_time(now_ms, &epoch, &t_acc_s, &from_session)) {
        const size_t n = purrgo_ubx_build_mga_ini_time_utc(frame, sizeof frame, epoch, t_acc_s);

        if (n == 0U || !purrgo_gnss_write(frame, (uint16_t)n)) {
            return false;   /* связь не готова: повтор на следующей секунде */
        }
        PURRGO_LOGI("GNSS: TX UBX-MGA-INI-TIME_UTC epoch=%lu tAcc=%lu s (%s)\r\n",
                    (unsigned long)epoch, (unsigned long)t_acc_s, from_session ? "session" : "saved");
        sent_any = true;
    }

    {
        int32_t lat = 0;
        int32_t lon = 0;
        int32_t alt_cm = 0;
        uint32_t acc_m = 0U;
        bool have = false;
        uint32_t since_s = 0U;

        if (purrgo_utc_hint_priv_position(now_ms, &lat, &lon, &alt_cm, &since_s)) {
            acc_m = clamp_u32(SESSION_POS_ACC_BASE_M + SESSION_POS_SPEED_M_S * clamp_u32(since_s, 100000U),
                              PURRGO_UTC_HINT_BOOT_POS_ACC_M);
            have = true;
        }
#if PURRGO_UTC_HINT_BOOT_POS
        else if (purrgo_utc_hint_floor() != 0U) {
            /* Центр карты записан прошивкой, которая ведёт и LAST_UTC: это не значение по умолчанию. */
            lat = app_config.last_lat_1e7;
            lon = app_config.last_lon_1e7;
            alt_cm = 0;
            acc_m = PURRGO_UTC_HINT_BOOT_POS_ACC_M;
            have = true;
        }
#endif
        if (have) {
            const size_t n = purrgo_ubx_build_mga_ini_pos_llh(frame, sizeof frame, lat, lon, alt_cm,
                                                              clamp_u32(acc_m, 40000000U) * 100U);

            if (n != 0U && purrgo_gnss_write(frame, (uint16_t)n)) {
                PURRGO_LOGI("GNSS: TX UBX-MGA-INI-POS_LLH lat=%ld lon=%ld acc=%lu m\r\n",
                            (long)lat, (long)lon, (unsigned long)acc_m);
                sent_any = true;
            }
        }
    }
    return sent_any;
}

void purrgo_utc_hint_reset(void)
{
    purrgo_utc_hint_clock_reset();
    s_sent = false;
    s_waiting = false;
    s_silent = false;
}

void purrgo_utc_hint_service(uint32_t now_ms)
{
    const purrgo_gnss_ubx_status_t *st = purrgo_gnss_ubx_status();
    const bool alive = st->pvt_valid && purrgo_gnss_ubx_fresh(st->pvt_ms, now_ms);

    purrgo_utc_hint_priv_touch(now_ms);

    if (!alive) {
        if (!s_silent) {
            s_silent = true;
            s_silent_ms = now_ms;
        } else if ((uint32_t)(now_ms - s_silent_ms) >= PURRGO_UTC_HINT_SILENCE_RESET_MS) {
            s_sent = false;
            s_waiting = false;
        }
        return;
    }
    s_silent = false;

    if (st->pvt_time_valid) {
        s_sent = false;      /* приёмник знает время: подсказка не нужна, период закончен */
        s_waiting = false;
        return;
    }
    if (s_sent) {
        return;
    }
    if (!s_waiting) {
        s_waiting = true;
        s_wait_ms = now_ms;
        return;
    }
    if ((uint32_t)(now_ms - s_wait_ms) >= PURRGO_UTC_HINT_GRACE_MS && send_hint(now_ms)) {
        s_sent = true;
    }
}

#else /* !HINT_ACTIVE */

void purrgo_utc_hint_reset(void)
{
    purrgo_utc_hint_clock_reset();
}

void purrgo_utc_hint_service(uint32_t now_ms)
{
    purrgo_utc_hint_priv_touch(now_ms);
}

#endif
