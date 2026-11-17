/*
 * Host-тест последовательности питания microSD (stm32/port/storage/sd_power.c):
 * порядок действий при включении и выключении, пауза без питания, режим без
 * ключа питания, переполнение счётчика миллисекунд.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "sd_power.h"
#include "test_common.h"

#define EV_MAX 32

typedef enum
{
    EV_RAIL_ON = 1,
    EV_RAIL_OFF,
    EV_PINS_PARKED,
    EV_PINS_IDLE,
    EV_DELAY
} ev_kind_t;

typedef struct
{
    ev_kind_t kind;
    uint32_t arg;       /* для EV_DELAY - длительность, мс */
    uint32_t at_ms;     /* время события */
} ev_t;

static ev_t g_ev[EV_MAX];
static unsigned g_ev_n;
static uint32_t g_now;
static bool g_rail;     /* текущее состояние ключа */

static void log_ev(ev_kind_t kind, uint32_t arg)
{
    if (g_ev_n < EV_MAX)
    {
        g_ev[g_ev_n].kind = kind;
        g_ev[g_ev_n].arg = arg;
        g_ev[g_ev_n].at_ms = g_now;
        g_ev_n++;
    }
}

static void fake_rail(bool on, void *ctx)
{
    (void)ctx;
    g_rail = on;
    log_ev(on ? EV_RAIL_ON : EV_RAIL_OFF, 0U);
}

static void fake_pins(sd_pins_t mode, void *ctx)
{
    (void)ctx;
    log_ev((mode == SD_PINS_PARKED) ? EV_PINS_PARKED : EV_PINS_IDLE, 0U);
}

static void fake_delay(uint32_t ms, void *ctx)
{
    (void)ctx;
    log_ev(EV_DELAY, ms);
    g_now += ms;
}

static uint32_t fake_tick(void *ctx)
{
    (void)ctx;
    return g_now;
}

static void reset(uint32_t start_ms)
{
    memset(g_ev, 0, sizeof g_ev);
    g_ev_n = 0U;
    g_now = start_ms;
    g_rail = true;      /* после сброса МК ключ мог остаться включённым */
}

static sd_power_port_t make_port(bool with_switch)
{
    sd_power_port_t port = {
        .rail = with_switch ? fake_rail : NULL,
        .pins = fake_pins,
        .delay_ms = fake_delay,
        .tick_ms = fake_tick,
        .off_min_ms = 250U,
        .ramp_ms = 10U,
        .ctx = NULL,
    };
    return port;
}

static unsigned count_kind(ev_kind_t kind)
{
    unsigned n = 0U;

    for (unsigned i = 0U; i < g_ev_n; i++)
    {
        n += (g_ev[i].kind == kind) ? 1U : 0U;
    }
    return n;
}

static int index_of(ev_kind_t kind, unsigned nth)
{
    for (unsigned i = 0U; i < g_ev_n; i++)
    {
        if ((g_ev[i].kind == kind) && (nth-- == 0U))
        {
            return (int)i;
        }
    }
    return -1;
}

static uint32_t total_delay(void)
{
    uint32_t sum = 0U;

    for (unsigned i = 0U; i < g_ev_n; i++)
    {
        sum += (g_ev[i].kind == EV_DELAY) ? g_ev[i].arg : 0U;
    }
    return sum;
}

static void test_params(void)
{
    sd_power_t p;
    sd_power_port_t port = make_port(true);

    CHECK(!sd_power_init(NULL, &port));
    CHECK(!sd_power_init(&p, NULL));

    port.tick_ms = NULL;
    CHECK(!sd_power_init(&p, &port));

    port = make_port(true);
    port.pins = NULL;
    CHECK(!sd_power_init(&p, &port));

    memset(&p, 0, sizeof p);
    CHECK(!sd_power_up(&p));
    sd_power_down(&p);    /* не должен падать на неинициализированном p */
    sd_power_down(NULL);
    CHECK(sd_power_state(NULL) == SD_POWER_OFF);
}

static void test_init_parks_and_switches_off(void)
{
    sd_power_t p;
    sd_power_port_t port = make_port(true);

    reset(1000U);
    CHECK(sd_power_init(&p, &port));

    CHECK(sd_power_state(&p) == SD_POWER_OFF);
    CHECK(sd_power_has_switch(&p));
    CHECK(!g_rail);
    /* Линии припаркованы раньше, чем выключен ключ. */
    CHECK(index_of(EV_PINS_PARKED, 0U) >= 0);
    CHECK(index_of(EV_PINS_PARKED, 0U) < index_of(EV_RAIL_OFF, 0U));
    CHECK(count_kind(EV_RAIL_ON) == 0U);
}

static void test_power_up_sequence(void)
{
    sd_power_t p;
    sd_power_port_t port = make_port(true);

    reset(1000U);
    CHECK(sd_power_init(&p, &port));
    g_ev_n = 0U;

    /* Сразу после init пауза без питания ещё не прошла: ждём все 250 мс. */
    CHECK(sd_power_up(&p));
    CHECK(sd_power_state(&p) == SD_POWER_ON);
    CHECK(g_rail);

    CHECK(g_ev_n == 4U);
    CHECK(g_ev[0].kind == EV_DELAY && g_ev[0].arg == 250U);
    CHECK(g_ev[1].kind == EV_RAIL_ON);
    CHECK(g_ev[2].kind == EV_DELAY && g_ev[2].arg == 10U);
    CHECK(g_ev[3].kind == EV_PINS_IDLE);
    CHECK(g_ev[3].at_ms == 1000U + 250U + 10U);
    CHECK(p.cycles == 1U);

    /* Повторное включение - ничего не делает. */
    g_ev_n = 0U;
    CHECK(sd_power_up(&p));
    CHECK(g_ev_n == 0U);
    CHECK(p.cycles == 1U);
}

static void test_power_down_sequence(void)
{
    sd_power_t p;
    sd_power_port_t port = make_port(true);

    reset(0U);
    CHECK(sd_power_init(&p, &port));
    CHECK(sd_power_up(&p));
    g_now += 5000U;
    g_ev_n = 0U;

    sd_power_down(&p);
    CHECK(sd_power_state(&p) == SD_POWER_OFF);
    CHECK(!g_rail);
    CHECK(g_ev_n == 2U);
    /* Сначала линии, потом ключ. */
    CHECK(g_ev[0].kind == EV_PINS_PARKED);
    CHECK(g_ev[1].kind == EV_RAIL_OFF);

    /* Повторное выключение - ничего не делает. */
    g_ev_n = 0U;
    sd_power_down(&p);
    CHECK(g_ev_n == 0U);
}

static void test_off_min_pause(void)
{
    sd_power_t p;
    sd_power_port_t port = make_port(true);

    reset(0U);
    CHECK(sd_power_init(&p, &port));
    CHECK(sd_power_up(&p));

    /* Выключили, через 100 мс включаем снова: недостающие 150 мс ждём. */
    g_now += 1000U;
    sd_power_down(&p);
    g_now += 100U;
    g_ev_n = 0U;
    CHECK(sd_power_up(&p));
    CHECK(g_ev[0].kind == EV_DELAY && g_ev[0].arg == 150U);
    CHECK(total_delay() == 150U + 10U);

    /* Выключили, подождали дольше паузы: ждать нечего, остаётся только ramp. */
    g_now += 1000U;
    sd_power_down(&p);
    g_now += 400U;
    g_ev_n = 0U;
    CHECK(sd_power_up(&p));
    CHECK(total_delay() == 10U);
    CHECK(g_ev[0].kind == EV_RAIL_ON);

    /* Включение ровно через off_min_ms: паузы нет. */
    g_now += 1000U;
    sd_power_down(&p);
    g_now += 250U;
    g_ev_n = 0U;
    CHECK(sd_power_up(&p));
    CHECK(total_delay() == 10U);
    CHECK(p.cycles == 4U);
}

static void test_tick_wraparound(void)
{
    sd_power_t p;
    sd_power_port_t port = make_port(true);

    reset(0xFFFFFFF0U);
    CHECK(sd_power_init(&p, &port));
    g_now += 100U;      /* перешагнули через 0: прошло 100 мс */
    g_ev_n = 0U;
    CHECK(sd_power_up(&p));
    CHECK(g_ev[0].kind == EV_DELAY && g_ev[0].arg == 150U);
}

static void test_no_switch(void)
{
    sd_power_t p;
    sd_power_port_t port = make_port(false);

    reset(0U);
    CHECK(sd_power_init(&p, &port));
    CHECK(!sd_power_has_switch(&p));
    CHECK(sd_power_state(&p) == SD_POWER_OFF);
    CHECK(count_kind(EV_RAIL_OFF) == 0U);

    g_ev_n = 0U;
    CHECK(sd_power_up(&p));
    /* Карта запитана всегда: ни пауз, ни ключа, только линии в «простой». */
    CHECK(g_ev_n == 1U);
    CHECK(g_ev[0].kind == EV_PINS_IDLE);
    CHECK(sd_power_state(&p) == SD_POWER_ON);

    g_ev_n = 0U;
    sd_power_down(&p);
    CHECK(g_ev_n == 1U);
    CHECK(g_ev[0].kind == EV_PINS_PARKED);

    /* Следующее включение - сразу, без паузы off_min_ms. */
    g_ev_n = 0U;
    CHECK(sd_power_up(&p));
    CHECK(total_delay() == 0U);
}

int main(void)
{
    test_params();
    test_init_parks_and_switches_off();
    test_power_up_sequence();
    test_power_down_sequence();
    test_off_min_pause();
    test_tick_wraparound();
    test_no_switch();
    return test_finish("test_sd_power");
}
