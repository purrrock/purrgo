#include "purrgo/system_time.h"
#include <time.h>

uint32_t purrgo_system_time_ms(void) {
    const clock_t ticks = clock();
    return (uint32_t)((ticks * 1000U) / CLOCKS_PER_SEC);
}
