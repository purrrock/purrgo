#ifndef PURRGO_GNSS_RAW_H
#define PURRGO_GNSS_RAW_H

/*
 * "Сырой" доступ к GNSS-приёмнику для аппаратных тестов (stm32/port/app/hw_test_gnss.c).
 *
 * Тот же LPUART1 + LPDMA1 + кольцевой буфер, что и в рабочей прошивке, но без gnss_link (подбор скорости),
 * без отправки настройки, без монитора и без автоматического восстановления связи после шторма ошибок:
 * тест сам решает, что отправлять модулю. Байты читаются purrgo_gnss_read_byte(), отправка - HAL_UART_Transmit()
 * на hlpuart1. После purrgo_gnss_raw_start() режим остаётся включённым до сброса МК.
 */

#include <stdbool.h>
#include <stdint.h>

/* Запустить приём на скорости baud (EN не трогает). false - ошибка DMA/UART. */
bool purrgo_gnss_raw_start(uint32_t baud);

/* Сменить скорость UART МК и перезапустить приём (модулю команда не отправляется). */
bool purrgo_gnss_raw_set_baud(uint32_t baud);

/*
 * EN = Low (on = false: приём остановлен, LPUART1 выключен, PA2 в Low) или EN = High (on = true: LPUART1 на
 * скорости CubeMX, приём запущен). Повторный вызов с тем же значением ничего не делает.
 */
void purrgo_gnss_raw_en(bool on);

/* Сколько ошибок приёма UART (FE/ORE/NE/PE) было с момента старта (счётчик не сбрасывается). */
uint32_t purrgo_gnss_raw_errors(void);

#endif /* PURRGO_GNSS_RAW_H */
