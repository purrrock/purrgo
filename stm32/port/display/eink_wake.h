#ifndef PURRGO_EINK_WAKE_H
#define PURRGO_EINK_WAKE_H

/*
 * Пробуждение МК по спаду BUSY E-Ink (PB2, EXTI2).
 *
 * Пока панель обновляется (секунды), epd4g ждёт BUSY. Вместо опроса раз в миллисекунду МК спит (Sleep
 * или Stop 2), а будит его спад BUSY или будильник на срок из display_stm32_wait_ms(). Линия
 * настраивается из кода на время сна: CubeMX держит PB2 обычным входом, менять его не нужно (вектор
 * EXTI2_IRQHandler определён здесь, в CubeMX это прерывание не включено).
 *
 * Обе функции вызываются при взведённом PRIMASK, в паре с WFI.
 */

#include <stdbool.h>

/*
 * Включить EXTI по спаду BUSY. false - BUSY уже низкий (панель готова): фронт упущен, спать нельзя,
 * нужен опрос. После любого результата вызвать purrgo_eink_busy_wake_disarm().
 */
bool purrgo_eink_busy_wake_arm(void);

/* Вернуть PB2 в обычный вход и погасить EXTI2 (в том числе ожидающее прерывание). */
void purrgo_eink_busy_wake_disarm(void);

#endif /* PURRGO_EINK_WAKE_H */
