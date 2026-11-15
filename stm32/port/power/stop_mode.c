/*
 * Stop 2 для режима чтения. См. stop_mode.h.
 *
 * Будильник и время HAL даёт LPTIM1 (timebase.c): он тактируется от LSI, идёт и в
 * Stop 2, поэтому после сна HAL_GetTick() уже верный, добирать время не нужно.
 */
#include "stop_mode.h"

#include "main.h"
#include "buttons.h"
#include "timebase.h"
#include "purrgo/logger.h"

#include <stdbool.h>
#include <stdint.h>

/* Определена в main.c (CubeMX). После Stop 2 PLL выключен: поднимаем заново. */
extern void SystemClock_Config(void);

/*
 * 1 - оставить отладку включённой в Stop (DBG_STOP): отладчик не отваливается,
 * но ток в Stop 2 больше. Для замеров тока - 0 и питание без отладчика.
 */
#ifndef PURRGO_STOP_KEEP_DEBUG
#define PURRGO_STOP_KEEP_DEBUG 0
#endif

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t      pin;
} wake_pin_t;

/* Кнопки берутся из CubeMX-меток K1..K4 (main.h), как и в buttons.c. */
static const wake_pin_t k_wake_pins[] =
{
    { K1_GPIO_Port, K1_Pin },
    { K2_GPIO_Port, K2_Pin },
    { K3_GPIO_Port, K3_Pin },
    { K4_GPIO_Port, K4_Pin },
};
#define WAKE_PIN_COUNT (sizeof(k_wake_pins) / sizeof(k_wake_pins[0]))

static bool s_ready;
static bool s_logged_first;

static IRQn_Type exti_irqn(uint16_t pin)
{
    /* EXTI0..EXTI15_IRQn идут подряд. */
    return (IRQn_Type)((int)EXTI0_IRQn + (int)__builtin_ctz((unsigned)pin));
}

bool purrgo_stop2_init(void)
{
    /* Будильник и время HAL - LPTIM1 (HAL_InitTick() в timebase.c). */
    if (!purrgo_timebase_ready())
    {
        PURRGO_LOGE("Stop2: LPTIM1 timebase is not running\r\n");
        return false;
    }

    __HAL_RCC_PWR_CLK_ENABLE();

    /* После Stop 2 системная частота - HSI16 (ниже поднимается PLL). */
    __HAL_RCC_WAKEUPSTOP_CLK_CONFIG(RCC_STOP_WAKEUPCLOCK_HSI);

#if (PURRGO_STOP_KEEP_DEBUG != 0)
    HAL_DBGMCU_EnableDBGStopMode();
#endif

    s_ready = true;
    PURRGO_LOG("Stop2: ready (wake timer: LPTIM1 on LSI)\r\n");
    return true;
}

/*
 * Кнопки -> EXTI только по спаду (будит нажатие, а не отпускание). false -
 * какая-то кнопка уже нажата. Линии EXTI и векторы NVIC кнопок включены всегда
 * (CubeMX, оба фронта, см. buttons.c); здесь режим на время сна сужается до
 * спада.
 */
static bool arm_buttons(void)
{
    GPIO_InitTypeDef gpio = {0};

    for (unsigned i = 0U; i < WAKE_PIN_COUNT; i++)
    {
        gpio.Pin = k_wake_pins[i].pin;
        gpio.Mode = GPIO_MODE_IT_FALLING;
        gpio.Pull = GPIO_PULLUP;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(k_wake_pins[i].port, &gpio);

        __HAL_GPIO_EXTI_CLEAR_RISING_IT(k_wake_pins[i].pin);
        __HAL_GPIO_EXTI_CLEAR_FALLING_IT(k_wake_pins[i].pin);
        NVIC_ClearPendingIRQ(exti_irqn(k_wake_pins[i].pin));
    }

    /*
     * Фронт, случившийся до настройки EXTI, не разбудит: проверяем уровень уже
     * после включения EXTI. Прерывания выключены (PRIMASK), гонки нет.
     */
    for (unsigned i = 0U; i < WAKE_PIN_COUNT; i++)
    {
        if (HAL_GPIO_ReadPin(k_wake_pins[i].port, k_wake_pins[i].pin) == GPIO_PIN_RESET)
        {
            return false;
        }
    }
    return true;
}

/*
 * Вернуть кнопкам обычный режим EXTI (оба фронта, векторы NVIC остаются
 * включёнными: их обслуживает buttons.c). Фронт, разбудивший МК, здесь
 * сбрасывается вместе с ожидающим прерыванием (PRIMASK ещё взведён), поэтому
 * драйверу кнопок явно заказывается опрос: он увидит уровень нажатой кнопки и
 * пройдёт debounce.
 */
static void disarm_buttons(void)
{
    GPIO_InitTypeDef gpio = {0};

    for (unsigned i = 0U; i < WAKE_PIN_COUNT; i++)
    {
        gpio.Pin = k_wake_pins[i].pin;
        gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
        gpio.Pull = GPIO_PULLUP;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(k_wake_pins[i].port, &gpio);   /* перенастраивает и линию EXTI */

        __HAL_GPIO_EXTI_CLEAR_RISING_IT(k_wake_pins[i].pin);
        __HAL_GPIO_EXTI_CLEAR_FALLING_IT(k_wake_pins[i].pin);
        NVIC_ClearPendingIRQ(exti_irqn(k_wake_pins[i].pin));
    }

    purrgo_stm32_buttons_request_poll();
}

bool purrgo_stop2_enter(uint32_t max_ms)
{
    if (!s_ready)
    {
        return false;
    }

    if (!s_logged_first)
    {
        s_logged_first = true;
        PURRGO_LOG("Stop2: first entry, wake period <= %lu ms\r\n", (unsigned long)max_ms);
    }

    __disable_irq();

    if (!arm_buttons() || !purrgo_timebase_wake_arm(max_ms))
    {
        purrgo_timebase_wake_disarm();
        disarm_buttons();
        __enable_irq();
        return false;
    }

    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);

    /* ---- проснулись; PRIMASK всё ещё взведён ---- */

    /* Снять NVIC-линии, пока прерывания выключены: настоящих обработчиков у них нет. */
    purrgo_timebase_wake_disarm();
    disarm_buttons();
    __enable_irq();

    /*
     * Время HAL (LPTIM1 на LSI) шло и во сне, добирать его не нужно. Стартовая
     * частота после Stop 2 - HSI16; PLL поднимается заново, HAL_RCC_* ждёт по
     * HAL_GetTick(), а HAL_InitTick() в конце повторный вызов без последствий.
     */
    SystemClock_Config();

    return true;
}

bool purrgo_stop2_power_off(void)
{
    if (!s_ready)
    {
        return false;
    }

    PURRGO_LOG("Stop2: power off, wake by K1 only (then reset)\r\n");

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = K1_Pin;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    __disable_irq();
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;

    /* Будить должна только K1: снять все линии NVIC (таймер LPTIM1 тоже). */
    for (uint32_t i = 0U; i < (sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0])); i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    for (;;)
    {
        HAL_GPIO_Init(K1_GPIO_Port, &gpio);
        __HAL_GPIO_EXTI_CLEAR_FALLING_IT(K1_Pin);
        NVIC_ClearPendingIRQ(exti_irqn(K1_Pin));
        NVIC_EnableIRQ(exti_irqn(K1_Pin));

        /* Спад до настройки EXTI не разбудит: если K1 удерживается, ждём отпускания. */
        if (HAL_GPIO_ReadPin(K1_GPIO_Port, K1_Pin) == GPIO_PIN_RESET)
        {
            for (volatile uint32_t d = 0U; d < 200000U; d++)
            {
            }
            continue;
        }

        __DSB();
        HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
        __ISB();

        /* Разбудил именно спад на K1 - перезапуск (как выход из Standby). */
        if (__HAL_GPIO_EXTI_GET_FALLING_IT(K1_Pin) != 0U)
        {
            NVIC_SystemReset();
        }
        /* Иначе пробуждение ложное: снова спать. */
    }
}
