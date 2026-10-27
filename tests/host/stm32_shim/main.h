/* Заглушка main.h CubeMX для host-теста display_eink.c. */
#ifndef STM32_SHIM_MAIN_H
#define STM32_SHIM_MAIN_H
#include <stdint.h>
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t ms);
#endif
