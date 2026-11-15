#ifndef PURRGO_GNSS_IO_H
#define PURRGO_GNSS_IO_H

#include <stdbool.h>
#include <stdint.h>

#include "purrgo/gnss_receiver.h"

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

/**
 * @brief Отправить байты приёмнику GNSS (команды UBX из ядра: опрос версии, сброс одометра).
 *
 * STM32: блокирующая передача по LPUART1; false, если GNSS выключен или идёт поиск
 * скорости UART (gnss_link). PC: с эмулятором GNSS - команда уходит в мок, с
 * реальным COM-портом передача не поддерживается (false).
 *
 * @return true, если данные переданы.
 */
bool purrgo_gnss_write(const uint8_t *data, uint16_t length);

/**
 * @brief Переключить профиль настройки приёмника (см. purrgo_gnss_profile_t в gnss_receiver.h).
 *
 * Навигация (PURRGO_GNSS_PROFILE_NAV) - данные каждую секунду, PSMCT; трекер
 * (PURRGO_GNSS_PROFILE_TRACKER) - PSMOO: приёмник просыпается раз в PURRGO_GNSS_TRACKER_PERIOD_S и
 * между фиксами молчит. Вызывается ядром при входе в режим TRACKER (SCREEN OFF) и выходе из него.
 *
 * STM32: не блокирует. Вход в трекер - сразу отправка настройки (повторно через
 * PURRGO_GNSS_CONFIG_RETRY_MS); выход - приёмник будится активностью на RX, настройка навигации
 * уходит после паузы. Пока GNSS выключен или идёт поиск скорости UART, профиль запоминается и
 * применяется после установления связи. Повторный вызов с тем же профилем ничего не делает.
 * PC: заглушка.
 */
void purrgo_gnss_set_profile(purrgo_gnss_profile_t profile);

#endif /* PURRGO_GNSS_IO_H */
