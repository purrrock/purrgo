/*
 * Установление связи с GNSS-модулем: определение скорости UART и смена скорости.
 * См. gnss_link.h.
 */

#include "gnss_link.h"

#include <stddef.h>

#define PROBED_DEFAULT 0x01U
#define PROBED_TARGET  0x02U

static uint8_t probed_bit(const gnss_link_t *l, uint32_t baud)
{
    return (baud == l->cfg.default_baud) ? PROBED_DEFAULT : PROBED_TARGET;
}

static bool elapsed(const gnss_link_t *l, uint32_t now_ms, uint32_t ms)
{
    return (uint32_t)(now_ms - l->t0) >= ms;
}

static void notify(gnss_link_t *l)
{
    if (l->ops.on_state != NULL)
    {
        l->ops.on_state(l->state, l->baud, l->ops.ctx);
    }
}

/* Итоговое состояние. FAILED и DEGRADED оставляют UART на заводской скорости. */
static void finish(gnss_link_t *l, gnss_link_state_t state)
{
    if ((state != GNSS_LINK_READY) && (l->baud != l->cfg.default_baud))
    {
        (void)l->ops.set_baud(l->cfg.default_baud, l->ops.ctx);
        l->baud = l->cfg.default_baud;
    }
    l->state = state;
    notify(l);
}

/* Слушать на скорости baud. Ошибка перенастройки UART - сразу FAILED. */
static void enter_probe(gnss_link_t *l, uint32_t baud, uint32_t now_ms)
{
    if (!l->ops.set_baud(baud, l->ops.ctx))
    {
        l->state = GNSS_LINK_FAILED;
        notify(l);
        return;
    }
    l->baud = baud;
    l->probed |= probed_bit(l, baud);
    gnss_diag_init(&l->diag);
    l->t0 = now_ms;
    l->state = GNSS_LINK_PROBE;
    notify(l);
}

void gnss_link_init(gnss_link_t *l, const gnss_link_cfg_t *cfg, const gnss_link_ops_t *ops)
{
    if ((l == NULL) || (cfg == NULL) || (ops == NULL))
    {
        return;
    }
    l->cfg = *cfg;
    l->ops = *ops;
    l->state = GNSS_LINK_IDLE;
    l->baud = cfg->default_baud;
    l->t0 = 0U;
    l->probed = 0U;
    l->cycles = 0U;
    l->switch_tries = 0U;
    gnss_diag_init(&l->diag);
}

void gnss_link_start(gnss_link_t *l, uint32_t now_ms)
{
    if ((l == NULL) || (l->ops.set_baud == NULL) || (l->ops.send_baud_cmd == NULL))
    {
        return;
    }
    l->probed = 0U;
    l->cycles = 0U;
    l->switch_tries = 0U;
    enter_probe(l, l->cfg.default_baud, now_ms);
}

void gnss_link_stop(gnss_link_t *l)
{
    if (l != NULL)
    {
        l->state = GNSS_LINK_IDLE;
    }
}

void gnss_link_feed(gnss_link_t *l, uint8_t byte)
{
    if ((l == NULL) ||
        ((l->state != GNSS_LINK_PROBE) && (l->state != GNSS_LINK_VERIFY)))
    {
        return;
    }
    gnss_diag_feed(&l->diag, byte);
}

static void poll_probe(gnss_link_t *l, uint32_t now_ms)
{
    if (l->diag.sentences_ok != 0U)
    {
        if (l->baud == l->cfg.target_baud)
        {
            finish(l, GNSS_LINK_READY);
            return;
        }

        /* Модуль на заводской скорости, рабочая другая: сменить скорость. */
        if ((l->switch_tries >= l->cfg.attempts) ||
            !l->ops.send_baud_cmd(l->cfg.target_baud, l->ops.ctx))
        {
            finish(l, GNSS_LINK_DEGRADED);
            return;
        }
        l->switch_tries++;
        l->t0 = now_ms;
        l->state = GNSS_LINK_SWITCH;
        notify(l);
        return;
    }

    if (!elapsed(l, now_ms, l->cfg.probe_ms))
    {
        return;
    }

    /* Тишина на этой скорости: попробовать другую; обе без ответа - цикл закончен. */
    const uint32_t other = (l->baud == l->cfg.default_baud) ? l->cfg.target_baud
                                                              : l->cfg.default_baud;

    if ((l->probed & probed_bit(l, other)) == 0U)
    {
        enter_probe(l, other, now_ms);
        return;
    }

    l->cycles++;
    if (l->cycles >= l->cfg.attempts)
    {
        finish(l, GNSS_LINK_FAILED);
        return;
    }
    l->probed = 0U;
    enter_probe(l, l->cfg.default_baud, now_ms);
}

void gnss_link_poll(gnss_link_t *l, uint32_t now_ms)
{
    if (l == NULL)
    {
        return;
    }

    switch (l->state)
    {
    case GNSS_LINK_PROBE:
        poll_probe(l, now_ms);
        break;

    case GNSS_LINK_SWITCH:
        if (elapsed(l, now_ms, l->cfg.settle_ms))
        {
            if (!l->ops.set_baud(l->cfg.target_baud, l->ops.ctx))
            {
                l->state = GNSS_LINK_FAILED;
                notify(l);
                break;
            }
            l->baud = l->cfg.target_baud;
            gnss_diag_init(&l->diag);
            l->t0 = now_ms;
            l->state = GNSS_LINK_VERIFY;
            notify(l);
        }
        break;

    case GNSS_LINK_VERIFY:
        if (l->diag.sentences_ok != 0U)
        {
            finish(l, GNSS_LINK_READY);
        }
        else if (elapsed(l, now_ms, l->cfg.probe_ms))
        {
            /* Модуль не заговорил на новой скорости: назад на заводскую. */
            l->probed = 0U;
            enter_probe(l, l->cfg.default_baud, now_ms);
        }
        break;

    default:
        break;
    }
}

gnss_link_state_t gnss_link_state(const gnss_link_t *l)
{
    return (l != NULL) ? l->state : GNSS_LINK_IDLE;
}

uint32_t gnss_link_baud(const gnss_link_t *l)
{
    return (l != NULL) ? l->baud : 0U;
}

bool gnss_link_busy(const gnss_link_t *l)
{
    return (l != NULL) &&
           ((l->state == GNSS_LINK_PROBE) || (l->state == GNSS_LINK_SWITCH) ||
            (l->state == GNSS_LINK_VERIFY));
}

const char *gnss_link_state_name(gnss_link_state_t state)
{
    switch (state)
    {
    case GNSS_LINK_IDLE:     return "IDLE";
    case GNSS_LINK_PROBE:    return "PROBE";
    case GNSS_LINK_SWITCH:   return "SWITCH";
    case GNSS_LINK_VERIFY:   return "VERIFY";
    case GNSS_LINK_READY:    return "READY";
    case GNSS_LINK_DEGRADED: return "DEGRADED";
    case GNSS_LINK_FAILED:   return "FAILED";
    default:                 return "?";
    }
}
