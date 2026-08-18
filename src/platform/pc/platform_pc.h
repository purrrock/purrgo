#ifndef PURRGO_PLATFORM_PC_H
#define PURRGO_PLATFORM_PC_H

#include <stdbool.h>
#include <stdint.h>

/* Minimal PC-side services used while developing the portable core. */
bool purrgo_pc_read_byte(uint8_t *byte);
uint32_t purrgo_pc_time_ms(void);

#endif /* PURRGO_PLATFORM_PC_H */
