/*
 * Платформа STM32 для профилировки кадра карты (см. purrgo/render_prof.h):
 * счётчик тактов DWT->CYCCNT. Сам по себе файл ничего не делает, пока
 * PURRGO_RENDER_PROF равен 0.
 */
#include "purrgo/render_prof.h"

#if PURRGO_RENDER_PROF

#include "main.h"

/*
 * Вызывается в начале каждого кадра: после Stop-режимов отладочный блок мог быть
 * выключен. Счётчик не обнуляется, разности считаются беззнаковым вычитанием.
 */
void purrgo_prof_platform_enable(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t purrgo_prof_ticks(void)
{
    return DWT->CYCCNT;
}

uint32_t purrgo_prof_ticks_per_us(void)
{
    return (uint32_t)((SystemCoreClock + 500000UL) / 1000000UL);
}

#else

typedef int purrgo_render_prof_stm32_disabled_t;

#endif
