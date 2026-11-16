#include "purrgo/render_prof.h"

#if PURRGO_RENDER_PROF

#include "purrgo/logger.h"

static uint32_t s_ticks[PURRGO_PROF_COUNT];
static uint32_t s_calls[PURRGO_PROF_COUNT];
static uint32_t s_frame_start;

void purrgo_render_prof_begin_frame(void)
{
    purrgo_prof_platform_enable();

    for (int i = 0; i < (int)PURRGO_PROF_COUNT; i++) {
        s_ticks[i] = 0;
        s_calls[i] = 0;
    }

    s_frame_start = purrgo_prof_ticks();
}

void purrgo_render_prof_add(purrgo_prof_id_t id, uint32_t ticks)
{
    if ((unsigned)id < (unsigned)PURRGO_PROF_COUNT) {
        s_ticks[id] += ticks;
        s_calls[id]++;
    }
}

/* Стоимость одного замера (пара BEGIN/END без работы между ними), такты. */
static uint32_t measure_overhead(void)
{
    enum { N = 64 };
    uint32_t sink = 0;
    const uint32_t t0 = purrgo_prof_ticks();

    for (int i = 0; i < N; i++) {
        const uint32_t a = purrgo_prof_ticks();
        sink += (uint32_t)(purrgo_prof_ticks() - a);
    }

    const uint32_t total = (uint32_t)(purrgo_prof_ticks() - t0);
    (void)sink;
    return total / N;
}

static uint32_t net_ticks(purrgo_prof_id_t id, uint32_t ovh)
{
    const uint32_t cut = s_calls[id] * ovh;
    return (s_ticks[id] > cut) ? (s_ticks[id] - cut) : 0u;
}

static uint32_t to_us(uint32_t ticks, uint32_t per_us)
{
    return (uint32_t)(((uint64_t)ticks) / (per_us != 0u ? per_us : 1u));
}

void purrgo_render_prof_end_frame(int scale, int ok)
{
    const uint32_t frame = (uint32_t)(purrgo_prof_ticks() - s_frame_start);
    const uint32_t per_us = purrgo_prof_ticks_per_us();
    const uint32_t ovh = measure_overhead();

    /* Накладные расходы вычитаются у всех статей и из кадра: замер стоит вокруг каждого вызова. */
    const uint32_t proj = net_ticks(PURRGO_PROF_PROJ, ovh);
    const uint32_t fill = net_ticks(PURRGO_PROF_FILL, ovh);
    const uint32_t lines = net_ticks(PURRGO_PROF_LINES, ovh);
    const uint32_t lbl = net_ticks(PURRGO_PROF_LABELS, ovh);
    const uint32_t name = net_ticks(PURRGO_PROF_NAME, ovh);

    uint32_t calls = 0;
    for (int i = 0; i < (int)PURRGO_PROF_COUNT; i++) {
        calls += s_calls[i];
    }
    const uint32_t frame_net = (frame > calls * ovh) ? (frame - calls * ovh) : 0u;
    const uint32_t used = proj + fill + lines + lbl + name;
    const uint32_t other = (frame_net > used) ? (frame_net - used) : 0u;

    purrgo_logger_write(
        "PROF scale=%d ok=%d frame=%lu us proj=%lu us n=%lu fill=%lu us n=%lu "
        "lines=%lu us n=%lu lbl=%lu us n=%lu name=%lu us n=%lu other=%lu us "
        "ovh=%lu tick per_us=%lu\r\n",
        scale, ok,
        (unsigned long)to_us(frame_net, per_us),
        (unsigned long)to_us(proj, per_us), (unsigned long)s_calls[PURRGO_PROF_PROJ],
        (unsigned long)to_us(fill, per_us), (unsigned long)s_calls[PURRGO_PROF_FILL],
        (unsigned long)to_us(lines, per_us), (unsigned long)s_calls[PURRGO_PROF_LINES],
        (unsigned long)to_us(lbl, per_us), (unsigned long)s_calls[PURRGO_PROF_LABELS],
        (unsigned long)to_us(name, per_us), (unsigned long)s_calls[PURRGO_PROF_NAME],
        (unsigned long)to_us(other, per_us),
        (unsigned long)ovh, (unsigned long)per_us);
}

#else

/* Пустая единица трансляции не допускается -Wpedantic. */
typedef int purrgo_render_prof_disabled_t;

#endif
