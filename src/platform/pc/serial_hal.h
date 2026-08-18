#ifndef SERIAL_HAL_H
#define SERIAL_HAL_H

#include <stdint.h>
#include <stdbool.h>

bool serial_hal_open(const char *port_name, uint32_t baud_rate);
int serial_hal_read_byte(uint8_t *byte);
void serial_hal_close(void);

#endif // SERIAL_HAL_H