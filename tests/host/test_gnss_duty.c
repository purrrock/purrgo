/*
 * Политика цикла питания GNSS в трекере (src/core/gnss/gnss_duty.c, purrgo/gnss_duty.h):
 *   - включение, ожидание фикса, приём фикса и выключение, период между началами включений;
 *   - ожидание хорошей точности (HOLD) и приём любого фикса по истечении ожидания;
 *   - промахи: пауза равна периоду, затем удваивается до предела, успех возвращает период;
 *   - таймаут при промахах не растёт; длинный (холодный) таймаут - при первом включении, старом фиксе и когда
 *     приёмник не знает времени;
 *   - медленные фиксы подряд переводят приёмник в непрерывный режим; выход по потере фикса и по повтору;
 *   - устаревшее решение (fix_seq не растёт) фиксом не считается;
 *   - переполнение 32-битного счётчика времени;
 *   - срок действия фикса, ожидание главного цикла, остановка.
 * Модуль чистый: приёмник и часы заменены записывающими заглушками.
 */
#include <stdint.h>
#include <string.h>

#include "purrgo/gnss_duty.h"
#include "purrgo/hardware_config.h"
#include "test_common.h"

typedef struct {
    purrgo_gnss_duty_t d;
    uint32_t now;
    purrgo_gnss_duty_input_t in;
    int on, off, cont;
    bool powered;
    uint32_t t_on_last;     /* момент последнего включения */
    uint32_t t_off_last;    /* момент последнего выключения */
} sim_t;

static void op_on(void *c)   { sim_t *s = c; s->on++;  s->powered = true;  s->t_on_last = s->now; }
static void op_off(void *c)  { sim_t *s = c; s->off++; s->powered = false; s->t_off_last = s->now; }
static void op_cont(void *c) { sim_t *s = c; s->cont++; }

/* Параметры не зависят от значений по умолчанию в hardware_config.h. */
static purrgo_gnss_duty_cfg_t cfg_test(void)
{
    purrgo_gnss_duty_cfg_t c;

    memset(&c, 0, sizeof c);
    c.period_ms = 15000U;
    c.fix_timeout_ms = 25000U;
    c.cold_timeout_ms = 90000U;
    c.cold_age_ms = 7200000U;
    c.hold_ms = 3000U;
    c.accept_hacc_mm = 30000U;
    c.min_pause_ms = 1000U;
    c.backoff_max_ms = 300000U;
    c.slow_fix_ms = 8000U;
    c.cont_retry_ms = 600000U;
    c.cont_loss_ms = 60000U;
    c.min_fix_type = 3U;
    c.slow_count = 3U;
    return c;
}

static void sim_init(sim_t *s, purrgo_gnss_duty_cfg_t cfg, uint32_t t0)
{
    static const purrgo_gnss_duty_ops_t ops = { op_on, op_off, op_cont };

    memset(s, 0, sizeof *s);
    s->now = t0;
    purrgo_gnss_duty_init(&s->d, &cfg, &ops, s);
}

/* Новое решение приёмника (растёт fix_seq). */
static void epoch(sim_t *s, bool valid, uint8_t fix_type, bool time_valid, uint32_t hacc_mm)
{
    s->in.fix_seq++;
    s->in.valid = valid;
    s->in.fix_type = fix_type;
    s->in.time_valid = time_valid;
    s->in.acc_valid = (hacc_mm != 0xFFFFFFFFU);
    s->in.hacc_mm = hacc_mm;
}

/* Хорошее 3D-решение с точностью 5 м. */
static void good_epoch(sim_t *s) { epoch(s, true, 3U, true, 5000U); }

/* Главный цикл: такт каждые 100 мс. */
static void step(sim_t *s, uint32_t ms)
{
    for (uint32_t t = 0U; t < ms; t += 100U) {
        s->now += 100U;
        purrgo_gnss_duty_tick(&s->d, s->now, &s->in);
    }
}

/* Идти тактами, пока не выполнится условие или не пройдёт limit_ms. */
static void step_until_on(sim_t *s, int on_before, uint32_t limit_ms)
{
    for (uint32_t t = 0U; (t < limit_ms) && (s->on == on_before); t += 100U) {
        step(s, 100U);
    }
}

static void test_cfg_default_from_macros(void)
{
    purrgo_gnss_duty_cfg_t c;

    purrgo_gnss_duty_cfg_default(&c);
    CHECK(c.period_ms == (uint32_t)PURRGO_GNSS_TRACKER_PERIOD_S * 1000U);
    CHECK(c.fix_timeout_ms == (uint32_t)PURRGO_GNSS_DUTY_FIX_TIMEOUT_S * 1000U);
    CHECK(c.cold_timeout_ms == (uint32_t)PURRGO_GNSS_DUTY_COLD_TIMEOUT_S * 1000U);
    CHECK(c.accept_hacc_mm == (uint32_t)PURRGO_GNSS_DUTY_ACCEPT_HACC_M * 1000U);
    CHECK(c.backoff_max_ms == (uint32_t)PURRGO_GNSS_DUTY_BACKOFF_MAX_S * 1000U);
    CHECK(c.min_fix_type == PURRGO_GNSS_DUTY_MIN_FIX_TYPE);
    CHECK(c.slow_count == PURRGO_GNSS_DUTY_SLOW_COUNT);
    /* Холодный таймаут длиннее горячего, а горячий короче периода пауз: иначе цикл бессмыслен. */
    CHECK(c.cold_timeout_ms > c.fix_timeout_ms);
    CHECK(c.min_pause_ms < c.period_ms);
}

static void test_start_powers_on(void)
{
    sim_t s;

    sim_init(&s, cfg_test(), 5000U);
    CHECK(s.d.state == PURRGO_GNSS_DUTY_STOPPED);
    CHECK(purrgo_gnss_duty_wait_ms(&s.d, s.now) == UINT32_MAX);

    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    CHECK(s.d.state == PURRGO_GNSS_DUTY_SEARCH);
    CHECK(s.on == 1 && s.off == 0 && s.powered);
    CHECK(!s.d.long_timeout);                                       /* фикс был при входе: старт горячий */
    CHECK(purrgo_gnss_duty_wait_ms(&s.d, s.now) == 25000U);

    sim_t c;
    sim_init(&c, cfg_test(), 5000U);
    purrgo_gnss_duty_start(&c.d, c.now, &c.in, false);
    CHECK(c.d.long_timeout);                                        /* фикса не было: холодный старт */
    CHECK(purrgo_gnss_duty_wait_ms(&c.d, c.now) == 90000U);
}

static void test_fix_accepted_and_period(void)
{
    sim_t s;

    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    step(&s, 3000U);
    CHECK(s.off == 0);                                              /* фикса ещё нет */
    good_epoch(&s);
    step(&s, 100U);
    CHECK(s.off == 1 && !s.powered);                                /* точность хорошая: принят сразу, EN = Low */
    CHECK(s.d.state == PURRGO_GNSS_DUTY_OFF);
    CHECK(s.d.last_ttf_ms >= 3000U && s.d.last_ttf_ms <= 3200U);
    CHECK(s.d.misses == 0U);

    /* Следующее включение через период от начала предыдущего (пауза = период - время работы). */
    const uint32_t wait = purrgo_gnss_duty_wait_ms(&s.d, s.now);

    CHECK(wait >= 11700U && wait <= 12000U);
    step(&s, wait - 100U);
    CHECK(s.on == 1);
    step(&s, 100U);
    CHECK(s.on == 2 && s.powered);
    CHECK(s.t_on_last - 0U >= 14900U && s.t_on_last <= 15200U);      /* ~15 с от первого включения */
    CHECK(s.d.state == PURRGO_GNSS_DUTY_SEARCH);
}

static void test_pause_not_below_minimum(void)
{
    sim_t s;
    purrgo_gnss_duty_cfg_t c = cfg_test();

    /* Поиск длился дольше периода (но успешно): пауза всё равно не меньше минимальной. */
    sim_init(&s, c, 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    step(&s, 20000U);
    good_epoch(&s);
    step(&s, 100U);
    CHECK(s.off == 1);
    CHECK(purrgo_gnss_duty_wait_ms(&s.d, s.now) >= 900U && purrgo_gnss_duty_wait_ms(&s.d, s.now) <= 1000U);
}

static void test_stale_solution_is_ignored(void)
{
    sim_t s;

    /* От прошлого цикла в решении осталась валидная позиция, но новых решений приёмник ещё не выдал. */
    sim_init(&s, cfg_test(), 0U);
    good_epoch(&s);                                                 /* fix_seq = 1, valid */
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    step(&s, 10000U);
    CHECK(s.off == 0 && s.d.state == PURRGO_GNSS_DUTY_SEARCH);      /* старое решение фиксом не считается */
    good_epoch(&s);                                                 /* новое решение */
    step(&s, 100U);
    CHECK(s.off == 1);
}

static void test_fix_type_and_validity_required(void)
{
    sim_t s;

    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    epoch(&s, true, 2U, true, 5000U);                               /* 2D при минимуме 3D */
    step(&s, 100U);
    epoch(&s, false, 3U, true, 5000U);                              /* тип 3D, но gnssFixOK = 0 */
    step(&s, 100U);
    epoch(&s, true, 1U, true, 5000U);                               /* valid, но приёмник сам пишет "нет фикса" */
    step(&s, 100U);
    CHECK(s.off == 0 && s.d.state == PURRGO_GNSS_DUTY_SEARCH);

    sim_t u;

    /* Тип фикса не передан (NMEA без GSA, тип 0): по признаку valid фикс принимается. */
    sim_init(&u, cfg_test(), 0U);
    purrgo_gnss_duty_start(&u.d, u.now, &u.in, true);
    epoch(&u, true, 0U, true, 5000U);
    step(&u, 100U);
    CHECK(u.off == 1);

    purrgo_gnss_duty_cfg_t c = cfg_test();
    sim_t t;

    c.min_fix_type = 2U;                                            /* 2D разрешён настройкой */
    sim_init(&t, c, 0U);
    purrgo_gnss_duty_start(&t.d, t.now, &t.in, true);
    epoch(&t, true, 2U, true, 5000U);
    step(&t, 100U);
    CHECK(t.off == 1);
}

static void test_hold_waits_for_accuracy(void)
{
    sim_t s;

    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    step(&s, 2000U);
    epoch(&s, true, 3U, true, 80000U);                              /* фикс есть, точность 80 м */
    step(&s, 100U);
    CHECK(s.d.state == PURRGO_GNSS_DUTY_HOLD && s.off == 0);
    step(&s, 900U);
    epoch(&s, true, 3U, true, 12000U);                              /* точность 12 м: принять */
    step(&s, 100U);
    CHECK(s.off == 1 && s.d.state == PURRGO_GNSS_DUTY_OFF);
}

static void test_hold_gives_up_after_hold_ms(void)
{
    sim_t s;

    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    epoch(&s, true, 3U, true, 80000U);
    step(&s, 100U);
    CHECK(s.d.state == PURRGO_GNSS_DUTY_HOLD);
    CHECK(purrgo_gnss_duty_wait_ms(&s.d, s.now) <= 3000U);
    for (int i = 0; i < 25; i++) {                                  /* точность не улучшается, эпохи идут */
        step(&s, 100U);
        epoch(&s, true, 3U, true, 80000U);
    }
    CHECK(s.off == 0);                                              /* ещё не прошло 3 с */
    step(&s, 1000U);
    CHECK(s.off == 1);                                              /* ожидание кончилось: принят любой фикс */
    CHECK(s.d.misses == 0U);
}

static void test_hold_without_accuracy_info(void)
{
    sim_t s;

    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    epoch(&s, true, 3U, true, 0xFFFFFFFFU);                         /* точность неизвестна (NMEA без UBX) */
    step(&s, 100U);
    CHECK(s.d.state == PURRGO_GNSS_DUTY_HOLD);
    step(&s, 3100U);
    CHECK(s.off == 1);
}

static void test_hold_disabled(void)
{
    sim_t s;
    purrgo_gnss_duty_cfg_t c = cfg_test();

    c.hold_ms = 0U;
    sim_init(&s, c, 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    epoch(&s, true, 3U, true, 90000U);                              /* точность плохая, но ожидания нет */
    step(&s, 100U);
    CHECK(s.off == 1);
}

static void test_miss_backoff_ladder(void)
{
    sim_t s;
    const uint32_t expect_pause_s[] = { 15U, 30U, 60U, 120U, 240U, 300U, 300U };

    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);

    for (unsigned i = 0U; i < sizeof expect_pause_s / sizeof expect_pause_s[0]; i++) {
        const int on_before = s.on;
        const int off_before = s.off;

        step(&s, 25000U);                                           /* таймаут: фикса нет */
        CHECK(s.off == off_before + 1);
        CHECK(s.d.misses == (uint16_t)(i + 1U));
        const uint32_t wait = purrgo_gnss_duty_wait_ms(&s.d, s.now);

        CHECK(wait <= expect_pause_s[i] * 1000U && wait + 200U >= expect_pause_s[i] * 1000U);
        step_until_on(&s, on_before, 400000U);
        CHECK(s.on == on_before + 1);
    }
    CHECK(s.d.misses_total == 7U);
}

static void test_timeout_does_not_grow_after_misses(void)
{
    sim_t s;

    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    for (int i = 0; i < 3; i++) {
        const int on_before = s.on;

        step(&s, 25000U);
        step_until_on(&s, on_before, 400000U);
        CHECK(purrgo_gnss_duty_wait_ms(&s.d, s.now) <= 25000U);     /* таймаут прежний: горячий старт */
        CHECK(!s.d.long_timeout);
    }
}

static void test_success_restores_period(void)
{
    sim_t s;

    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    for (int i = 0; i < 3; i++) {                                   /* три промаха: пауза 60 с */
        const int on_before = s.on;

        step(&s, 25000U);
        step_until_on(&s, on_before, 400000U);
    }
    CHECK(s.d.misses == 3U);
    step(&s, 2000U);
    good_epoch(&s);
    step(&s, 100U);
    CHECK(s.d.misses == 0U && s.off == 4);
    const uint32_t wait = purrgo_gnss_duty_wait_ms(&s.d, s.now);

    CHECK(wait >= 12000U && wait <= 13100U);                        /* снова около периода (15 с - ~2,1 с) */
}

static void test_cold_when_time_unknown(void)
{
    sim_t s;

    /* Старт горячий (фикс при входе был), но приёмник сообщает, что времени не знает (BBR пуст). */
    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    CHECK(purrgo_gnss_duty_wait_ms(&s.d, s.now) == 25000U);
    epoch(&s, false, 1U, false, 0xFFFFFFFFU);
    step(&s, 100U);
    CHECK(s.d.long_timeout);
    CHECK(purrgo_gnss_duty_wait_ms(&s.d, s.now) > 80000U);          /* срок вырос до холодного (90 с) */
    step(&s, 30000U);
    CHECK(s.off == 0);                                              /* горячий срок прошёл, ждём дальше */
    step(&s, 60000U);
    CHECK(s.off == 1 && s.d.misses == 1U);                          /* холодный срок вышел без фикса */
}

static void test_time_known_keeps_hot_timeout(void)
{
    sim_t s;

    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    epoch(&s, false, 1U, true, 0xFFFFFFFFU);                        /* время известно, фикса ещё нет */
    step(&s, 100U);
    CHECK(!s.d.long_timeout);
    step(&s, 25000U);
    CHECK(s.off == 1);
}

static void test_time_known_later_does_not_shrink_cold_timeout(void)
{
    sim_t s;

    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, false);              /* холодный старт */
    epoch(&s, false, 1U, true, 0xFFFFFFFFU);
    step(&s, 100U);
    CHECK(s.d.long_timeout);                                        /* знание времени срок не сокращает */
}

static void test_cold_after_old_fix(void)
{
    sim_t s;
    purrgo_gnss_duty_cfg_t c = cfg_test();

    c.cold_age_ms = 60000U;                                         /* эфемериды "живут" минуту */
    sim_init(&s, c, 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    good_epoch(&s);
    step(&s, 100U);
    CHECK(s.off == 1);
    for (int i = 0; i < 2; i++) {                                   /* два промаха: 25 + 15 + 25 + 30 с */
        const int on_before = s.on;

        step_until_on(&s, on_before, 100000U);
        CHECK(!s.d.long_timeout || i > 0);
        step(&s, 25000U);
    }
    const int on_before = s.on;

    step_until_on(&s, on_before, 100000U);
    CHECK(s.d.long_timeout);                                        /* с последнего фикса прошло больше минуты */
}

static void test_slow_fixes_switch_to_continuous(void)
{
    sim_t s;

    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    for (int i = 0; i < 2; i++) {                                   /* два медленных фикса: выключаем как обычно */
        const int on_before = s.on;

        step(&s, 9000U);
        good_epoch(&s);
        step(&s, 100U);
        CHECK(s.off == i + 1 && s.cont == 0);
        step_until_on(&s, on_before, 60000U);
    }
    step(&s, 9000U);
    good_epoch(&s);
    step(&s, 100U);
    CHECK(s.cont == 1 && s.off == 2 && s.powered);                  /* третий: приёмник остаётся включённым */
    CHECK(s.d.state == PURRGO_GNSS_DUTY_CONT);
}

static void test_fast_fix_resets_slow_run(void)
{
    sim_t s;

    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    const int seq[] = { 9000, 9000, 3000, 9000, 9000 };           /* быстрый фикс обнуляет счёт медленных */

    for (unsigned i = 0U; i < sizeof seq / sizeof seq[0]; i++) {
        const int on_before = s.on;

        step(&s, (uint32_t)seq[i]);
        good_epoch(&s);
        step(&s, 100U);
        CHECK(s.cont == 0);
        step_until_on(&s, on_before, 60000U);
    }
    CHECK(s.d.slow_run == 0U || s.d.slow_run < 3U);
}

static void test_continuous_disabled(void)
{
    sim_t s;
    purrgo_gnss_duty_cfg_t c = cfg_test();

    c.slow_count = 0U;
    sim_init(&s, c, 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    for (int i = 0; i < 5; i++) {
        const int on_before = s.on;

        step(&s, 20000U);
        good_epoch(&s);
        step(&s, 100U);
        step_until_on(&s, on_before, 60000U);
    }
    CHECK(s.cont == 0 && s.d.state == PURRGO_GNSS_DUTY_SEARCH);
}

static sim_t *to_continuous(sim_t *s)
{
    sim_init(s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s->d, s->now, &s->in, true);
    for (int i = 0; i < 3; i++) {
        const int on_before = s->on;

        step(s, 9000U);
        good_epoch(s);
        step(s, 100U);
        if (s->d.state != PURRGO_GNSS_DUTY_CONT) {
            step_until_on(s, on_before, 60000U);
        }
    }
    return s;
}

static void test_continuous_keeps_running_while_fixes_arrive(void)
{
    sim_t s;

    to_continuous(&s);
    CHECK(s.d.state == PURRGO_GNSS_DUTY_CONT);
    const int off_before = s.off;

    for (int i = 0; i < 20; i++) {                                  /* 200 с: фикс каждые 10 с (PSMCT) */
        step(&s, 10000U);
        good_epoch(&s);
    }
    CHECK(s.d.state == PURRGO_GNSS_DUTY_CONT && s.off == off_before && s.powered);
    CHECK(purrgo_gnss_duty_fix_timeout_ms(&s.d) == 60000U + 10000U);
}

static void test_continuous_ends_on_loss(void)
{
    sim_t s;

    to_continuous(&s);
    CHECK(s.d.state == PURRGO_GNSS_DUTY_CONT);
    step(&s, 59000U);
    CHECK(s.d.state == PURRGO_GNSS_DUTY_CONT);
    step(&s, 2000U);                                                /* 60 с без фикса */
    CHECK(s.d.state == PURRGO_GNSS_DUTY_OFF && !s.powered);
    CHECK(s.d.misses == 1U);
    const uint32_t wait = purrgo_gnss_duty_wait_ms(&s.d, s.now);

    CHECK(wait <= 15000U && wait >= 14000U);                        /* как после промаха: пауза = период */
}

static void test_continuous_retries_cycle(void)
{
    sim_t s;

    to_continuous(&s);
    const uint32_t t0 = s.now;
    const int on_before = s.on;

    while ((s.now - t0 < 650000U) && (s.d.state == PURRGO_GNSS_DUTY_CONT)) {
        step(&s, 10000U);
        good_epoch(&s);
    }
    CHECK(s.d.state == PURRGO_GNSS_DUTY_OFF && !s.powered);         /* через 10 мин пробуем цикл заново */
    CHECK(s.now - t0 >= 590000U && s.now - t0 <= 620000U);
    CHECK(s.d.misses == 0U);                                        /* это не промах */
    step_until_on(&s, on_before, 5000U);
    CHECK(s.on == on_before + 1 && s.d.state == PURRGO_GNSS_DUTY_SEARCH);   /* включение через минимальную паузу */
}

static void test_time_wraparound(void)
{
    sim_t s;
    const uint32_t t0 = UINT32_MAX - 7000U;                         /* счётчик переполнится на седьмой секунде */

    sim_init(&s, cfg_test(), t0);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    step(&s, 12000U);                                               /* переполнение внутри ожидания фикса */
    CHECK(s.off == 0);
    good_epoch(&s);
    step(&s, 100U);
    CHECK(s.off == 1);
    const int on_before = s.on;
    const uint32_t wait = purrgo_gnss_duty_wait_ms(&s.d, s.now);

    CHECK(wait >= 2000U && wait <= 3100U);                          /* 15 с - 12,1 с */
    step_until_on(&s, on_before, 20000U);
    CHECK(s.on == on_before + 1);
    step(&s, 25000U);                                               /* и промах после переполнения */
    CHECK(s.d.misses == 1U);
}

static void test_fix_timeout_ms(void)
{
    sim_t s;

    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    CHECK(purrgo_gnss_duty_fix_timeout_ms(&s.d) == 15000U + 25000U + 10000U);
    step(&s, 25000U);                                               /* промах, пауза 15 с */
    CHECK(purrgo_gnss_duty_fix_timeout_ms(&s.d) == 15000U + 25000U + 10000U);
    for (int i = 0; i < 3; i++) {                                   /* пауза растёт до 120 с */
        const int on_before = s.on;

        step_until_on(&s, on_before, 400000U);
        step(&s, 25000U);
    }
    CHECK(purrgo_gnss_duty_fix_timeout_ms(&s.d) == 120000U + 25000U + 10000U);
    sim_t c;

    sim_init(&c, cfg_test(), 0U);
    purrgo_gnss_duty_start(&c.d, c.now, &c.in, false);              /* холодный старт: срок включает 90 с */
    CHECK(purrgo_gnss_duty_fix_timeout_ms(&c.d) == 15000U + 90000U + 10000U);
}

static void test_stop(void)
{
    sim_t s;

    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, &s.in, true);
    purrgo_gnss_duty_stop(&s.d);
    CHECK(s.d.state == PURRGO_GNSS_DUTY_STOPPED);
    CHECK(purrgo_gnss_duty_wait_ms(&s.d, s.now) == UINT32_MAX);
    const int on = s.on, off = s.off;

    good_epoch(&s);
    step(&s, 60000U);
    CHECK(s.on == on && s.off == off);                              /* остановленный автомат приёмник не трогает */
}

static void test_null_arguments(void)
{
    purrgo_gnss_duty_tick(NULL, 0U, NULL);
    purrgo_gnss_duty_stop(NULL);
    purrgo_gnss_duty_start(NULL, 0U, NULL, true);
    CHECK(purrgo_gnss_duty_wait_ms(NULL, 0U) == UINT32_MAX);
    CHECK(purrgo_gnss_duty_fix_timeout_ms(NULL) > 0U);

    sim_t s;

    sim_init(&s, cfg_test(), 0U);
    purrgo_gnss_duty_start(&s.d, s.now, NULL, true);                /* без решения приёмника: ничего не падает */
    purrgo_gnss_duty_tick(&s.d, 100U, NULL);
    CHECK(s.d.state == PURRGO_GNSS_DUTY_SEARCH);

    purrgo_gnss_duty_t bare;                                         /* без ops: действия пропускаются */

    purrgo_gnss_duty_init(&bare, NULL, NULL, NULL);
    purrgo_gnss_duty_start(&bare, 0U, NULL, true);
    purrgo_gnss_duty_tick(&bare, 30000U, NULL);
    CHECK(bare.state == PURRGO_GNSS_DUTY_OFF);
}

int main(void)
{
    test_cfg_default_from_macros();
    test_start_powers_on();
    test_fix_accepted_and_period();
    test_pause_not_below_minimum();
    test_stale_solution_is_ignored();
    test_fix_type_and_validity_required();
    test_hold_waits_for_accuracy();
    test_hold_gives_up_after_hold_ms();
    test_hold_without_accuracy_info();
    test_hold_disabled();
    test_miss_backoff_ladder();
    test_timeout_does_not_grow_after_misses();
    test_success_restores_period();
    test_cold_when_time_unknown();
    test_time_known_keeps_hot_timeout();
    test_time_known_later_does_not_shrink_cold_timeout();
    test_cold_after_old_fix();
    test_slow_fixes_switch_to_continuous();
    test_fast_fix_resets_slow_run();
    test_continuous_disabled();
    test_continuous_keeps_running_while_fixes_arrive();
    test_continuous_ends_on_loss();
    test_continuous_retries_cycle();
    test_time_wraparound();
    test_fix_timeout_ms();
    test_stop();
    test_null_arguments();
    return test_finish("test_gnss_duty");
}
