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

uint32_t purrgo_stm32_time_ms(void)
{
    return 0U;
}
