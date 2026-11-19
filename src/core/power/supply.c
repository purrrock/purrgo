#include "purrgo/supply.h"
#include "purrgo/battery.h"
#include "purrgo/config.h"
#include "purrgo/logger.h"
#include "purrgo/sound.h"
#include "purrgo/track_logger.h"

static struct {
    bool     blocked;
    uint8_t  reason;       /* purrgo_supply_reason_t: что держит блокировку (0, пока не заблокировано) */
    bool     pending;      /* идёт отсчёт: низкое (до блокировки) или нормальное (после) */
    uint32_t pending_ms;   /* момент начала отсчёта */
    bool     batt_low;     /* причина 2: напряжение батарей по АЦП (держится до смены источника) */
    uint8_t  batt_polls;   /* опросов подряд ниже порога */
    bool     batt_polled;
    uint32_t batt_poll_ms;
    purrgo_power_source_t batt_src;   /* источник питания, для которого накоплено состояние */
} s_supply;

/*
 * Причина 2: напряжение блока батарей ниже порога источника питания (опрос раз
 * в PURRGO_SUPPLY_BATT_POLL_MS = период замера АЦП, PURRGO_SUPPLY_BATT_CONFIRM_POLLS
 * подряд). После срабатывания
 * состояние держится, пока не сменят источник питания (меню CONFIG) или не
 * перезапустят устройство. Порога нет (0) у источников без снятой кривой разряда
 * и у питания 5 В.
 */
static bool battery_low(uint32_t now_ms)
{
    const purrgo_power_source_t src = app_config.power_source;

    if (s_supply.batt_polled && s_supply.batt_src != src) {
        /* Источник сменили в меню: накопленное состояние относится к прежнему. */
        s_supply.batt_low = false;
        s_supply.batt_polls = 0;
        s_supply.batt_polled = false;
    }
    s_supply.batt_src = src;

    const uint16_t stop_mv = purrgo_battery_stop_mv(src);
    if (stop_mv == 0u) {
        s_supply.batt_low = false;
        s_supply.batt_polls = 0;
        return false;
    }
    if (s_supply.batt_low) {
        return true;
    }
    if (s_supply.batt_polled &&
        (uint32_t)(now_ms - s_supply.batt_poll_ms) < PURRGO_SUPPLY_BATT_POLL_MS) {
        return false;
    }
    s_supply.batt_polled = true;
    s_supply.batt_poll_ms = now_ms;

    purrgo_battery_t bat;
    (void)purrgo_battery_read(&bat);
    if (bat.external || bat.millivolts == 0u || bat.millivolts >= stop_mv) {
        /* Питание 5 В, напряжение не измерено или в норме: по АЦП не судим. */
        s_supply.batt_polls = 0;
    } else if (++s_supply.batt_polls >= PURRGO_SUPPLY_BATT_CONFIRM_POLLS) {
        s_supply.batt_low = true;
    }
    return s_supply.batt_low;
}

bool purrgo_supply_storage_blocked(void)
{
    return s_supply.blocked;
}

uint8_t purrgo_supply_reason(void)
{
    return s_supply.blocked ? s_supply.reason : (uint8_t)PURRGO_SUPPLY_REASON_NONE;
}

const char* purrgo_supply_reason_text(bool compact)
{
    static const char* const k_full[4]    = { "", "VDD", "BATT", "BATT+VDD" };
    static const char* const k_compact[4] = { "", "VDD", "BAT",  "B+V" };
    const uint8_t r = purrgo_supply_reason() &
                      (uint8_t)(PURRGO_SUPPLY_REASON_VDD | PURRGO_SUPPLY_REASON_BATT);

    return compact ? k_compact[r] : k_full[r];
}

void purrgo_supply_reset(void)
{
    s_supply.blocked = false;
    s_supply.reason = PURRGO_SUPPLY_REASON_NONE;
    s_supply.pending = false;
    s_supply.pending_ms = 0;
    s_supply.batt_low = false;
    s_supply.batt_polls = 0;
    s_supply.batt_polled = false;
    s_supply.batt_poll_ms = 0;
    s_supply.batt_src = app_config.power_source;
}

static void storage_stop(void)
{
    /*
     * Закрываем трек: футер GPX и f_close - последняя запись на карту. RAM-трек
     * для экрана сохраняется. Карту не размонтируем и файлы карты местности не
     * закрываем: чтение должно работать. Остальные записи (PURRGO.CFG, файлы
     * точек) отклоняет сам вызывающий код по purrgo_supply_storage_blocked().
     */
    purrgo_logger_suspend();
}

bool purrgo_supply_tick(uint32_t now_ms)
{
    const bool vdd_low = purrgo_platform_supply_low();
    /* Опрос батарей идёт всегда (не в short-circuit с PVD): причина должна быть известна точно. */
    const bool batt_low = battery_low(now_ms);
    const bool low = vdd_low || batt_low;
    const uint8_t cause = (uint8_t)((vdd_low ? PURRGO_SUPPLY_REASON_VDD : 0u) |
                                    (batt_low ? PURRGO_SUPPLY_REASON_BATT : 0u));
    bool changed = false;

    /* Блокировка уже держится, а причины сменились (добавилась или ушла одна из двух). */
    if (s_supply.blocked && low && cause != s_supply.reason) {
        s_supply.reason = cause;
        changed = true;
        PURRGO_LOGW("Supply low reason changed: %s\r\n", purrgo_supply_reason_text(false));
    }

    /* Состояние, противоположное текущему, должно продержаться заданное время. */
    if (low == s_supply.blocked) {
        s_supply.pending = false;
        return changed;
    }

    if (!s_supply.pending) {
        s_supply.pending = true;
        s_supply.pending_ms = now_ms;
    }

    const uint32_t need_ms = low ? PURRGO_SUPPLY_LOW_CONFIRM_MS : PURRGO_SUPPLY_RECOVER_MS;
    if ((uint32_t)(now_ms - s_supply.pending_ms) < need_ms) {
        return changed;
    }

    s_supply.pending = false;
    s_supply.blocked = low;
    if (low) {
        s_supply.reason = cause;
        /* LOGE: единственное сообщение о разряде в UART, должно проходить и в
         * релизной сборке (уровень 1). */
        PURRGO_LOGE("CRITICAL: battery low (%s), track recording stopped, SD writes stopped\r\n",
                    purrgo_supply_reason_text(false));
        storage_stop();
        /* Остановка записи - единственное событие, после которого теряется трек, а экран
         * в кармане не виден: длинный громкий сигнал (один раз, при переходе в блокировку). */
        purrgo_sound_play(PURRGO_SOUND_ERROR);
    } else {
        s_supply.reason = PURRGO_SUPPLY_REASON_NONE;
        PURRGO_LOGW("Supply recovered: SD writes allowed\r\n");
    }
    return true;
}
