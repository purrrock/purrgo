#ifndef PURRGO_GNSS_STOP_H
#define PURRGO_GNSS_STOP_H

/*
 * GNSS и Stop 2 (STM32U585).
 *
 * Приём идёт без участия ядра: LPUART1 (ядро тактируется от HSI16) складывает байты через LPDMA1
 * в кольцо в SRAM4. LPUART1, LPDMA1 и SRAM4 включены в автономный режим (RCC_SRDAMR, stop_mode.c),
 * поэтому в Stop 2 приём не прерывается.
 *
 * Пробуждение ядра: прерывание RXNE LPUART1 (пришёл байт; у UART STM32U5 флага WUF нет), оно
 * разрешается только на время сна (purrgo_gnss_stop_arm()). Пачка байтов одной эпохи приёмника
 * длится десятки миллисекунд, а будить на каждый байт нельзя (на выход из Stop 2 уходит поднятие
 * PLL), поэтому после пробуждения МК остаётся в Sleep, пока в кольцо не перестанут поступать данные
 * (purrgo_gnss_stop_ready(): тишина не меньше PURRGO_GNSS_STOP_QUIET_MS), и только потом снова
 * уходит в Stop 2.
 *
 * Вызываются из главного цикла (stop_ready) и из purrgo_stop2_enter() при взведённом PRIMASK
 * (arm/disarm). Пока GNSS выключен по EN (читалка), LPUART1 остановлен: все функции ничего не
 * делают, stop_ready() всегда true.
 */

#include <stdbool.h>
#include <stdint.h>

/*
 * Тишина на входе, после которой можно уходить в Stop 2, мс. Должна быть заметно больше паузы между
 * предложениями NMEA/кадрами UBX одной эпохи (единицы мс при 115200 бод) и не больше периода сна
 * PURRGO_SLEEP_MAX_MS (stm32_app.c): за один сон проверка выполняется ровно один раз.
 */
#ifndef PURRGO_GNSS_STOP_QUIET_MS
#define PURRGO_GNSS_STOP_QUIET_MS 15U
#endif

/* true - GNSS включён по EN (LPUART1 и приём работают). */
bool purrgo_gnss_is_powered(void);

/*
 * true - со стороны GNSS можно уходить в Stop 2: GNSS выключен, либо связь установлена, на входе нет
 * ошибок и необработанных данных, ожидающих таймеров (настройка приёмника) и тишина длится не меньше
 * PURRGO_GNSS_STOP_QUIET_MS. Вызывать из главного цикла на каждом проходе: по ней отслеживается
 * движение кольца.
 */
bool purrgo_gnss_stop_ready(uint32_t now_ms);

/*
 * Взвести пробуждение от приёма (RXNEIE LPUART1) перед входом в Stop 2. PRIMASK взведён. false - в кольце
 * уже есть непрочитанные байты или приём в ошибке: спать нельзя.
 */
bool purrgo_gnss_stop_arm(void);

/* Снять пробуждение от приёма после сна (или после отказа от входа в Stop 2). PRIMASK взведён. */
void purrgo_gnss_stop_disarm(void);

#endif /* PURRGO_GNSS_STOP_H */
