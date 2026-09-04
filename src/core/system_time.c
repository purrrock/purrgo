#include "purrgo/system_time.h"
#include "purrgo/hardware_config.h"

#if PURRGO_HW_MCU == PURRGO_PLATFORM_PC
#include "../platform/pc/platform_pc.h"

uint32_t purrgo_system_time_ms(void) {
    return purrgo_pc_time_ms();
}
#else
#include "../platform/stm32/platform_stm32.h"

uint32_t purrgo_system_time_ms(void) {
    return purrgo_stm32_time_ms();
}
#endif
