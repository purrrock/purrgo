/*
 * Звуковые сигналы на STM32U585: пьезоизлучатель, два комплементарных канала TIM16
 * (PA6 = TIM16_CH1, PB6 = TIM16_CH1N), см. purrgo/sound.h и docs/hardware.md.
 *
 * Сигнал - короткая последовательность шагов (звук с заданной скважностью или пауза). Проигрыватель
 * неблокирующий: главный цикл вызывает purrgo_sound_stm32_service(), а срок сна берёт у
 * purrgo_sound_stm32_wait_ms(). Пока сигнал звучит, в Stop 2 входить нельзя (таймер в нём не
 * работает): stm32_app.c проверяет purrgo_sound_stm32_busy().
 *
 * Пока CubeMX не сгенерировал TIM16 (нет tim.h или модуль HAL TIM выключен), драйвер собирается
 * заглушкой: сигналы только пишутся в лог.
 */
#ifndef PURRGO_SOUND_STM32_H
#define PURRGO_SOUND_STM32_H

#include <stdbool.h>
#include <stdint.h>

/* Один раз при старте, после MX_TIM16_Init(): выводы в Low, замер частоты таймера. */
void purrgo_sound_stm32_init(void);

/* Вызывать в каждом проходе главного цикла: переход к следующему шагу сигнала. */
void purrgo_sound_stm32_service(uint32_t now_ms);

/* true - сигнал звучит (или ждёт следующего шага): в Stop 2 входить нельзя. */
bool purrgo_sound_stm32_busy(void);

/* Сколько мс до ближайшего шага сигнала; UINT32_MAX - ничего не звучит. */
uint32_t purrgo_sound_stm32_wait_ms(uint32_t now_ms);

#endif /* PURRGO_SOUND_STM32_H */
