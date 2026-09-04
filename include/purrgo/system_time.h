#ifndef PURRGO_SYSTEM_TIME_H
#define PURRGO_SYSTEM_TIME_H

#include <stdint.h>

/**
 * @brief Returns the monotonic elapsed time since system startup in milliseconds.
 *
 * This function must be implemented by the platform (e.g., HAL_GetTick() on STM32,
 * or a clock()-based wrapper on PC). It wraps around after approximately 49.7 days.
 */
uint32_t purrgo_system_time_ms(void);

#endif /* PURRGO_SYSTEM_TIME_H */
