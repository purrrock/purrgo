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
/* Отличается ли снимок настолько, чтобы изменилась картинка на экране. */
static bool info_differs(const purrgo_trip_info_t* a, const purrgo_trip_info_t* b) {
    return a->dist_valid != b->dist_valid ||
           (a->dist_m / 10U) != (b->dist_m / 10U) ||     /* на экране километры с двумя знаками */
           a->acc_valid != b->acc_valid ||
           a->hacc_dm != b->hacc_dm || a->vacc_dm != b->vacc_dm ||
           a->sig_valid != b->sig_valid ||
           a->jam_state != b->jam_state || a->spoof_state != b->spoof_state;
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
