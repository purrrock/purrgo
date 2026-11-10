#ifndef PURRGO_GNSS_MONITOR_H
#define PURRGO_GNSS_MONITOR_H

/*
 * Монитор связи с GNSS-приёмником (отладочный лог в UART1).
 *
 * Показывает, приходят ли от модуля байты, NMEA ли это, ищет ли модуль спутники
 * и получено ли решение. Помогает отличить неверное подключение / неисправный
 * модуль / неверную скорость от нормального холодного старта.
 *
 * Работает на данных, которые платформенный gnss_io.c отдаёт ядру, поэтому
 * не вмешивается в приём. Отключается макросом PURRGO_GNSS_MONITOR=0.
 */

#include <stdbool.h>
#include <stdint.h>

#ifndef PURRGO_GNSS_MONITOR
#define PURRGO_GNSS_MONITOR 1
#endif

#if PURRGO_GNSS_MONITOR

/** @brief Сбросить счётчики и запомнить момент запуска (после purrgo_gnss_init). */
void purrgo_gnss_monitor_init(void);

/** @brief Остановить монитор (GNSS выключен): отчёты и подсчёт фронтов 1PPS прекращаются до init. */
void purrgo_gnss_monitor_stop(void);

/**
 * @brief Состояние связи с модулем (gnss_link): baud - текущая скорость UART, ready -
 *        false пока идёт поиск скорости (текстовые отчёты о связи не выводятся, счёт
 *        1PPS не прерывается), true - связь установлена или поиск закончился неудачей
 *        (диагностика начинается с нуля на скорости baud).
 *        Вызывать после purrgo_gnss_init(), purrgo_gnss_monitor_init() их не меняет.
 */
void purrgo_gnss_monitor_link(uint32_t baud, bool ready);

/** @brief Учесть байт, отданный ядру (контекст основного цикла). */
void purrgo_gnss_monitor_feed(uint8_t byte);

/** @brief Учесть ошибку UART (HAL_UART_ERROR_* битами; можно из прерывания). */
void purrgo_gnss_monitor_uart_error(uint32_t error_code);

/** @brief Учесть фронт 1PPS (из прерывания EXTI; now_ms - HAL_GetTick()). */
void purrgo_gnss_monitor_pps_edge(uint32_t now_ms);

/** @brief Периодический вывод состояния; вызывать из основного цикла. */
void purrgo_gnss_monitor_process(void);

#else

static inline void purrgo_gnss_monitor_init(void) {}
static inline void purrgo_gnss_monitor_stop(void) {}
static inline void purrgo_gnss_monitor_link(uint32_t baud, bool ready) { (void)baud; (void)ready; }
static inline void purrgo_gnss_monitor_feed(uint8_t byte) { (void)byte; }
static inline void purrgo_gnss_monitor_uart_error(uint32_t error_code) { (void)error_code; }
static inline void purrgo_gnss_monitor_pps_edge(uint32_t now_ms) { (void)now_ms; }
static inline void purrgo_gnss_monitor_process(void) {}

#endif

#endif /* PURRGO_GNSS_MONITOR_H */
