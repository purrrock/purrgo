/*
 * Пробуждение по спаду BUSY E-Ink. См. eink_wake.h.
 */
#include "eink_wake.h"

#include "main.h"

_Static_assert(EINK_BUSY_Pin == GPIO_PIN_2,
               "eink_wake.c: EINK_BUSY must be on EXTI line 2 (vector name and IRQn below)");

static void busy_pin_input(void)
{
    GPIO_InitTypeDef gpio = {0};

    /*
     * HAL_GPIO_DeInit() снимает и линию EXTI (HAL_GPIO_Init() в режиме входа триггеры не сбрасывает,
     * а активная линия будила бы МК из Stop при каждом спаде BUSY).
     */
    HAL_GPIO_DeInit(EINK_BUSY_GPIO_Port, EINK_BUSY_Pin);

    gpio.Pin = EINK_BUSY_Pin;                 /* как в CubeMX (gpio.c) */
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(EINK_BUSY_GPIO_Port, &gpio);
}

bool purrgo_eink_busy_wake_arm(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = EINK_BUSY_Pin;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(EINK_BUSY_GPIO_Port, &gpio);

    __HAL_GPIO_EXTI_CLEAR_FALLING_IT(EINK_BUSY_Pin);
    NVIC_ClearPendingIRQ(EXTI2_IRQn);
    NVIC_EnableIRQ(EXTI2_IRQn);

    /* Спад, случившийся до настройки EXTI, не разбудит: уровень проверяется уже после неё. */
    return HAL_GPIO_ReadPin(EINK_BUSY_GPIO_Port, EINK_BUSY_Pin) == GPIO_PIN_SET;
}

void purrgo_eink_busy_wake_disarm(void)
{
    NVIC_DisableIRQ(EXTI2_IRQn);
    busy_pin_input();
    __HAL_GPIO_EXTI_CLEAR_FALLING_IT(EINK_BUSY_Pin);
    NVIC_ClearPendingIRQ(EXTI2_IRQn);
}

/*
 * Вектор нужен, чтобы ожидающее прерывание не ушло в Default_Handler, если оно всё же сработает
 * (disarm() вызывается раньше, чем PRIMASK снимается, и обычно этого не бывает).
 */
void EXTI2_IRQHandler(void)
{
    __HAL_GPIO_EXTI_CLEAR_FALLING_IT(EINK_BUSY_Pin);
}
