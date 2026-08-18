#ifndef PURRGO_PLATFORM_STM32_H
#define PURRGO_PLATFORM_STM32_H

#include <stdbool.h>
#include <stdint.h>

/* STM32-facing adapter interface. Hardware implementation is added per board. */
bool purrgo_stm32_gnss_read_byte(uint8_t *byte);
uint32_t purrgo_stm32_time_ms(void);

#endif /* PURRGO_PLATFORM_STM32_H */
