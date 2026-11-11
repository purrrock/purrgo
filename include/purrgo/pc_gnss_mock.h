#ifndef PC_GNSS_MOCK_H
#define PC_GNSS_MOCK_H

#include <purrgo/gnss_types.h>
#include <stdbool.h>
#include <stdint.h>

void pc_gnss_mock_init(void);
void pc_gnss_mock_update(void);
bool pc_gnss_mock_read_byte(uint8_t *byte);

/*
 * Команда приёмнику (purrgo_gnss_write): мок понимает UBX-NAV-RESETODO (обнуляет
 * одометр) и опрос UBX-MON-VER (ответит в ближайшем обновлении). Только PURRGO_GNSS_UBX = 1.
 */
bool pc_gnss_mock_write(const uint8_t *data, uint16_t length);

#endif // PC_GNSS_MOCK_H
