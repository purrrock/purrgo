#ifndef PURRGO_GNSS_MONITOR_H
#define PURRGO_GNSS_MONITOR_H

/*
 * Монитор связи с GNSS-приёмником (отладочный лог в UART2).
 *
 * Показывает, приходят ли от модуля байты, NMEA ли это, ищет ли модуль спутники
 * и получено ли решение. Помогает отличить неверное подключение / неисправный
 * модуль / неверную скорость от нормального холодного старта.
 *
 * Работает на данных, которые платформенный gnss_io.c отдаёт ядру, поэтому
 * не вмешивается в приём. Отключается макросом PURRGO_GNSS_MONITOR=0.
 */

#include <stdint.h>

#ifndef PURRGO_GNSS_MONITOR
#define PURRGO_GNSS_MONITOR 1
#endif

#if PURRGO_GNSS_MONITOR

/** @brief Сбросить счётчики и запомнить момент запуска (после purrgo_gnss_init). */
void purrgo_gnss_monitor_init(void);

/** @brief Учесть байт, отданный ядру (контекст основного цикла). */
void purrgo_gnss_monitor_feed(uint8_t byte);

/** @brief Учесть ошибку UART (HAL_UART_ERROR_* битами; можно из прерывания). */
void purrgo_gnss_monitor_uart_error(uint32_t error_code);

/** @brief Периодический вывод состояния; вызывать из основного цикла. */
void purrgo_gnss_monitor_process(void);

#else

static inline void purrgo_gnss_monitor_init(void) {}
static inline void purrgo_gnss_monitor_feed(uint8_t byte) { (void)byte; }
static inline void purrgo_gnss_monitor_uart_error(uint32_t error_code) { (void)error_code; }
static inline void purrgo_gnss_monitor_process(void) {}

#endif

#endif /* PURRGO_GNSS_MONITOR_H */
