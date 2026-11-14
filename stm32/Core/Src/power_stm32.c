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

    /*
     * GNSS_EN (PB0, Push-Pull, без подтяжки в Run): в Standby вывод без
     * PWR-подтяжки уходит в Hi-Z, и внутренняя подтяжка модуля снова включила
     * бы GNSS. PWR удерживает PB0 в Low. Проверить на железе: подтяжка PWR
     * должна пересиливать подтяжку модуля.
     */
    (void)HAL_PWREx_EnableGPIOPullDown(PWR_GPIO_B, PWR_GPIO_BIT_0);
    HAL_PWREx_EnablePullUpPullDownConfig();

    /*
     * Диагностика EN (TODO.md: «Удержание EN в Low на время Standby МК»):
     * убедиться, что PWR-подтяжка PB0 действительно включена перед WFI.
     * Ожидается: PDCRB.0 = 1, PUCRB.0 = 0, APC = 1, ODR.0 = 0.
     */
    PURRGO_LOG("STBY: PWR PDCRB.0=%u PUCRB.0=%u APC=%u, GPIOB ODR.0=%u\r\n",
               (unsigned)(PWR->PDCRB & 1U), (unsigned)(PWR->PUCRB & 1U),
               (unsigned)((PWR->APCR & PWR_APCR_APC) ? 1U : 0U),
               (unsigned)(GPIOB->ODR & 1U));

    /*
     * Ждать отпускания K1: пробуждение - по спаду на PA0, а вход в Standby с
     * нажатой кнопкой (меню выбирается кнопкой) не даёт ни спада, ни понятного
     * поведения. Таймаут 3 с, чтобы застрявшая кнопка не повесила выключение.
     */
    {
        const uint32_t t0 = HAL_GetTick();
        while ((HAL_GPIO_ReadPin(K1_GPIO_Port, K1_Pin) == GPIO_PIN_RESET) &&
               ((HAL_GetTick() - t0) < 3000U))
        {
        }
        HAL_Delay(30U);   /* дребезг отпускания */
    }

    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1_LOW_0);

    /*
     * Сбросить ВСЕ флаги пробуждения (WUF1..8): неснятый флаг оставляет линию
     * пробуждения активной, и следующий спад на PA0 не даст нового события.
     */
    WRITE_REG(PWR->WUSCR, PWR_WUSCR_CWUF);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SBF);

    /*
     * Любое прерывание, ожидающее в NVIC (или SysTick 1 кГц), обрывает WFI:
     * МК остаётся в Run и падает в бесконечный цикл ниже - K1 его уже не
     * разбудит, помогает только отключение батареи. Поэтому перед WFI снимаем
     * тик и все линии NVIC (в Standby обработчики не нужны, после пробуждения
     * идёт полный перезапуск).
     */
    __disable_irq();
    HAL_SuspendTick();
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    for (uint32_t i = 0U; i < (sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0])); i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    PURRGO_LOG("Entering STANDBY mode.\n\r");

    /*
     * WFI не должен возвращаться. Если вернулся, Standby не был принят:
     * один раз пишем причину (ожидающие IRQ и VECTPENDING) и пробуем снова,
     * а не уходим в пустой цикл с работающим ядром.
     */
    for (uint32_t attempt = 0U;; attempt++)
    {
        __DSB();
        HAL_PWR_EnterSTANDBYMode();
        __ISB();

        if (attempt == 0U)
        {
            PURRGO_LOG("STBY: WFI returned, not in Standby: ICSR=0x%08lX ISPR0=0x%08lX "
                       "ISPR1=0x%08lX PWR.SR=0x%08lX\r\n",
                       (unsigned long)SCB->ICSR, (unsigned long)NVIC->ISPR[0],
                       (unsigned long)NVIC->ISPR[1], (unsigned long)PWR->SR);
        }
        SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
        for (uint32_t i = 0U; i < (sizeof(NVIC->ICPR) / sizeof(NVIC->ICPR[0])); i++)
        {
            NVIC->ICPR[i] = 0xFFFFFFFFU;
        }
    }
}
