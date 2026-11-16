/*
 * Stop 2 (STM32U585) в простое: читалка, карта, путевой компьютер, трекер.
 *
 * Пока экран статичен, МК уходит в Stop 2 (единицы мкА вместо ~8 мА в Sleep на 80 МГц). Решение о
 * входе принимает stm32_app.c: приложение в покое (purrgo_app_deep_sleep_allowed()), E-Ink не
 * требует опроса ближайшие мс (display_stm32_wait_ms()), кнопки отпущены (purrgo_stm32_buttons_idle()) и на входе GNSS
 * тишина (purrgo_gnss_stop_ready(), gnss_stop.h).
 *
 * Время HAL (HAL_GetTick) - свободно идущий счётчик LPTIM1 на LSI (timebase.h): он работает и в
 * Stop 2, поэтому таймауты ядра (сохранение позиции, опрос батареи, PVD, 1-секундный такт GNSS)
 * остаются корректными без какого-либо добора времени после сна. Точность хода - точность LSI
 * (единицы процентов): для таймаутов в секунды и десятки секунд достаточно.
 *
 * Приём GNSS идёт во сне сам (LPUART1 + LPDMA1 + SRAM4 в автономном режиме).
 *
 * Пробуждение: кнопки K1..K4 (EXTI, спад), первый байт от GNSS (RXNE LPUART1), ошибка приёма,
 * заполнение половины/всего буфера LPDMA1 (редкий запасной случай), спад BUSY E-Ink (по запросу) или
 * будильник LPTIM1 (max_ms).
 */
#ifndef PURRGO_STOP_MODE_H
#define PURRGO_STOP_MODE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Подготовить Stop 2 (LPTIM1 уже запущен как таймбейз HAL, см. timebase.h).
 * Вызывается один раз при старте. false - таймбейз не идёт, Stop 2 недоступен,
 * purrgo_stop2_enter() вернёт false.
 */
bool purrgo_stop2_init(void);

/*
 * Войти в Stop 2 не более чем на max_ms. Возвращает после пробуждения (кнопка, приём GNSS
 * или таймаут) с восстановленной тактовой частотой. false - в Stop не входили (кнопка уже нажата,
 * в кольце GNSS есть непрочитанные байты, wake_on_busy и BUSY уже низкий или Stop 2 недоступен):
 * вызывающий должен выполнить обычный WFI.
 *
 * wake_on_busy: будить ещё и спадом BUSY E-Ink (eink_wake.h), когда задание библиотеки epd4g ждёт панель.
 */
bool purrgo_stop2_enter(uint32_t max_ms, bool wake_on_busy);

/*
 * POWER OFF через Stop 2 вместо Standby (замеры: Standby 5,17 мА против 2,75 мА
 * в Stop 2 при той же плате). Будит только K1 (спад), после пробуждения -
 * NVIC_SystemReset(): запуск как после Standby. При успехе не возвращается.
 * false - Stop 2 недоступен (purrgo_stop2_init() не вызывался или завершился
 * ошибкой): вызывающий должен перейти в Standby.
 */
bool purrgo_stop2_power_off(void);

#endif /* PURRGO_STOP_MODE_H */
