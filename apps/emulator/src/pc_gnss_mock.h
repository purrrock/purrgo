#ifndef PC_GNSS_MOCK_H
#define PC_GNSS_MOCK_H

#include <purrgo/gnss_types.h>
#include <stdbool.h>
#include <stdint.h>

void pc_gnss_mock_init(void);
void pc_gnss_mock_update(void);
bool pc_gnss_mock_read_byte(uint8_t *byte);

#endif // PC_GNSS_MOCK_H
