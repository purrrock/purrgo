#include "sd_power.h"

#include <stddef.h>
#include <string.h>

static bool port_valid(const sd_power_port_t *port)
{
    return (port != NULL) && (port->pins != NULL) &&
           (port->delay_ms != NULL) && (port->tick_ms != NULL);
}

bool sd_power_init(sd_power_t *p, const sd_power_port_t *port)
{
    if ((p == NULL) || !port_valid(port))
    {
        return false;
    }

    memset(p, 0, sizeof *p);
    p->port = *port;

    /* Линии - в безопасное состояние до того, как ключ будет выключен. */
    p->port.pins(SD_PINS_PARKED, p->port.ctx);
    if (p->port.rail != NULL)
    {
        p->port.rail(false, p->port.ctx);
    }

    p->state = SD_POWER_OFF;
    p->off_since_ms = p->port.tick_ms(p->port.ctx);
    return true;
}

bool sd_power_up(sd_power_t *p)
{
    if ((p == NULL) || !port_valid(&p->port))
    {
        return false;
    }
    if (p->state == SD_POWER_ON)
    {
        return true;
    }

    if (p->port.rail != NULL)
    {
        const uint32_t off_ms = (uint32_t)(p->port.tick_ms(p->port.ctx) - p->off_since_ms);

        if (off_ms < p->port.off_min_ms)
        {
            p->port.delay_ms(p->port.off_min_ms - off_ms, p->port.ctx);
        }

        p->port.rail(true, p->port.ctx);
        if (p->port.ramp_ms > 0U)
        {
            p->port.delay_ms(p->port.ramp_ms, p->port.ctx);
        }
    }

    p->port.pins(SD_PINS_IDLE, p->port.ctx);
    p->state = SD_POWER_ON;
    p->cycles++;
    return true;
}

void sd_power_down(sd_power_t *p)
{
    if ((p == NULL) || !port_valid(&p->port) || (p->state == SD_POWER_OFF))
    {
        return;
    }

    /*
     * Сначала линии: пока ключ ещё включён, SCK/MOSI уходят в Low, а CS и MISO в Hi-Z
     * (без ключа - CS = High). Если сначала выключить ключ, CS и MOSI на время перехода
     * остались бы в High и подали бы питание в обесточенную карту.
     */
    p->port.pins(SD_PINS_PARKED, p->port.ctx);
    if (p->port.rail != NULL)
    {
        p->port.rail(false, p->port.ctx);
        p->off_since_ms = p->port.tick_ms(p->port.ctx);
    }
    p->state = SD_POWER_OFF;
}

sd_power_state_t sd_power_state(const sd_power_t *p)
{
    return (p != NULL) ? p->state : SD_POWER_OFF;
}

bool sd_power_has_switch(const sd_power_t *p)
{
    return (p != NULL) && (p->port.rail != NULL);
}
