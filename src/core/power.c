#include "purrgo/power.h"
#include <stdlib.h>

__attribute__((weak)) void purrgo_system_power_off(void) {
    // Default implementation for PC emulator or unsupported platforms
    exit(0);
}
