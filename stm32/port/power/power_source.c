/*
 * Определение питания 5 В через модуль ORing LM66200 (purrgo/battery.h:
 * purrgo_platform_ext_power_present()).
 *
 * Вывод ST модуля подключён к PA8 (метка PWR_SRC, GPIO_Input/EXTI, без подтяжки):
 * высокий уровень - открыт вход VI1 (батареи), низкий - вход VI2 (5 В от отдельного
 * USB-C). Подробности и ограничения - purrgo/hardware_config.h (PURRGO_HW_ORING_PRESENT)
 * и docs/hardware.md, раздел «Питание».
 *
 * Прерывание EXTI8 нужно только для пробуждения из Stop 2: после пробуждения главный
 * цикл сам опрашивает уровень через purrgo_battery_monitor_tick(), флагов в обработчике
 * не нужно. В POWER OFF (purrgo_stop2_power_off) все линии NVIC сняты, кроме K1, и
 * смена источника прибор не будит.
 */
#include "purrgo/battery.h"
#include "purrgo/hardware_config.h"
#include "main.h"

bool purrgo_platform_ext_power_present(void)
{
#if (PURRGO_HW_ORING_PRESENT != 0)
    return HAL_GPIO_ReadPin(PWR_SRC_GPIO_Port, PWR_SRC_Pin) == GPIO_PIN_RESET;
#else
    return false;
#endif
}
