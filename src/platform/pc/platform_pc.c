#include "platform_pc.h"

#include <stdio.h>
#include <time.h>

bool purrgo_pc_read_byte(uint8_t *byte)
{
    const int c = getchar();
    if (c == EOF) {
        return false;
    }

    *byte = (uint8_t)c;
    return true;
}

uint32_t purrgo_pc_time_ms(void)
{
    const clock_t ticks = clock();
    return (uint32_t)((ticks * 1000U) / CLOCKS_PER_SEC);
}
