/*
 * Контроль напряжения VDD (после TPS63802) через PVD STM32U585. См. supply_stm32.h.
 *
 * Прерывание не используется: ядро опрашивает флаг PVDO в каждом проходе
 * главного цикла (purrgo_app_tick(), период около 1 мс), а гистерезис уже есть
 * в самом PVD. EXTI/NVIC и CubeMX не затрагиваются.
 *
 * PVDO = 1: VDD ниже порога PVDLS (RM0456, регистр PWR_SVMSR).
 */
#include "supply_stm32.h"

#include "purrgo/supply.h"
#include "purrgo/logger.h"
#include "main.h"

#include <stdbool.h>

#ifndef PURRGO_SUPPLY_PVD_LEVEL
#define PURRGO_SUPPLY_PVD_LEVEL PWR_PVDLEVEL_5
#endif

static bool s_ready;

void purrgo_supply_stm32_init(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();

    PWR_PVDTypeDef cfg = {
        .PVDLevel = PURRGO_SUPPLY_PVD_LEVEL,
        .Mode = PWR_PVD_MODE_NORMAL,    /* без прерывания и события: флаг опрашивается */
    };

    if (HAL_PWR_ConfigPVD(&cfg) != HAL_OK)
    {
        PURRGO_LOGE("Supply: PVD config failed\r\n");
        return;
    }
    HAL_PWR_EnablePVD();

    /* Время запуска компаратора - единицы микросекунд; 1 мс (тик HAL) с запасом. */
    HAL_Delay(1U);

    s_ready = true;
    PURRGO_LOG("Supply: PVD level 0x%08lX, VDD %s threshold\r\n",
               (unsigned long)PURRGO_SUPPLY_PVD_LEVEL,
               purrgo_platform_supply_low() ? "below" : "above");
}

bool purrgo_platform_supply_low(void)
{
    if (!s_ready)
    {
        return false;
    }
    return (PWR->SVMSR & PWR_SVMSR_PVDO) != 0U;
}
