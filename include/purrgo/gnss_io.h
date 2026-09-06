#ifndef PURRGO_GNSS_IO_H
#define PURRGO_GNSS_IO_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Прочитать один байт из входного потока GNSS.
 *
 * Функция не занимается разбором NMEA или UBX.
 * Она только получает очередной байт от физического
 * или эмулируемого GNSS-приёмника.
 *
 * @param[out] byte
 *     Указатель на переменную, куда будет записан прочитанный байт.
 *
 * @return true
 *     Если байт был получен.
 *
 * @return false
 *     Если доступного байта сейчас нет.
 *
 * Важно:
 *     false не означает ошибку GNSS. Это также может означать,
 *     что в данный момент новых данных нет.
 */
bool purrgo_gnss_read_byte(uint8_t *byte);

#endif /* PURRGO_GNSS_IO_H */