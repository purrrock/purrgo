#include "purrgo/gnss_duty.h"

#include "purrgo/gnss_io.h"
#include "purrgo/hardware_config.h"
#include "purrgo/logger.h"

#include <string.h>

/* Разность моментов с учётом переполнения 32-битного счётчика: true, если now достиг t. */
static bool reached(uint32_t now, uint32_t t)
{
    return (int32_t)(now - t) >= 0;
}

/* Сколько мс прошло с t (0, если t в будущем: метка могла быть взята чуть позже now). */
static uint32_t since(uint32_t now, uint32_t t)
{
    const int32_t diff = (int32_t)(now - t);

    return (diff > 0) ? (uint32_t)diff : 0U;
}

/* Сколько мс осталось до t (0, если срок наступил). */
static uint32_t until(uint32_t now, uint32_t t)
{
    const int32_t diff = (int32_t)(t - now);

    return (diff > 0) ? (uint32_t)diff : 0U;
}

void purrgo_gnss_duty_cfg_default(purrgo_gnss_duty_cfg_t *cfg)
{
    if (cfg == NULL) {
        return;
    }
    cfg->period_ms       = (uint32_t)PURRGO_GNSS_TRACKER_PERIOD_S * 1000U;
    cfg->fix_timeout_ms  = (uint32_t)PURRGO_GNSS_DUTY_FIX_TIMEOUT_S * 1000U;
    cfg->cold_timeout_ms = (uint32_t)PURRGO_GNSS_DUTY_COLD_TIMEOUT_S * 1000U;
    cfg->cold_age_ms     = (uint32_t)PURRGO_GNSS_DUTY_COLD_AGE_S * 1000U;
    cfg->hold_ms         = (uint32_t)PURRGO_GNSS_DUTY_HOLD_MS;
    cfg->accept_hacc_mm  = (uint32_t)PURRGO_GNSS_DUTY_ACCEPT_HACC_M * 1000U;
    cfg->min_pause_ms    = (uint32_t)PURRGO_GNSS_DUTY_MIN_PAUSE_S * 1000U;
    cfg->backoff_max_ms  = (uint32_t)PURRGO_GNSS_DUTY_BACKOFF_MAX_S * 1000U;
    cfg->slow_fix_ms     = (uint32_t)PURRGO_GNSS_DUTY_SLOW_FIX_S * 1000U;
    cfg->cont_retry_ms   = (uint32_t)PURRGO_GNSS_DUTY_CONT_RETRY_S * 1000U;
    cfg->cont_loss_ms    = (uint32_t)PURRGO_GNSS_DUTY_CONT_LOSS_S * 1000U;
    cfg->min_fix_type    = (uint8_t)PURRGO_GNSS_DUTY_MIN_FIX_TYPE;
    cfg->slow_count      = (uint8_t)PURRGO_GNSS_DUTY_SLOW_COUNT;
}

void purrgo_gnss_duty_init(purrgo_gnss_duty_t *d, const purrgo_gnss_duty_cfg_t *cfg,
                           const purrgo_gnss_duty_ops_t *ops, void *ctx)
{
    if (d == NULL) {
        return;
    }
    memset(d, 0, sizeof *d);
    if (cfg != NULL) {
        d->cfg = *cfg;
    } else {
        purrgo_gnss_duty_cfg_default(&d->cfg);
    }
    if (ops != NULL) {
        d->ops = *ops;
    }
    d->ctx = ctx;
    d->state = PURRGO_GNSS_DUTY_STOPPED;
}

static void do_power_on(purrgo_gnss_duty_t *d)
{
    if (d->ops.power_on != NULL) {
        d->ops.power_on(d->ctx);
    }
}

static void do_power_off(purrgo_gnss_duty_t *d)
{
    if (d->ops.power_off != NULL) {
        d->ops.power_off(d->ctx);
    }
}

/* Холодный старт: фикса ещё не было или он слишком стар, эфемериды устарели. */
static bool is_cold(const purrgo_gnss_duty_t *d, uint32_t now_ms)
{
    return !d->have_fix || (since(now_ms, d->last_fix_ms) > d->cfg.cold_age_ms);
}

static void start_search(purrgo_gnss_duty_t *d, uint32_t now_ms, const purrgo_gnss_duty_input_t *in)
{
    d->state = PURRGO_GNSS_DUTY_SEARCH;
    d->cycles++;
    d->t_on = now_ms;
    d->seq_seen = in->fix_seq;
    d->long_timeout = is_cold(d, now_ms);
    d->time_seen = false;
    PURRGO_LOGI("GNSS duty: on #%lu (%s start, timeout %lu s, misses %u)\r\n", (unsigned long)d->cycles,
                d->long_timeout ? "cold" : "hot",
                (unsigned long)((d->long_timeout ? d->cfg.cold_timeout_ms : d->cfg.fix_timeout_ms) / 1000U),
                (unsigned)d->misses);
    do_power_on(d);   /* может блокировать (перезапуск по EN): момент включения уже взят */
}

/* Пауза после промаха: период, затем удвоение до предела. */
static uint32_t backoff_pause_ms(const purrgo_gnss_duty_t *d)
{
    const uint32_t limit = (d->cfg.backoff_max_ms > d->cfg.period_ms) ? d->cfg.backoff_max_ms : d->cfg.period_ms;
    uint32_t pause = d->cfg.period_ms;

    for (uint16_t i = 1U; (i < d->misses) && (pause < limit); i++) {
        pause = (pause > (UINT32_MAX / 2U)) ? limit : (pause * 2U);
    }
    if (pause > limit) {
        pause = limit;
    }
    if (pause < d->cfg.min_pause_ms) {
        pause = d->cfg.min_pause_ms;
    }
    return pause;
}

static void go_off(purrgo_gnss_duty_t *d, uint32_t now_ms, uint32_t pause_ms)
{
    d->state = PURRGO_GNSS_DUTY_OFF;
    d->last_pause_ms = pause_ms;
    d->t_next_start = now_ms + pause_ms;
}

static void miss(purrgo_gnss_duty_t *d, uint32_t now_ms, const char *why)
{
    do_power_off(d);
    d->misses = (d->misses < UINT16_MAX) ? (uint16_t)(d->misses + 1U) : d->misses;
    d->misses_total++;
    d->slow_run = 0U;
    const uint32_t pause = backoff_pause_ms(d);

    PURRGO_LOGW("GNSS duty: no fix (%s), miss %u in a row, pause %lu s\r\n", why, (unsigned)d->misses,
                (unsigned long)(pause / 1000U));
    go_off(d, now_ms, pause);
}

static void enter_continuous(purrgo_gnss_duty_t *d, uint32_t now_ms)
{
    d->state = PURRGO_GNSS_DUTY_CONT;
    d->t_cont_start = now_ms;
    d->t_cont_fix = now_ms;
    d->slow_run = 0U;
    PURRGO_LOGI("GNSS duty: slow fixes, receiver stays on (PSMCT)\r\n");
    if (d->ops.enter_continuous != NULL) {
        d->ops.enter_continuous(d->ctx);
    }
}

static void accept(purrgo_gnss_duty_t *d, uint32_t now_ms)
{
    const uint32_t on_ms = since(now_ms, d->t_on);

    d->last_ttf_ms = on_ms;
    d->misses = 0U;
    d->have_fix = true;
    d->last_fix_ms = now_ms;
    d->slow_run = (on_ms > d->cfg.slow_fix_ms) ? (uint8_t)((d->slow_run < UINT8_MAX) ? d->slow_run + 1U : d->slow_run)
                                                : 0U;
    PURRGO_LOGI("GNSS duty: fix in %lu ms (slow %u of %u)\r\n", (unsigned long)on_ms, (unsigned)d->slow_run,
                (unsigned)d->cfg.slow_count);

    if ((d->cfg.slow_count != 0U) && (d->slow_run >= d->cfg.slow_count)) {
        enter_continuous(d, now_ms);
        return;
    }

    do_power_off(d);
    /* Период считается от начала включения: пауза = период минус время работы. */
    uint32_t pause = (d->cfg.period_ms > on_ms) ? (d->cfg.period_ms - on_ms) : 0U;

    if (pause < d->cfg.min_pause_ms) {
        pause = d->cfg.min_pause_ms;
    }
    go_off(d, now_ms, pause);
}

void purrgo_gnss_duty_start(purrgo_gnss_duty_t *d, uint32_t now_ms, const purrgo_gnss_duty_input_t *in,
                            bool recent_fix)
{
    purrgo_gnss_duty_input_t none;

    if (d == NULL) {
        return;
    }
    if (in == NULL) {
        memset(&none, 0, sizeof none);
        in = &none;
    }
    d->misses = 0U;
    d->slow_run = 0U;
    d->have_fix = recent_fix;
    d->last_fix_ms = now_ms;
    d->last_pause_ms = d->cfg.period_ms;
    start_search(d, now_ms, in);
}

void purrgo_gnss_duty_stop(purrgo_gnss_duty_t *d)
{
    if (d != NULL) {
        d->state = PURRGO_GNSS_DUTY_STOPPED;
    }
}

/*
 * Фикс принят к рассмотрению: решение действительно, а тип не хуже минимального. Тип 0 - "не передан" (NMEA
 * без GSA): такому решению доверяем по признаку valid, иначе фикс по одним RMC/GGA не принимался бы никогда.
 * Тип 1 - приёмник сам говорит "нет фикса".
 */
static bool fix_ok(const purrgo_gnss_duty_t *d, const purrgo_gnss_duty_input_t *in)
{
    return in->valid && ((in->fix_type == 0U) || (in->fix_type >= d->cfg.min_fix_type));
}

static bool good_enough(const purrgo_gnss_duty_t *d, const purrgo_gnss_duty_input_t *in)
{
    return (d->cfg.hold_ms == 0U) || (in->acc_valid && (in->hacc_mm <= d->cfg.accept_hacc_mm));
}

static void tick_search(purrgo_gnss_duty_t *d, uint32_t now_ms, const purrgo_gnss_duty_input_t *in)
{
    const bool epoch = (in->fix_seq != d->seq_seen);

    if (epoch) {
        d->seq_seen = in->fix_seq;
        if (in->time_valid) {
            d->time_seen = true;
        } else if (!d->time_seen && !d->long_timeout) {
            /* Приёмник не знает времени (RTC сброшен, BBR пуст): это холодный старт, нужен длинный таймаут. */
            d->long_timeout = true;
            PURRGO_LOGI("GNSS duty: receiver does not know the time, cold start, timeout %lu s\r\n",
                        (unsigned long)(d->cfg.cold_timeout_ms / 1000U));
        }
        if (fix_ok(d, in)) {
            d->t_fix = now_ms;
            if (good_enough(d, in)) {
                accept(d, now_ms);
                return;
            }
            d->state = PURRGO_GNSS_DUTY_HOLD;
            return;
        }
    }

    const uint32_t limit = d->long_timeout ? d->cfg.cold_timeout_ms : d->cfg.fix_timeout_ms;

    if (since(now_ms, d->t_on) >= limit) {
        miss(d, now_ms, d->long_timeout ? "cold timeout" : "timeout");
    }
}

static void tick_hold(purrgo_gnss_duty_t *d, uint32_t now_ms, const purrgo_gnss_duty_input_t *in)
{
    if (in->fix_seq != d->seq_seen) {
        d->seq_seen = in->fix_seq;
        if (fix_ok(d, in) && good_enough(d, in)) {
            accept(d, now_ms);
            return;
        }
    }
    /* Фикс уже получен: по истечении ожидания принимается любой (даже если точность не улучшилась). */
    if (since(now_ms, d->t_fix) >= d->cfg.hold_ms) {
        accept(d, now_ms);
    }
}

static void tick_cont(purrgo_gnss_duty_t *d, uint32_t now_ms, const purrgo_gnss_duty_input_t *in)
{
    if (in->fix_seq != d->seq_seen) {
        d->seq_seen = in->fix_seq;
        if (fix_ok(d, in)) {
            d->t_cont_fix = now_ms;
            d->have_fix = true;
            d->last_fix_ms = now_ms;
            d->misses = 0U;
        }
    }
    if (since(now_ms, d->t_cont_fix) >= d->cfg.cont_loss_ms) {
        miss(d, now_ms, "lost in continuous mode");
        return;
    }
    if (since(now_ms, d->t_cont_start) >= d->cfg.cont_retry_ms) {
        PURRGO_LOGI("GNSS duty: trying EN cycle again\r\n");
        do_power_off(d);
        d->slow_run = 0U;
        go_off(d, now_ms, d->cfg.min_pause_ms);
    }
}

void purrgo_gnss_duty_tick(purrgo_gnss_duty_t *d, uint32_t now_ms, const purrgo_gnss_duty_input_t *in)
{
    purrgo_gnss_duty_input_t none;

    if (d == NULL) {
        return;
    }
    if (in == NULL) {
        memset(&none, 0, sizeof none);
        in = &none;
    }

    switch (d->state) {
    case PURRGO_GNSS_DUTY_OFF:
        if (reached(now_ms, d->t_next_start)) {
            start_search(d, now_ms, in);
        }
        break;
    case PURRGO_GNSS_DUTY_SEARCH:
        tick_search(d, now_ms, in);
        break;
    case PURRGO_GNSS_DUTY_HOLD:
        tick_hold(d, now_ms, in);
        break;
    case PURRGO_GNSS_DUTY_CONT:
        tick_cont(d, now_ms, in);
        break;
    case PURRGO_GNSS_DUTY_STOPPED:
    default:
        break;
    }
}

static uint32_t min_u32(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

uint32_t purrgo_gnss_duty_wait_ms(const purrgo_gnss_duty_t *d, uint32_t now_ms)
{
    if (d == NULL) {
        return UINT32_MAX;
    }
    switch (d->state) {
    case PURRGO_GNSS_DUTY_OFF:
        return until(now_ms, d->t_next_start);
    case PURRGO_GNSS_DUTY_SEARCH: {
        const uint32_t limit = d->long_timeout ? d->cfg.cold_timeout_ms : d->cfg.fix_timeout_ms;

        return (limit > since(now_ms, d->t_on)) ? (limit - since(now_ms, d->t_on)) : 0U;
    }
    case PURRGO_GNSS_DUTY_HOLD:
        return (d->cfg.hold_ms > since(now_ms, d->t_fix)) ? (d->cfg.hold_ms - since(now_ms, d->t_fix)) : 0U;
    case PURRGO_GNSS_DUTY_CONT: {
        const uint32_t loss = (d->cfg.cont_loss_ms > since(now_ms, d->t_cont_fix))
                                  ? (d->cfg.cont_loss_ms - since(now_ms, d->t_cont_fix)) : 0U;
        const uint32_t retry = (d->cfg.cont_retry_ms > since(now_ms, d->t_cont_start))
                                   ? (d->cfg.cont_retry_ms - since(now_ms, d->t_cont_start)) : 0U;

        return min_u32(loss, retry);
    }
    case PURRGO_GNSS_DUTY_STOPPED:
    default:
        return UINT32_MAX;
    }
}

uint32_t purrgo_gnss_duty_fix_timeout_ms(const purrgo_gnss_duty_t *d)
{
    const uint32_t margin_ms = 10000U;

    if (d == NULL) {
        return margin_ms;
    }
    if (d->state == PURRGO_GNSS_DUTY_CONT) {
        return d->cfg.cont_loss_ms + margin_ms;
    }
    const uint32_t pause = (d->last_pause_ms != 0U) ? d->last_pause_ms : d->cfg.period_ms;

    return pause + (d->long_timeout ? d->cfg.cold_timeout_ms : d->cfg.fix_timeout_ms) + margin_ms;
}

/* ------------------------------------------------------------------------------------------------------------- */
/* Трекер платформы                                                                                              */
/* ------------------------------------------------------------------------------------------------------------- */

#if PURRGO_GNSS_TRACKER_EN_ACTIVE

static purrgo_gnss_duty_t s_tracker;
static bool s_tracker_active;

static void tracker_power_on(void *ctx)
{
    (void)ctx;
    purrgo_gnss_restart_profile(PURRGO_GNSS_PROFILE_TRACKER_EN);
}

static void tracker_power_off(void *ctx)
{
    (void)ctx;
    purrgo_gnss_power_off();
}

static void tracker_enter_continuous(void *ctx)
{
    (void)ctx;
    purrgo_gnss_set_profile(PURRGO_GNSS_PROFILE_TRACKER_CT);
}

void purrgo_gnss_tracker_start(uint32_t now_ms, const purrgo_gnss_duty_input_t *in, bool recent_fix)
{
    static const purrgo_gnss_duty_ops_t ops = { tracker_power_on, tracker_power_off, tracker_enter_continuous };
    purrgo_gnss_duty_cfg_t cfg;

    purrgo_gnss_duty_cfg_default(&cfg);
    purrgo_gnss_duty_init(&s_tracker, &cfg, &ops, NULL);
    s_tracker_active = true;
    purrgo_gnss_duty_start(&s_tracker, now_ms, in, recent_fix);
}

void purrgo_gnss_tracker_stop(void)
{
    if (!s_tracker_active) {
        return;
    }
    s_tracker_active = false;
    purrgo_gnss_duty_stop(&s_tracker);
    /* Навигации нужны 115200 бод, PSMCT и полный набор сообщений: приёмник включается заново с профилем навигации. */
    purrgo_gnss_restart_profile(PURRGO_GNSS_PROFILE_NAV);
}

void purrgo_gnss_tracker_tick(uint32_t now_ms, const purrgo_gnss_duty_input_t *in)
{
    if (s_tracker_active) {
        purrgo_gnss_duty_tick(&s_tracker, now_ms, in);
    }
}

uint32_t purrgo_gnss_tracker_wait_ms(uint32_t now_ms)
{
    return s_tracker_active ? purrgo_gnss_duty_wait_ms(&s_tracker, now_ms) : UINT32_MAX;
}

uint32_t purrgo_gnss_tracker_fix_timeout_ms(void)
{
    return purrgo_gnss_duty_fix_timeout_ms(&s_tracker);
}

bool purrgo_gnss_tracker_active(void)
{
    return s_tracker_active;
}

#else /* !PURRGO_GNSS_TRACKER_EN_ACTIVE */

void purrgo_gnss_tracker_start(uint32_t now_ms, const purrgo_gnss_duty_input_t *in, bool recent_fix)
{
    (void)now_ms;
    (void)in;
    (void)recent_fix;
}

void purrgo_gnss_tracker_stop(void)
{
}

void purrgo_gnss_tracker_tick(uint32_t now_ms, const purrgo_gnss_duty_input_t *in)
{
    (void)now_ms;
    (void)in;
}

uint32_t purrgo_gnss_tracker_wait_ms(uint32_t now_ms)
{
    (void)now_ms;
    return UINT32_MAX;
}

uint32_t purrgo_gnss_tracker_fix_timeout_ms(void)
{
    return 5000U;   /* вне цикла по EN срок задаёт app_fsm.c (PURRGO_GNSS_FIX_TIMEOUT_MS и т. д.) */
}

bool purrgo_gnss_tracker_active(void)
{
    return false;
}

#endif /* PURRGO_GNSS_TRACKER_EN_ACTIVE */
