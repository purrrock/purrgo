#ifndef PURRGO_GNSS_IO_H
#define PURRGO_GNSS_IO_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize GNSS input.
 */
void purrgo_gnss_init(void);

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

/**
 * @brief Включить GNSS-приёмник и настроить его заново.
 *
 * STM32: отпускает линию EN (PB0), заново инициализирует LPUART1 и приём
 * через DMA, затем отправляет приёмнику настройку (скорость, набор NMEA).
 * Повторный вызов при уже включённом GNSS ничего не делает.
 * PC: заглушка.
 */
void purrgo_gnss_power_on(void);

/**
 * @brief Выключить GNSS-приёмник (EN = Low).
 *
 * STM32: останавливает приём (DMA), тянет EN (PB0) в Low, отключает LPUART1
 * и переводит линию TX к модулю (PA2) в Low, RX (PA3) остаётся без подтяжки.
 * После вызова purrgo_gnss_read_byte() возвращает false.
 * Повторный вызов при уже выключенном GNSS ничего не делает.
 * PC: заглушка.
 */
void purrgo_gnss_power_off(void);

#endif /* PURRGO_GNSS_IO_H */