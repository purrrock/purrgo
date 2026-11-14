/*
 * Host-тест статистики 1PPS (stm32/port/gnss): подсчёт фронтов,
 * классификация интервалов, пропуски, переход через 2^32 мс, снимок, активность.
 */
#include <stdint.h>
#include <string.h>
#include "gnss_pps.h"
#include "test_common.h"

static gnss_pps_stats_t snap(const gnss_pps_t *p)
{
    gnss_pps_stats_t s;
    gnss_pps_snapshot(p, &s);
    return s;
}

static void test_empty(void)
{
    gnss_pps_t p;
    gnss_pps_init(&p);
    gnss_pps_stats_t s = snap(&p);

    CHECK(s.edges == 0 && s.intervals == 0 && s.ok == 0);
    CHECK(s.min_ms == 0 && s.max_ms == 0);
    CHECK(!gnss_pps_active(&s, 1000, 2000));
}

static void test_steady_1hz(void)
{
    gnss_pps_t p;
    gnss_pps_init(&p);

    for (uint32_t i = 0; i < 600; i++) {            /* 10 минут, дрожание +-3 мс */
        gnss_pps_edge(&p, 5000u + i * 1000u + (i % 2 ? 3u : 0u));
    }
    gnss_pps_stats_t s = snap(&p);

    CHECK(s.edges == 600);
    CHECK(s.intervals == 599);
    CHECK(s.ok == 599);
    CHECK(s.too_short == 0 && s.gaps == 0 && s.missed == 0 && s.drift == 0);
    CHECK(s.min_ms == 997 && s.max_ms == 1003);
    CHECK(gnss_pps_active(&s, 5000u + 599u * 1000u + 3u + 900u, 1500));
    CHECK(!gnss_pps_active(&s, 5000u + 599u * 1000u + 3u + 5000u, 1500));
}

static void test_classification(void)
{
    gnss_pps_t p;
    gnss_pps_init(&p);
    uint32_t t = 100;

    gnss_pps_edge(&p, t);                 /* первый фронт: интервала нет */
    t += 1000; gnss_pps_edge(&p, t);      /* ok */
    t += 1050; gnss_pps_edge(&p, t);      /* ok: граница допуска */
    t += 1051; gnss_pps_edge(&p, t);      /* drift */
    t += 950;  gnss_pps_edge(&p, t);      /* ok: граница допуска снизу */
    t += 949;  gnss_pps_edge(&p, t);      /* too_short */
    t += 10;   gnss_pps_edge(&p, t);      /* too_short (ложный фронт) */
    t += 2000; gnss_pps_edge(&p, t);      /* gap: один пропуск */
    t += 3000; gnss_pps_edge(&p, t);      /* gap: два пропуска */
    t += 1500; gnss_pps_edge(&p, t);      /* gap по порогу: 1500 -> 2 с -> один пропуск */

    gnss_pps_stats_t s = snap(&p);

    CHECK(s.edges == 10);
    CHECK(s.intervals == 9);
    CHECK(s.ok == 3);
    CHECK(s.drift == 1);
    CHECK(s.too_short == 2);
    CHECK(s.gaps == 3);
    CHECK(s.missed == 1 + 2 + 1);
    CHECK(s.min_ms == 10 && s.max_ms == 3000);
}

static void test_wraparound(void)
{
    gnss_pps_t p;
    gnss_pps_init(&p);

    gnss_pps_edge(&p, 0xFFFFFFFFu - 400u);
    gnss_pps_edge(&p, 599u);              /* 1000 мс через переход */
    gnss_pps_stats_t s = snap(&p);

    CHECK(s.ok == 1 && s.intervals == 1);
    CHECK(s.min_ms == 1000 && s.max_ms == 1000);
    CHECK(gnss_pps_active(&s, 700u, 500));
}

static void test_reinit_and_null(void)
{
    gnss_pps_t p;
    gnss_pps_init(&p);
    gnss_pps_edge(&p, 10);
    gnss_pps_edge(&p, 1010);
    gnss_pps_init(&p);                    /* новое включение GNSS: счёт с нуля */
    gnss_pps_stats_t s = snap(&p);
    CHECK(s.edges == 0 && s.ok == 0 && s.last_edge_ms == 0);

    gnss_pps_init(NULL);                  /* не падает */
    gnss_pps_edge(NULL, 1);
    gnss_pps_snapshot(NULL, &s);
    gnss_pps_snapshot(&p, NULL);
    CHECK(!gnss_pps_active(NULL, 0, 0));
    CHECK(p.seq % 2 == 0);
}

int main(void)
{
    test_empty();
    test_steady_1hz();
    test_classification();
    test_wraparound();
    test_reinit_and_null();
    return test_finish("test_gnss_pps");
}
