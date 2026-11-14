#include "purrgo/trip_computer.h"

#include "purrgo/hardware_config.h"

#include <string.h>

#if PURRGO_GNSS_UBX
#include "purrgo/gnss_ubx.h"
#include "purrgo/system_time.h"
#endif

static purrgo_trip_info_t s_info;
static bool s_changed;

void purrgo_trip_computer_init(void) {
    memset(&s_info, 0, sizeof s_info);
    s_changed = false;
}

#if PURRGO_GNSS_UBX
/* Процент с округлением к ближайшему кратному 5. */
static uint8_t percent_step5(uint32_t value, uint32_t full) {
    uint32_t pct = (value * 100U + full / 2U) / full;

    if (pct > 100U) {
        pct = 100U;
    }
    return (uint8_t)(((pct + 2U) / 5U) * 5U);
}

/* Отличается ли снимок настолько, чтобы изменилась картинка на экране. */
static bool info_differs(const purrgo_trip_info_t* a, const purrgo_trip_info_t* b) {
    return a->dist_valid != b->dist_valid ||
           (a->dist_m / 10U) != (b->dist_m / 10U) ||     /* на экране километры с двумя знаками */
           a->acc_valid != b->acc_valid ||
           a->hacc_dm != b->hacc_dm || a->vacc_dm != b->vacc_dm ||
           a->sig_valid != b->sig_valid ||
           a->jam_state != b->jam_state || a->spoof_state != b->spoof_state ||
           a->rf_valid != b->rf_valid ||
           a->cw_percent != b->cw_percent || a->agc_percent != b->agc_percent ||
           a->ant_status != b->ant_status ||
           a->cn0_valid != b->cn0_valid || a->cn0_dbhz != b->cn0_dbhz;
}
#endif

void purrgo_trip_computer_update(const purrgo_gnss_solution_t* current_fix) {
#if PURRGO_GNSS_UBX
    purrgo_trip_info_t n;
    const uint32_t now = purrgo_system_time_ms();
    const purrgo_gnss_ubx_status_t* st = purrgo_gnss_ubx_status();

    memset(&n, 0, sizeof n);

    n.dist_valid = purrgo_gnss_ubx_trip_distance(now, &n.dist_m);

    if (current_fix != NULL && current_fix->valid && current_fix->acc_valid) {
        n.acc_valid = true;
        n.hacc_dm = current_fix->hacc_mm / 100U;
        n.vacc_dm = current_fix->vacc_mm / 100U;
    }

    if (st->sec_valid && purrgo_gnss_ubx_fresh(st->sec_ms, now)) {
        n.sig_valid = true;                      /* SPG 5.30+: UBX-SEC-SIG */
        n.jam_state = st->jam_state;
        n.spoof_state = st->spoof_state;
    } else {
        /* SPG 5.10: SEC-SIG нет; глушение - из MON-RF, спуфинг - из NAV-STATUS. */
        const bool jam_fresh = st->rf_valid && purrgo_gnss_ubx_fresh(st->rf_ms, now);
        const bool spoof_fresh = st->status_valid && purrgo_gnss_ubx_fresh(st->status_ms, now);

        if (jam_fresh || spoof_fresh) {
            n.sig_valid = true;
            n.jam_state = jam_fresh ? st->rf_jam_state : 0U;
            n.spoof_state = spoof_fresh ? st->status_spoof_state : 0U;
        }
    }

    if (st->rf_valid && purrgo_gnss_ubx_fresh(st->rf_ms, now)) {
        n.rf_valid = true;
        n.cw_percent = percent_step5(st->cw_suppression, 255U);
        n.agc_percent = percent_step5(st->agc_cnt, 8191U);
        n.ant_status = st->ant_status;
    }

    /*
     * Средний C/N0 меняется на 1 дБ почти каждую секунду, а каждая смена показанного значения -
     * перерисовка E-Ink. Поэтому показанное значение сохраняется, пока новое не отойдёт от него
     * больше чем на 1 дБ.
     */
    if (st->sat_valid && purrgo_gnss_ubx_fresh(st->sat_ms, now) && st->sat_used != 0U &&
        st->sat_cno_avg != 0U) {
        n.cn0_valid = true;
        n.cn0_dbhz = st->sat_cno_avg;
        if (s_info.cn0_valid) {
            const int diff = (int)n.cn0_dbhz - (int)s_info.cn0_dbhz;

            if (diff >= -1 && diff <= 1) {
                n.cn0_dbhz = s_info.cn0_dbhz;
            }
        }
    }

    if (info_differs(&n, &s_info)) {
        s_changed = true;
    }
    s_info = n;
#else
    (void)current_fix;    /* без UBX снимок остаётся пустым */
#endif
}

bool purrgo_trip_computer_handle_button(purrgo_action_t button) {
    (void)button;
    return false; // Trip computer does not handle any specific buttons right now
}

const purrgo_trip_info_t* purrgo_trip_computer_info(void) {
    return &s_info;
}

bool purrgo_trip_computer_take_changed(void) {
    const bool changed = s_changed;

    s_changed = false;
    return changed;
}
