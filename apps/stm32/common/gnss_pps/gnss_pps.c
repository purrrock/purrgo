#include "gnss_pps.h"

#include <string.h>

void gnss_pps_init(gnss_pps_t *p)
{
    if (p != NULL)
    {
        memset((void *)p, 0, sizeof *p);
    }
}

void gnss_pps_edge(gnss_pps_t *p, uint32_t now_ms)
{
    if (p == NULL)
    {
        return;
    }

    p->seq++;                         /* нечётное: запись */
    gnss_pps_stats_t *s = &p->s;

    if (s->edges != 0U)
    {
        const uint32_t dt = now_ms - s->last_edge_ms;   /* корректно при переходе через 2^32 */

        s->intervals++;
        if ((s->intervals == 1U) || (dt < s->min_ms))
        {
            s->min_ms = dt;
        }
        if (dt > s->max_ms)
        {
            s->max_ms = dt;
        }

        if (dt >= GNSS_PPS_GAP_MS)
        {
            s->gaps++;
            /* 2000 мс - один пропуск, 3000 мс - два: округление до целых секунд. */
            s->missed += ((dt + 500U) / 1000U) - 1U;
        }
        else if (dt + GNSS_PPS_TOLERANCE_MS < 1000U)
        {
            s->too_short++;
        }
        else if (dt <= 1000U + GNSS_PPS_TOLERANCE_MS)
        {
            s->ok++;
        }
        else
        {
            s->drift++;
        }
    }

    s->edges++;
    s->last_edge_ms = now_ms;
    p->seq++;                         /* чётное: запись завершена */
}

void gnss_pps_snapshot(const gnss_pps_t *p, gnss_pps_stats_t *out)
{
    if ((p == NULL) || (out == NULL))
    {
        return;
    }

    for (;;)
    {
        const uint32_t before = p->seq;

        if ((before & 1U) == 0U)
        {
            *out = p->s;
            if (p->seq == before)
            {
                return;
            }
        }
    }
}

bool gnss_pps_active(const gnss_pps_stats_t *s, uint32_t now_ms, uint32_t max_age_ms)
{
    return (s != NULL) && (s->edges != 0U) && ((uint32_t)(now_ms - s->last_edge_ms) <= max_age_ms);
}
