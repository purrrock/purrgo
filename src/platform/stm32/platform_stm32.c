#include "platform_stm32.h"

/*
 * This file is intentionally not coupled to a specific STM32 HAL yet.
 * Board/peripheral integration will be added after the prototype wiring is fixed.
 */
bool purrgo_stm32_gnss_read_byte(uint8_t *byte)
{
    (void)byte;
    return false;
}

// Forward declaration of the standard STM32 HAL function to avoid introducing
// board-specific headers before the hardware architecture is finalized.
uint32_t HAL_GetTick(void);

uint32_t purrgo_stm32_time_ms(void)
{
    return HAL_GetTick();
}
