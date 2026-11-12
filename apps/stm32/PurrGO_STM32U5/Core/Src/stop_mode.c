/*
 * Stop 2 для режима чтения. См. stop_mode.h.
 *
 * Тактирование LPTIM1 - LSI (32 кГц), предделитель 128 -> 250 Гц, 4 мс на тик,
 * 16 бит -> 262 с до переполнения (спим не дольше PURRGO_STOP2_MAX_MS).
 * Счётчик свободно идёт всё время: до сна и после него по разности отсчётов
 * считается, сколько миллисекунд прошло.
 */
#include "stop_mode.h"

#include "main.h"
#include "purrgo/logger.h"

#include <stdbool.h>
#include <stdint.h>

/* Определена в main.c (CubeMX). После Stop 2 PLL выключен: поднимаем заново. */
extern void SystemClock_Config(void);

/* Номинальная частота LSI, Гц (разброс единицы процентов). */
#ifndef PURRGO_STOP_LSI_HZ
#define PURRGO_STOP_LSI_HZ 32000U
#endif

#define LPTIM_PRESC_DIV     128U      /* соответствует LPTIM_CFGR_PRESC = 0b111 */
#define LPTIM_ARR           0xFFFFU
#define LPTIM_MIN_WAKE_TICKS 8U       /* не короче ~32 мс: CMP не должен совпасть с CNT */
#define LPTIM_SYNC_LOOPS    200000U   /* защита от зависания: ~ десятки мс на 80 МГц */

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

extern __IO uint32_t uwTick;   /* stm32u5xx_hal.c */

static IRQn_Type exti_irqn(uint16_t pin)
{
    /* EXTI0..EXTI15_IRQn идут подряд. */
    return (IRQn_Type)((int)EXTI0_IRQn + (int)__builtin_ctz((unsigned)pin));
}

static uint16_t lptim_count(void)
{
    /* Счётчик в асинхронном домене: читать, пока два чтения подряд не совпадут. */
    uint32_t a = LPTIM1->CNT & 0xFFFFU;
    uint32_t b;
    do
    {
        b = a;
        a = LPTIM1->CNT & 0xFFFFU;
    } while (a != b);
    return (uint16_t)a;
}

static bool lptim_wait_flag(uint32_t flag)
{
    uint32_t n = LPTIM_SYNC_LOOPS;
    while (((LPTIM1->ISR & flag) == 0U) && (n != 0U))
    {
        n--;
    }
    return n != 0U;
}

bool purrgo_stop2_init(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* LSI: источник LPTIM1. Остаётся включённым (доли мкА). */
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_LSI;
    osc.LSIState = RCC_LSI_ON;
    osc.LSIDiv = RCC_LSI_DIV1;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        PURRGO_LOGE("Stop2: LSI start failed\r\n");
        return false;
    }

    __HAL_RCC_LPTIM1_CONFIG(RCC_LPTIM1CLKSOURCE_LSI);
    __HAL_RCC_LPTIM1_CLK_ENABLE();
    __HAL_RCC_LPTIM1_CLK_SLEEP_ENABLE();   /* тактирование LPTIM1 в Sleep и Stop */
    __HAL_RCC_LPTIM1_FORCE_RESET();
    __HAL_RCC_LPTIM1_RELEASE_RESET();

    /* После Stop 2 системная частота - HSI16 (ниже поднимается PLL). */
    __HAL_RCC_WAKEUPSTOP_CLK_CONFIG(RCC_STOP_WAKEUPCLOCK_HSI);

#if (PURRGO_STOP_KEEP_DEBUG != 0)
    HAL_DBGMCU_EnableDBGStopMode();
#endif

    /* CFGR, CCMR1 и DIER пишутся только при выключенном LPTIM. */
    LPTIM1->CR = 0U;
    LPTIM1->CFGR = LPTIM_CFGR_PRESC;          /* /128, внутренний тактовый сигнал */
    LPTIM1->CCMR1 = 0U;                       /* канал 1: сравнение (output compare) */
    LPTIM1->DIER = LPTIM_DIER_CC1IE;        /* прерывание сравнения CC1 (NVIC выключен вне сна) */
    LPTIM1->CR = LPTIM_CR_ENABLE;

    LPTIM1->ICR = LPTIM_ICR_ARROKCF;
    LPTIM1->ARR = LPTIM_ARR;
    if (!lptim_wait_flag(LPTIM_ISR_ARROK))
    {
        PURRGO_LOGE("Stop2: LPTIM1 ARR sync timeout\r\n");
        return false;
    }
    LPTIM1->ICR = LPTIM_ICR_ARROKCF;

    LPTIM1->ICR = LPTIM_ICR_CMP1OKCF;
    LPTIM1->CCR1 = 0U;
    if (!lptim_wait_flag(LPTIM_ISR_CMP1OK))
    {
        PURRGO_LOGE("Stop2: LPTIM1 CMP sync timeout\r\n");
        return false;
    }
    LPTIM1->ICR = LPTIM_ICR_CMP1OKCF;

    LPTIM1->CR |= LPTIM_CR_CNTSTRT;           /* свободный счёт */

    s_ready = true;
    PURRGO_LOG("Stop2: ready (LPTIM1 on LSI, %lu Hz nominal)\r\n",
               (unsigned long)(PURRGO_STOP_LSI_HZ / LPTIM_PRESC_DIV));
    return true;
}

/* Кнопки -> EXTI (спад). false - какая-то кнопка уже нажата. */
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

        __HAL_GPIO_EXTI_CLEAR_FALLING_IT(k_wake_pins[i].pin);
        NVIC_ClearPendingIRQ(exti_irqn(k_wake_pins[i].pin));
        NVIC_EnableIRQ(exti_irqn(k_wake_pins[i].pin));
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

/* Вернуть кнопки в обычный вход с подтяжкой (опрос из buttons.c), снять EXTI. */
static void disarm_buttons(void)
{
    GPIO_InitTypeDef gpio = {0};

    for (unsigned i = 0U; i < WAKE_PIN_COUNT; i++)
    {
        NVIC_DisableIRQ(exti_irqn(k_wake_pins[i].pin));

        gpio.Pin = k_wake_pins[i].pin;
        gpio.Mode = GPIO_MODE_INPUT;
        gpio.Pull = GPIO_PULLUP;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(k_wake_pins[i].port, &gpio);   /* заодно очищает конфигурацию EXTI */

        __HAL_GPIO_EXTI_CLEAR_FALLING_IT(k_wake_pins[i].pin);
        NVIC_ClearPendingIRQ(exti_irqn(k_wake_pins[i].pin));
    }
}

/* Запланировать пробуждение через ticks тиков LPTIM1. false - сбой синхронизации. */
static bool arm_timer(uint32_t ticks)
{
    const uint16_t target = (uint16_t)((lptim_count() + ticks) & 0xFFFFU);

    LPTIM1->ICR = LPTIM_ICR_CMP1OKCF;
    LPTIM1->CCR1 = target;
    if (!lptim_wait_flag(LPTIM_ISR_CMP1OK))
    {
        return false;
    }
    LPTIM1->ICR = LPTIM_ICR_CMP1OKCF;

    /* Сбросить старое совпадение и дождаться, пока флаг действительно снят. */
    LPTIM1->ICR = LPTIM_ICR_CC1CF;
    uint32_t n = LPTIM_SYNC_LOOPS;
    while (((LPTIM1->ISR & LPTIM_ISR_CC1IF) != 0U) && (n != 0U))
    {
        n--;
    }

    NVIC_ClearPendingIRQ(LPTIM1_IRQn);
    NVIC_EnableIRQ(LPTIM1_IRQn);
    return true;
}

static void disarm_timer(void)
{
    NVIC_DisableIRQ(LPTIM1_IRQn);
    LPTIM1->ICR = LPTIM_ICR_CC1CF;
    NVIC_ClearPendingIRQ(LPTIM1_IRQn);
}

bool purrgo_stop2_enter(uint32_t max_ms)
{
    if (!s_ready)
    {
        return false;
    }

    const uint32_t ms_per_tick = (LPTIM_PRESC_DIV * 1000U) / PURRGO_STOP_LSI_HZ;   /* 4 */
    uint32_t ticks = max_ms / ms_per_tick;
    if (ticks < LPTIM_MIN_WAKE_TICKS)
    {
        ticks = LPTIM_MIN_WAKE_TICKS;
    }
    if (ticks > (LPTIM_ARR / 2U))
    {
        ticks = LPTIM_ARR / 2U;
    }

    if (!s_logged_first)
    {
        s_logged_first = true;
        PURRGO_LOG("Stop2: first entry, wake period <= %lu ms\r\n", (unsigned long)max_ms);
    }

    __disable_irq();

    if (!arm_buttons() || !arm_timer(ticks))
    {
        disarm_timer();
        disarm_buttons();
        __enable_irq();
        return false;
    }

    const uint16_t t0 = lptim_count();

    HAL_SuspendTick();
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;   /* висящий SysTick не должен сорвать вход в Stop */

    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);

    /* ---- проснулись; PRIMASK всё ещё взведён ---- */
    const uint16_t t1 = lptim_count();

    /* Снять NVIC-линии, пока прерывания выключены: обработчиков у них нет. */
    disarm_timer();
    disarm_buttons();

    /*
     * HAL_RCC_* ждёт по HAL_GetTick(): тик должен идти. SysTick на HSI16 до
     * перенастройки идёт медленнее номинала, для таймаутов это безопасно.
     */
    HAL_ResumeTick();
    __enable_irq();

    SystemClock_Config();   /* PLL 80 МГц, SysTick переинициализируется в HAL_InitTick */

    const uint32_t dticks = (uint32_t)(uint16_t)(t1 - t0);
    const uint32_t elapsed_ms = (uint32_t)(((uint64_t)dticks * LPTIM_PRESC_DIV * 1000U)
                                           / PURRGO_STOP_LSI_HZ);

    /* Время во сне: SysTick стоял, добираем по LPTIM1. */
    __disable_irq();
    uwTick += elapsed_ms;
    __enable_irq();

    return true;
}
