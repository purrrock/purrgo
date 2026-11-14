#include "purrgo/system_time.h"
#include <stdint.h>

// Forward declaration of the standard STM32 HAL function to avoid introducing
// board-specific headers before the hardware architecture is finalized.
uint32_t HAL_GetTick(void);

uint32_t purrgo_system_time_ms(void) {
    return HAL_GetTick();
}
