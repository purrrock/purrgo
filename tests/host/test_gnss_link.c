/*
 * Host-тест установления связи с GNSS (stm32/common/gnss_link): поиск скорости
 * UART, смена скорости модуля и откат. Модуль и UART моделируются: модуль раз в
 * секунду выдаёт предложение NMEA на своей скорости; если скорость UART другая,
 * в автомат приходит мусор (как при неверной скорости).
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "gnss_link.h"
#include "test_common.h"

#define DEF_BAUD 38400U
#define TGT_BAUD 115200U
#define STEP_MS  10U

typedef struct
{
    /* Модель модуля. */
    uint32_t module_baud;     /* 0 - модуль молчит (нет питания / не подключён) */
    uint32_t boot_ms;         /* с этого момента выдаёт NMEA раз в секунду */
    int ignore_cmds;          /* сколько первых команд смены скорости модуль не применит */
    bool bad_checksum;        /* предложения с неверной контрольной суммой */
    bool ubx_only;            /* NMEA выключен настройкой: вместо предложений кадры UBX (PVT, DOP) */
    /* Модель платформы. */
    bool can_send;            /* send_baud_cmd возвращает true */
    bool fail_set_baud;       /* set_baud возвращает false */
    uint32_t uart_baud;
    int set_calls;
    int cmds;
    uint32_t last_cmd_baud;
    /* Журнал состояний. */
    gnss_link_state_t states[32];
    uint32_t state_baud[32];
    int n_states;
} sim_t;

static bool op_set_baud(uint32_t baud, void *ctx)
{
    sim_t *s = (sim_t *)ctx;

    s->set_calls++;
    if (s->fail_set_baud)
    {
        return false;
    }
    s->uart_baud = baud;
    return true;
}

static bool op_send_baud_cmd(uint32_t baud, void *ctx)
{
    sim_t *s = (sim_t *)ctx;

    if (!s->can_send)
    {
        return false;
    }
    s->cmds++;
    s->last_cmd_baud = baud;
    if (s->ignore_cmds > 0)
    {
        s->ignore_cmds--;
    }
    else if (s->module_baud != 0U)
    {
        s->module_baud = baud;
    }
    return true;
}

static void op_on_state(gnss_link_state_t st, uint32_t baud, void *ctx)
{
    sim_t *s = (sim_t *)ctx;

    if (s->n_states < 32)
    {
        s->states[s->n_states] = st;
        s->state_baud[s->n_states] = baud;
        s->n_states++;
    }
}

static void feed_sentence(gnss_link_t *l, const char *body, bool bad_checksum)
{
    uint8_t x = 0U;
    char tail[8];

    gnss_link_feed(l, (uint8_t)'$');
    for (const char *p = body; *p != '\0'; p++)
    {
        gnss_link_feed(l, (uint8_t)*p);
        x ^= (uint8_t)*p;
    }
    if (bad_checksum)
    {
        x ^= 0x55U;
    }
    (void)snprintf(tail, sizeof tail, "*%02X\r\n", (unsigned)x);
    for (const char *p = tail; *p != '\0'; p++)
    {
        gnss_link_feed(l, (uint8_t)*p);
    }
}

/* Кадр UBX с контрольной суммой Fletcher-8 (подмена контрольной суммы - bad_checksum). */
static void feed_ubx(gnss_link_t *l, uint8_t cls, uint8_t id, uint16_t len, bool bad_checksum)
{
    uint8_t f[128];
    uint8_t a = 0U, b = 0U;

    f[0] = 0xB5U; f[1] = 0x62U; f[2] = cls; f[3] = id;
    f[4] = (uint8_t)len; f[5] = (uint8_t)(len >> 8);
    for (uint16_t i = 0U; i < len; i++)
    {
        f[6U + i] = (uint8_t)(0x20U + i);       /* произвольные данные, в том числе '$' */
    }
    for (size_t i = 2U; i < 6U + (size_t)len; i++)
    {
        a = (uint8_t)(a + f[i]);
        b = (uint8_t)(b + a);
    }
    f[6U + len] = bad_checksum ? (uint8_t)(a ^ 0x55U) : a;
    f[7U + len] = b;
    for (size_t i = 0U; i < 8U + (size_t)len; i++)
    {
        gnss_link_feed(l, f[i]);
    }
}

/* Выдача модуля за один шаг моделирования. */
static void module_step(sim_t *s, gnss_link_t *l, uint32_t t)
{
    if ((s->module_baud == 0U) || (t < s->boot_ms) || (((t - s->boot_ms) % 1000U) != 0U))
    {
        return;
    }
    if ((s->uart_baud == s->module_baud) && s->ubx_only)
    {
        feed_ubx(l, 0x01U, 0x07U, 92U, s->bad_checksum);     /* NAV-PVT */
        feed_ubx(l, 0x01U, 0x04U, 18U, s->bad_checksum);     /* NAV-DOP */
    }
    else if (s->uart_baud == s->module_baud)
    {
        feed_sentence(l, "GNGGA,123519.00,5354.1234,N,02734.5678,E,1,08,0.9,200.0,M,25.0,M,,", s->bad_checksum);
        feed_sentence(l, "GNRMC,123519.00,A,5354.1234,N,02734.5678,E,0.0,0.0,090126,,,A", s->bad_checksum);
    }
    else
    {
        for (unsigned i = 0U; i < 60U; i++)    /* мусор при неверной скорости */
        {
            gnss_link_feed(l, (uint8_t)(0x80U | ((i * 37U) & 0x7FU)));
        }
    }
}

static bool terminal(gnss_link_state_t st)
{
    return (st == GNSS_LINK_READY) || (st == GNSS_LINK_DEGRADED) || (st == GNSS_LINK_FAILED);
}

static void make_link(gnss_link_t *l, sim_t *s, uint32_t def, uint32_t tgt)
{
    const gnss_link_cfg_t cfg = { def, tgt, 1300U, 100U, 3U };
    const gnss_link_ops_t ops = { op_set_baud, op_send_baud_cmd, op_on_state, s };

    gnss_link_init(l, &cfg, &ops);
}

/* Моделирование до терминального состояния или limit_ms; возвращает время, мс. */
static uint32_t run(sim_t *s, gnss_link_t *l, uint32_t start_ms, uint32_t limit_ms)
{
    uint32_t t = start_ms;

    gnss_link_start(l, t);
    while ((t - start_ms) < limit_ms && !terminal(gnss_link_state(l)))
    {
        t += STEP_MS;
        module_step(s, l, t);
        gnss_link_poll(l, t);
    }
    return t - start_ms;
}

static void test_fresh_module(void)
{
    sim_t s;
    gnss_link_t l;

    memset(&s, 0, sizeof s);
    s.module_baud = DEF_BAUD;
    s.boot_ms = 300U;
    s.can_send = true;
    make_link(&l, &s, DEF_BAUD, TGT_BAUD);

    const uint32_t t = run(&s, &l, 0U, 20000U);

    CHECK(gnss_link_state(&l) == GNSS_LINK_READY);
    CHECK(gnss_link_baud(&l) == TGT_BAUD);
    CHECK(s.uart_baud == TGT_BAUD);
    CHECK(s.module_baud == TGT_BAUD);
    CHECK(s.cmds == 1);
    CHECK(s.last_cmd_baud == TGT_BAUD);
    CHECK(t < 2500U);
    CHECK(!gnss_link_busy(&l));

    /* Последовательность: PROBE(38400) -> SWITCH -> VERIFY(115200) -> READY(115200). */
    CHECK(s.n_states == 4);
    CHECK(s.states[0] == GNSS_LINK_PROBE && s.state_baud[0] == DEF_BAUD);
    CHECK(s.states[1] == GNSS_LINK_SWITCH);
    CHECK(s.states[2] == GNSS_LINK_VERIFY && s.state_baud[2] == TGT_BAUD);
    CHECK(s.states[3] == GNSS_LINK_READY && s.state_baud[3] == TGT_BAUD);
}

static void test_module_already_at_target(void)
{
    sim_t s;
    gnss_link_t l;

    memset(&s, 0, sizeof s);
    s.module_baud = TGT_BAUD;     /* сброс МК без выключения модуля */
    s.boot_ms = 0U;
    s.can_send = true;
    make_link(&l, &s, DEF_BAUD, TGT_BAUD);

    const uint32_t t = run(&s, &l, 0U, 20000U);

    CHECK(gnss_link_state(&l) == GNSS_LINK_READY);
    CHECK(s.uart_baud == TGT_BAUD);
    CHECK(s.cmds == 0);           /* команда смены скорости не нужна */
    CHECK(t < 3500U);
}

static void test_late_boot(void)
{
    sim_t s;
    gnss_link_t l;

    memset(&s, 0, sizeof s);
    s.module_baud = DEF_BAUD;
    s.boot_ms = 2200U;            /* первый цикл поиска ничего не слышит */
    s.can_send = true;
    make_link(&l, &s, DEF_BAUD, TGT_BAUD);

    run(&s, &l, 0U, 20000U);

    CHECK(gnss_link_state(&l) == GNSS_LINK_READY);
    CHECK(s.uart_baud == TGT_BAUD);
    CHECK(s.cmds == 1);
}

static void test_lost_first_command(void)
{
    sim_t s;
    gnss_link_t l;

    memset(&s, 0, sizeof s);
    s.module_baud = DEF_BAUD;
    s.boot_ms = 300U;
    s.can_send = true;
    s.ignore_cmds = 1;            /* первая команда потеряна */
    make_link(&l, &s, DEF_BAUD, TGT_BAUD);

    run(&s, &l, 0U, 20000U);

    CHECK(gnss_link_state(&l) == GNSS_LINK_READY);
    CHECK(s.cmds == 2);
    CHECK(s.uart_baud == TGT_BAUD);
}

static void test_module_never_switches(void)
{
    sim_t s;
    gnss_link_t l;

    memset(&s, 0, sizeof s);
    s.module_baud = DEF_BAUD;
    s.boot_ms = 300U;
    s.can_send = true;
    s.ignore_cmds = 1000;
    make_link(&l, &s, DEF_BAUD, TGT_BAUD);

    run(&s, &l, 0U, 30000U);

    CHECK(gnss_link_state(&l) == GNSS_LINK_DEGRADED);
    CHECK(s.cmds == 3);                 /* attempts */
    CHECK(s.uart_baud == DEF_BAUD);     /* работа на заводской скорости */
    CHECK(gnss_link_baud(&l) == DEF_BAUD);
}

static void test_receiver_cannot_switch(void)
{
    sim_t s;
    gnss_link_t l;

    memset(&s, 0, sizeof s);
    s.module_baud = DEF_BAUD;
    s.boot_ms = 300U;
    s.can_send = false;                 /* модель приёмника без смены скорости */
    make_link(&l, &s, DEF_BAUD, TGT_BAUD);

    run(&s, &l, 0U, 20000U);

    CHECK(gnss_link_state(&l) == GNSS_LINK_DEGRADED);
    CHECK(s.cmds == 0);
    CHECK(s.uart_baud == DEF_BAUD);
}

static void test_silent_module(void)
{
    sim_t s;
    gnss_link_t l;

    memset(&s, 0, sizeof s);
    s.module_baud = 0U;                 /* модуль не отвечает */
    s.can_send = true;
    make_link(&l, &s, DEF_BAUD, TGT_BAUD);

    const uint32_t t = run(&s, &l, 0U, 30000U);

    CHECK(gnss_link_state(&l) == GNSS_LINK_FAILED);
    CHECK(s.uart_baud == DEF_BAUD);     /* UART оставлен на заводской скорости */
    CHECK(s.cmds == 0);                 /* команд не отправляли */
    CHECK(t >= 3U * 2U * 1300U);        /* 3 цикла по две скорости */
    CHECK(t <= 3U * 2U * 1300U + 100U);
}

static void test_bad_checksum_is_not_nmea(void)
{
    sim_t s;
    gnss_link_t l;

    memset(&s, 0, sizeof s);
    s.module_baud = DEF_BAUD;
    s.boot_ms = 0U;
    s.bad_checksum = true;              /* поток похож на NMEA, но контрольная сумма неверна */
    s.can_send = true;
    make_link(&l, &s, DEF_BAUD, TGT_BAUD);

    run(&s, &l, 0U, 30000U);

    CHECK(gnss_link_state(&l) == GNSS_LINK_FAILED);
    CHECK(s.cmds == 0);
}

static void test_same_baud(void)
{
    sim_t s;
    gnss_link_t l;

    memset(&s, 0, sizeof s);
    s.module_baud = DEF_BAUD;
    s.boot_ms = 300U;
    s.can_send = true;
    make_link(&l, &s, DEF_BAUD, DEF_BAUD);    /* скорость не меняется */

    run(&s, &l, 0U, 20000U);

    CHECK(gnss_link_state(&l) == GNSS_LINK_READY);
    CHECK(gnss_link_baud(&l) == DEF_BAUD);
    CHECK(s.cmds == 0);
    CHECK(s.set_calls == 1);

    /* Молчащий модуль при одинаковых скоростях: FAILED, без зацикливания. */
    memset(&s, 0, sizeof s);
    s.can_send = true;
    make_link(&l, &s, DEF_BAUD, DEF_BAUD);
    run(&s, &l, 0U, 30000U);
    CHECK(gnss_link_state(&l) == GNSS_LINK_FAILED);
}

static void test_set_baud_failure(void)
{
    sim_t s;
    gnss_link_t l;

    memset(&s, 0, sizeof s);
    s.module_baud = DEF_BAUD;
    s.can_send = true;
    s.fail_set_baud = true;
    make_link(&l, &s, DEF_BAUD, TGT_BAUD);

    run(&s, &l, 0U, 1000U);

    CHECK(gnss_link_state(&l) == GNSS_LINK_FAILED);
    CHECK(s.cmds == 0);
}

static void test_stop_and_restart(void)
{
    sim_t s;
    gnss_link_t l;

    memset(&s, 0, sizeof s);
    s.module_baud = DEF_BAUD;
    s.boot_ms = 300U;
    s.can_send = true;
    make_link(&l, &s, DEF_BAUD, TGT_BAUD);

    run(&s, &l, 0U, 20000U);
    CHECK(gnss_link_state(&l) == GNSS_LINK_READY);

    /* Выключение GNSS: IDLE, байты и poll игнорируются. */
    gnss_link_stop(&l);
    CHECK(gnss_link_state(&l) == GNSS_LINK_IDLE);
    gnss_link_feed(&l, 0x24U);
    gnss_link_poll(&l, 100000U);
    CHECK(gnss_link_state(&l) == GNSS_LINK_IDLE);
    CHECK(!gnss_link_busy(&l));

    /* Модуль после EN Low -> High снова на заводской скорости. */
    s.module_baud = DEF_BAUD;
    s.boot_ms = 100000U + 300U;
    s.cmds = 0;
    s.n_states = 0;
    run(&s, &l, 100000U, 20000U);
    CHECK(gnss_link_state(&l) == GNSS_LINK_READY);
    CHECK(s.cmds == 1);
    CHECK(s.uart_baud == TGT_BAUD);
}

/*
 * NMEA выключен настройкой (PURRGO_GNSS_UBX_NMEA = 0), модуль на рабочей скорости после сброса
 * одного МК: поток - только кадры UBX. Связь определяется по ним, команда смены скорости не нужна.
 */
static void test_ubx_only_module_at_target(void)
{
    sim_t s;
    gnss_link_t l;

    memset(&s, 0, sizeof s);
    s.module_baud = TGT_BAUD;
    s.ubx_only = true;
    s.can_send = true;
    make_link(&l, &s, DEF_BAUD, TGT_BAUD);

    const uint32_t t = run(&s, &l, 0U, 20000U);

    CHECK(gnss_link_state(&l) == GNSS_LINK_READY);
    CHECK(gnss_link_baud(&l) == TGT_BAUD && s.uart_baud == TGT_BAUD);
    CHECK(s.cmds == 0);
    CHECK(t < 4000U);
}

/* Кадры UBX с неверной контрольной суммой модулем не считаются: связи нет. */
static void test_ubx_bad_checksum_is_not_a_module(void)
{
    sim_t s;
    gnss_link_t l;

    memset(&s, 0, sizeof s);
    s.module_baud = TGT_BAUD;
    s.ubx_only = true;
    s.bad_checksum = true;
    s.can_send = true;
    make_link(&l, &s, DEF_BAUD, TGT_BAUD);

    (void)run(&s, &l, 0U, 60000U);
    CHECK(gnss_link_state(&l) == GNSS_LINK_FAILED);
    CHECK(s.cmds == 0);
}

/* Модуль на заводской скорости, но без NMEA: найден по UBX, затем обычная смена скорости. */
static void test_ubx_only_module_at_default(void)
{
    sim_t s;
    gnss_link_t l;

    memset(&s, 0, sizeof s);
    s.module_baud = DEF_BAUD;
    s.ubx_only = true;
    s.can_send = true;
    make_link(&l, &s, DEF_BAUD, TGT_BAUD);

    (void)run(&s, &l, 0U, 20000U);
    CHECK(gnss_link_state(&l) == GNSS_LINK_READY);
    CHECK(s.cmds == 1 && s.module_baud == TGT_BAUD && s.uart_baud == TGT_BAUD);
}

static void test_null_args(void)
{
    gnss_link_init(NULL, NULL, NULL);
    gnss_link_start(NULL, 0U);
    gnss_link_stop(NULL);
    gnss_link_feed(NULL, 0U);
    gnss_link_poll(NULL, 0U);
    CHECK(gnss_link_state(NULL) == GNSS_LINK_IDLE);
    CHECK(gnss_link_baud(NULL) == 0U);
    CHECK(!gnss_link_busy(NULL));
    CHECK(strcmp(gnss_link_state_name(GNSS_LINK_READY), "READY") == 0);
}

static void test_time_wrap(void)
{
    sim_t s;
    gnss_link_t l;

    memset(&s, 0, sizeof s);
    s.module_baud = DEF_BAUD;
    s.boot_ms = 0xFFFFFFFFU - 500U;           /* таймер HAL_GetTick() переполняется во время поиска */
    s.can_send = true;
    make_link(&l, &s, DEF_BAUD, TGT_BAUD);

    /* module_step сравнивает t с boot_ms без переполнения: сдвигаем начало к границе. */
    uint32_t t = 0xFFFFFFFFU - 1000U;
    gnss_link_start(&l, t);
    for (int i = 0; i < 1000 && !terminal(gnss_link_state(&l)); i++)
    {
        t += STEP_MS;
        /* выдача раз в секунду от начала */
        if (((t - (0xFFFFFFFFU - 1000U)) % 1000U) == 300U)
        {
            if (s.uart_baud == s.module_baud)
            {
                feed_sentence(&l, "GNGGA,1,2,3", false);
            }
        }
        gnss_link_poll(&l, t);
    }
    CHECK(gnss_link_state(&l) == GNSS_LINK_READY);
    CHECK(s.uart_baud == TGT_BAUD);
}

int main(void)
{
    test_fresh_module();
    test_module_already_at_target();
    test_late_boot();
    test_lost_first_command();
    test_module_never_switches();
    test_receiver_cannot_switch();
    test_silent_module();
    test_bad_checksum_is_not_nmea();
    test_same_baud();
    test_set_baud_failure();
    test_stop_and_restart();
    test_ubx_only_module_at_target();
    test_ubx_bad_checksum_is_not_a_module();
    test_ubx_only_module_at_default();
    test_null_args();
    test_time_wrap();
    return test_finish("test_gnss_link");
}
