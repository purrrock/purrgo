/*
 * Платформенные хуки выключения STM32. Последовательность шагов (экран,
 * логгер, конфиг, карта, дисплей) живёт в ядре: purrgo_system_power_off()
 * в src/core/power.c.
 */
#include "purrgo/power.h"
#include "purrgo/logger.h"
#include "main.h"
#include "fatfs.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_pwr.h"

void purrgo_platform_storage_unmount(void)
{
    /* Все метаданные записаны, карта в безопасном состоянии. */
    f_mount(NULL, "0:", 1);
    PURRGO_LOG("FatFs dismount\r\n");
}

void purrgo_platform_enter_standby(void)
{
    /* Ensure Wakeup pin is enabled to allow turning back on (usually WKUP pin, PA0). */
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);

    /* Clear the Wakeup flag if already set. */
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

    /* Enter STANDBY mode. */
    PURRGO_LOG("Entering STANDBY mode.\n\r");
    HAL_PWR_EnterSTANDBYMode();

    /* Device should reset upon wakeup. */
    while (1) {
    }
}
