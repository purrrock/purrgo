#include "purrgo/render_prof.h"

#if PURRGO_RENDER_PROF

#include "purrgo/logger.h"
#include "purrgo/ram_arena.h"

#include <stdlib.h>
#include <string.h>

static uint32_t s_ticks[PURRGO_PROF_COUNT];
static uint32_t s_calls[PURRGO_PROF_COUNT];
static uint32_t s_max[PURRGO_PROF_COUNT];
static uint32_t s_cnt[PURRGO_PROF_CNT_COUNT];
static uint32_t s_frame_start;

/* Трасса обращений к секторам: slot(4) | miss(1) | sector(27). */
#define TRACE_SLOTS      16u
#define TRACE_MISS_BIT   (1u << 27)
#define TRACE_SECTOR_MAX (TRACE_MISS_BIT - 1u)
#define SIM_MAX_ENTRIES  1024u
#define SIM_EMPTY        0xFFFFFFFFu

/* Трасса живёт в арене режима карты (остаток после кэша секторов), свой RAM не занимает. */
static uint32_t *s_trace;
static uint32_t s_trace_cap;
static uint32_t s_trace_session;   /* purrgo_arena_session(), при котором выделена трасса */
static uint32_t s_trace_n;
static bool s_trace_ovf;
static bool s_trace_active; /* пишем только между begin_frame и end_frame/abort_frame */

static char s_slot_name[TRACE_SLOTS][16];
static uint8_t s_slot_ra[TRACE_SLOTS];
static uint32_t s_cache_entries = 64u;

static uint32_t s_sim_tag[SIM_MAX_ENTRIES];
static uint32_t s_sim_stamp[SIM_MAX_ENTRIES];

void purrgo_render_prof_begin_frame(void)
{
    purrgo_prof_platform_enable();

    for (int i = 0; i < (int)PURRGO_PROF_COUNT; i++) {
        s_ticks[i] = 0;
        s_calls[i] = 0;
        s_max[i] = 0;
    }
    for (int i = 0; i < (int)PURRGO_PROF_CNT_COUNT; i++) {
        s_cnt[i] = 0;
    }

    if (purrgo_arena_owner() != PURRGO_ARENA_MAP) {
        s_trace = NULL;
        s_trace_cap = 0u;
    } else if (s_trace == NULL || s_trace_session != purrgo_arena_session()) {
        /* Первый кадр в этой сессии арены: берём весь остаток (кэш секторов уже выделен). */
        const size_t rest = purrgo_arena_remaining(PURRGO_ARENA_MAP);
        void *ram = (rest != 0u) ? purrgo_arena_alloc(PURRGO_ARENA_MAP, rest) : NULL;

        s_trace = (uint32_t *)ram;
        s_trace_cap = (ram != NULL) ? (uint32_t)(rest / sizeof(uint32_t)) : 0u;
        if (s_trace_cap > PURRGO_PROF_TRACE_MAX) {
            s_trace_cap = PURRGO_PROF_TRACE_MAX;
        }
        s_trace_session = purrgo_arena_session();
    }
    s_trace_n = 0;
    s_trace_ovf = (s_trace_cap == 0u);
    s_trace_active = (s_trace_cap != 0u);

    s_frame_start = purrgo_prof_ticks();
}

void purrgo_render_prof_file_info(uint8_t slot, const char *path, uint8_t read_ahead,
                                  uint32_t cache_entries)
{
    if (slot >= TRACE_SLOTS) {
        return;
    }

    s_slot_ra[slot] = read_ahead;
    if (cache_entries != 0u) {
        s_cache_entries = cache_entries;
    }

    if (path != NULL) {
        const char *base = path;
        for (const char *p = path; *p != '\0'; p++) {
            if (*p == '/' || *p == '\\') {
                base = p + 1;
            }
        }
        strncpy(s_slot_name[slot], base, sizeof(s_slot_name[slot]) - 1u);
        s_slot_name[slot][sizeof(s_slot_name[slot]) - 1u] = '\0';
    }
}

void purrgo_render_prof_sector(uint8_t slot, uint32_t sector, bool miss)
{
    if (slot >= TRACE_SLOTS) {
        return;
    }
    if (!s_trace_active) {
        return;
    }
    if (s_trace_n >= s_trace_cap) {
        s_trace_ovf = true;
        return;
    }
    s_trace[s_trace_n++] = ((uint32_t)slot << 28) | (miss ? TRACE_MISS_BIT : 0u) |
                           (sector & TRACE_SECTOR_MAX);
}

void purrgo_render_prof_add(purrgo_prof_id_t id, uint32_t ticks)
{
    if ((unsigned)id < (unsigned)PURRGO_PROF_COUNT) {
        s_ticks[id] += ticks;
        s_calls[id]++;
        if (ticks > s_max[id]) {
            s_max[id] = ticks;
        }
    }
}

void purrgo_render_prof_count(purrgo_prof_cnt_t id, uint32_t n)
{
    if ((unsigned)id < (unsigned)PURRGO_PROF_CNT_COUNT) {
        s_cnt[id] += n;
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

static bool slot_is_idx(uint32_t slot)
{
    const char *n = s_slot_name[slot];
    const size_t len = strlen(n);
    return len >= 4u && strcmp(&n[len - 4u], ".idx") == 0;
}

static uint32_t slot_ra(uint32_t slot, bool idx_ra8)
{
    uint32_t ra = (s_slot_ra[slot] != 0u) ? s_slot_ra[slot] : 1u;
    if (idx_ra8 && slot_is_idx(slot)) {
        ra = 8u;
    }
    return ra;
}

static void sim_insert(uint32_t n, uint32_t key, uint32_t *clock)
{
    uint32_t victim = 0;
    uint32_t oldest = 0xFFFFFFFFu;

    for (uint32_t i = 0; i < n; i++) {
        if (s_sim_tag[i] == SIM_EMPTY) {
            victim = i;
            break;
        }
        if (s_sim_stamp[i] < oldest) {
            oldest = s_sim_stamp[i];
            victim = i;
        }
    }
    s_sim_tag[victim] = key;
    s_sim_stamp[victim] = ++(*clock);
}

static int sim_find(uint32_t n, uint32_t key)
{
    for (uint32_t i = 0; i < n; i++) {
        if (s_sim_tag[i] == key) {
            return (int)i;
        }
    }
    return -1;
}

/* LRU-кэш на n секторов с read-ahead по файлам; считает команды чтения SD и секторы. */
static void sim_run(uint32_t n, bool idx_ra8, uint32_t *cmds, uint32_t *sects)
{
    if (n > SIM_MAX_ENTRIES) {
        n = SIM_MAX_ENTRIES;
    }
    for (uint32_t i = 0; i < n; i++) {
        s_sim_tag[i] = SIM_EMPTY;
        s_sim_stamp[i] = 0;
    }

    uint32_t clock = 0;
    *cmds = 0;
    *sects = 0;

    for (uint32_t i = 0; i < s_trace_n; i++) {
        const uint32_t slot = s_trace[i] >> 28;
        const uint32_t sector = s_trace[i] & TRACE_SECTOR_MAX;
        const uint32_t key = s_trace[i] & ~TRACE_MISS_BIT;

        const int hit = sim_find(n, key);
        if (hit >= 0) {
            s_sim_stamp[hit] = ++clock;
            continue;
        }

        const uint32_t ra = slot_ra(slot, idx_ra8);
        const uint32_t base = sector - (sector % ra);

        for (uint32_t k = 0; k < ra; k++) {
            const uint32_t s = base + k;
            if (s == sector) {
                continue;
            }
            const uint32_t other_key = (slot << 28) | s;
            if (sim_find(n, other_key) < 0) {
                sim_insert(n, other_key, &clock);
            }
        }
        sim_insert(n, key, &clock);

        (*cmds)++;
        *sects += ra;
    }
}

static int cmp_u32(const void *a, const void *b)
{
    const uint32_t x = *(const uint32_t *)a;
    const uint32_t y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

static void trace_report(void)
{
    uint32_t acc[TRACE_SLOTS] = {0};
    uint32_t miss[TRACE_SLOTS] = {0};
    uint32_t seq[TRACE_SLOTS] = {0};
    uint32_t near_[TRACE_SLOTS] = {0};
    uint32_t dist[TRACE_SLOTS] = {0};
    uint32_t last_sector[TRACE_SLOTS] = {0};
    bool has_last[TRACE_SLOTS] = {false};

    for (uint32_t i = 0; i < s_trace_n; i++) {
        const uint32_t slot = s_trace[i] >> 28;
        acc[slot]++;

        if ((s_trace[i] & TRACE_MISS_BIT) == 0u) {
            continue;
        }

        const uint32_t sector = s_trace[i] & TRACE_SECTOR_MAX;
        const uint32_t ra = slot_ra(slot, false);
        miss[slot]++;

        if (has_last[slot]) {
            if (sector / ra == last_sector[slot] / ra + 1u) {
                seq[slot]++;
            }
            const uint32_t d = (sector > last_sector[slot]) ? (sector - last_sector[slot])
                                                           : (last_sector[slot] - sector);
            if (d != 0u && d <= 16u) {
                near_[slot]++;
            }
        }
        last_sector[slot] = sector;
        has_last[slot] = true;
    }

    /* Различные секторы: сортировка трассы без бита промаха (порядок дальше не нужен). */
    uint32_t sim_res[6][2];
    static const uint32_t sizes[3] = {0u, 256u, 768u};
    for (uint32_t s = 0; s < 3u; s++) {
        const uint32_t n = (sizes[s] != 0u) ? sizes[s] : s_cache_entries;
        sim_run(n, false, &sim_res[s * 2u][0], &sim_res[s * 2u][1]);
        sim_run(n, true, &sim_res[s * 2u + 1u][0], &sim_res[s * 2u + 1u][1]);
    }

    for (uint32_t i = 0; i < s_trace_n; i++) {
        s_trace[i] &= ~TRACE_MISS_BIT;
    }
    if (s_trace_n != 0u) {
        qsort(s_trace, s_trace_n, sizeof(s_trace[0]), cmp_u32);
    }
    for (uint32_t i = 0; i < s_trace_n; i++) {
        if (i == 0u || s_trace[i] != s_trace[i - 1u]) {
            dist[s_trace[i] >> 28]++;
        }
    }

    for (uint32_t slot = 0; slot < TRACE_SLOTS; slot++) {
        if (acc[slot] == 0u) {
            continue;
        }
        purrgo_logger_write(
            "PROF3 f=%s ra=%lu acc=%lu miss=%lu seq=%lu near=%lu dist=%lu\r\n",
            s_slot_name[slot], (unsigned long)slot_ra(slot, false),
            (unsigned long)acc[slot], (unsigned long)miss[slot], (unsigned long)seq[slot],
            (unsigned long)near_[slot], (unsigned long)dist[slot]);
    }

    purrgo_logger_write(
        "PROF4 ovf=%d cur=%lu ccur=%lu/%lu ccuri8=%lu/%lu c256=%lu/%lu c256i8=%lu/%lu "
        "c768=%lu/%lu c768i8=%lu/%lu\r\n",
        s_trace_ovf ? 1 : 0, (unsigned long)s_cache_entries,
        (unsigned long)sim_res[0][0], (unsigned long)sim_res[0][1],
        (unsigned long)sim_res[1][0], (unsigned long)sim_res[1][1],
        (unsigned long)sim_res[2][0], (unsigned long)sim_res[2][1],
        (unsigned long)sim_res[3][0], (unsigned long)sim_res[3][1],
        (unsigned long)sim_res[4][0], (unsigned long)sim_res[4][1],
        (unsigned long)sim_res[5][0], (unsigned long)sim_res[5][1]);
}

void purrgo_render_prof_abort_frame(void)
{
    s_trace_active = false;
}

void purrgo_render_prof_end_frame(int scale, int ok)
{
    s_trace_active = false;

    const uint32_t frame = (uint32_t)(purrgo_prof_ticks() - s_frame_start);
    const uint32_t per_us = purrgo_prof_ticks_per_us();
    const uint32_t ovh = measure_overhead();

    /* Накладные расходы вычитаются у всех статей и из кадра: замер стоит вокруг каждого вызова. */
    const uint32_t proj = net_ticks(PURRGO_PROF_PROJ, ovh);
    const uint32_t fill = net_ticks(PURRGO_PROF_FILL, ovh);
    const uint32_t lines = net_ticks(PURRGO_PROF_LINES, ovh);
    const uint32_t lbl = net_ticks(PURRGO_PROF_LABELS, ovh);
    const uint32_t name = net_ticks(PURRGO_PROF_NAME, ovh);

    /* В сумму кадра входят только первые пять статей, остальные лежат внутри `other`. */
    uint32_t calls = 0;
    for (int i = 0; i < (int)PURRGO_PROF_SYNC; i++) {
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

    const uint32_t sync = net_ticks(PURRGO_PROF_SYNC, ovh);
    const uint32_t mlpi = net_ticks(PURRGO_PROF_MLP_INIT, ovh);
    const uint32_t mlpn = net_ticks(PURRGO_PROF_MLP_NEXT, ovh);
    const uint32_t sd = net_ticks(PURRGO_PROF_SD, ovh);
    const uint32_t inner = sync + mlpi + mlpn;
    const uint32_t rest = (other > inner) ? (other - inner) : 0u;

    purrgo_logger_write(
        "PROF2 sync=%lu us mlpi=%lu us n=%lu mlpn=%lu us n=%lu rest=%lu us "
        "sd=%lu us n=%lu max=%lu us sect=%lu hit=%lu rd=%lu B=%lu\r\n",
        (unsigned long)to_us(sync, per_us),
        (unsigned long)to_us(mlpi, per_us), (unsigned long)s_calls[PURRGO_PROF_MLP_INIT],
        (unsigned long)to_us(mlpn, per_us), (unsigned long)s_calls[PURRGO_PROF_MLP_NEXT],
        (unsigned long)to_us(rest, per_us),
        (unsigned long)to_us(sd, per_us), (unsigned long)s_calls[PURRGO_PROF_SD],
        (unsigned long)to_us(s_max[PURRGO_PROF_SD], per_us),
        (unsigned long)s_cnt[PURRGO_PROF_CNT_SD_SECTORS],
        (unsigned long)s_cnt[PURRGO_PROF_CNT_CACHE_HIT],
        (unsigned long)s_cnt[PURRGO_PROF_CNT_FS_READ],
        (unsigned long)s_cnt[PURRGO_PROF_CNT_FS_READ_BYTES]);

    trace_report();

    /* Запись между кадрами не ведётся (s_trace_active), память остаётся за режимом карты. */
    s_trace_n = 0;
}

#else

/* Пустая единица трансляции не допускается -Wpedantic. */
typedef int purrgo_render_prof_disabled_t;

#endif
