#include "purrgo/supply.h"
#include "purrgo/battery.h"
#include "purrgo/logger.h"
#include "purrgo/map.h"
#include "purrgo/track_logger.h"

static struct {
    bool     blocked;
    bool     pending;      /* идёт отсчёт: низкое (до блокировки) или нормальное (после) */
    uint32_t pending_ms;   /* момент начала отсчёта */
    bool     batt_low;     /* причина 2: напряжение батарей по АЦП (с гистерезисом) */
    bool     batt_polled;
    uint32_t batt_poll_ms;
} s_supply;

static uint16_t s_batt_stop_mv = (uint16_t)PURRGO_SUPPLY_BATT_STOP_MV;

void purrgo_supply_set_battery_stop_mv(uint16_t mv)
{
    s_batt_stop_mv = mv;
    s_supply.batt_low = false;
    s_supply.batt_polled = false;
}

/* Причина 2: напряжение блока батарей ниже порога (гистерезис, опрос раз в секунду). */
static bool battery_low(uint32_t now_ms)
{
    if (s_batt_stop_mv == 0u) {
        return false;
    }
    if (s_supply.batt_polled &&
        (uint32_t)(now_ms - s_supply.batt_poll_ms) < PURRGO_SUPPLY_BATT_POLL_MS) {
        return s_supply.batt_low;
    }
    s_supply.batt_polled = true;
    s_supply.batt_poll_ms = now_ms;

    purrgo_battery_t bat;
    (void)purrgo_battery_read(&bat);
    if (bat.external || bat.millivolts == 0u) {
        /* Питание 5 В или напряжение не измерено: по АЦП не судим. */
        s_supply.batt_low = false;
    } else if (bat.millivolts < s_batt_stop_mv) {
        s_supply.batt_low = true;
    } else if (bat.millivolts >= (uint32_t)s_batt_stop_mv + PURRGO_SUPPLY_BATT_HYST_MV) {
        s_supply.batt_low = false;
    }
    return s_supply.batt_low;
}

bool purrgo_supply_storage_blocked(void)
{
    return s_supply.blocked;
}

void purrgo_supply_reset(void)
{
    s_supply.blocked = false;
    s_supply.pending = false;
    s_supply.pending_ms = 0;
    s_supply.batt_low = false;
    s_supply.batt_polled = false;
    s_supply.batt_poll_ms = 0;
}

static void storage_stop(void)
{
    /*
     * Порядок важен: сначала трек (футер и закрытие файла требуют записи на
     * карту, она ещё смонтирована), затем файлы карты, затем размонтирование.
     * RAM-трек для экрана сохраняется.
     */
    purrgo_logger_suspend();
    purrgo_map_shutdown();
    purrgo_platform_storage_suspend();
}

bool purrgo_supply_tick(uint32_t now_ms)
{
    const bool vdd_low = purrgo_platform_supply_low();
    const bool low = vdd_low || battery_low(now_ms);

    /* Состояние, противоположное текущему, должно продержаться заданное время. */
    if (low == s_supply.blocked) {
        s_supply.pending = false;
        return false;
    }

    if (!s_supply.pending) {
        s_supply.pending = true;
        s_supply.pending_ms = now_ms;
    }

    const uint32_t need_ms = low ? PURRGO_SUPPLY_LOW_CONFIRM_MS : PURRGO_SUPPLY_RECOVER_MS;
    if ((uint32_t)(now_ms - s_supply.pending_ms) < need_ms) {
        return false;
    }

    s_supply.pending = false;
    s_supply.blocked = low;
    if (low) {
        PURRGO_LOGW("Supply low (%s): stopping SD writes\r\n", vdd_low ? "VDD" : "battery ADC");
        storage_stop();
    } else {
        PURRGO_LOGW("Supply recovered: SD writes allowed\r\n");
    }
    return true;
}
