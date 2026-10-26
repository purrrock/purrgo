/*
 * Платформенные хуки выключения STM32U585. Последовательность шагов (экран,
 * логгер, конфиг, карта, дисплей) живёт в ядре: purrgo_system_power_off()
 * в src/core/power.c.
 */
#include "purrgo/power.h"
#include "purrgo/logger.h"
#include "main.h"
#include "ff.h"

void purrgo_platform_storage_unmount(void)
{
    /* Все метаданные записаны, карта в безопасном состоянии. */
    f_mount(NULL, "0:", 1);
    PURRGO_LOG("FatFs dismount\r\n");
}

void purrgo_platform_enter_standby(void)
{
    /*
     * Пробуждение кнопкой K1 (PA0 = WKUP1): кнопка замыкает вывод на GND,
     * поэтому активный уровень - низкий. Подтяжки к питанию на плате нет,
     * в Standby внутренняя подтяжка сбрасывается, если её не сохранить:
     * без PU/PD-конфигурации PWR вывод плавал бы и будил МК случайно.
     */
    (void)HAL_PWREx_EnableGPIOPullUp(PWR_GPIO_A, PWR_GPIO_BIT_0);
    HAL_PWREx_EnablePullUpPullDownConfig();

    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1_LOW_0);

    /* Сбросить флаг пробуждения, если он уже установлен. */
    __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG1);

    PURRGO_LOG("Entering STANDBY mode.\n\r");
    HAL_PWR_EnterSTANDBYMode();

    /* После пробуждения МК перезапускается. */
    while (1) {
    }
}
