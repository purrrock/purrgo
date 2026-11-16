/* Платформа ПК для профилировки кадра карты (см. purrgo/render_prof.h). */
#include "purrgo/render_prof.h"

#if PURRGO_RENDER_PROF

#include <time.h>

void purrgo_prof_platform_enable(void)
{
}

/* Тик - наносекунда; 32 бита хватает на 4 с, кадру этого достаточно. */
uint32_t purrgo_prof_ticks(void)
{
    struct timespec ts;
    (void)timespec_get(&ts, TIME_UTC);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec);
}

uint32_t purrgo_prof_ticks_per_us(void)
{
    return 1000u;
}

#else

typedef int purrgo_render_prof_pc_disabled_t;

#endif
