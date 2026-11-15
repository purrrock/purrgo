/*
 * Таймбейз HAL на LPTIM1 (LSI). См. timebase.h.
 *
 * Регистры LPTIM1 пишутся напрямую, без HAL_LPTIM: HAL_InitTick() вызывается из
 * HAL_Init() до SystemClock_Config() (МК тогда на MSI 4 МГц), а модуль HAL_LPTIM
 * в проекте выключен.
 *
 * Тактирование LPTIM1 - LSI 32 кГц, предделитель /32 -> 1000 Гц. Счётчик 16 бит,
 * ARR = 0xFFFF, считает свободно с момента старта. Старшие 16 бит времени
 * набираются в HAL_GetTick() по факту переполнения (см. PURRGO_TIMEBASE_MAX_GAP_MS).
 *
 * Функции HAL (HAL_InitTick, HAL_GetTick, HAL_SuspendTick, HAL_ResumeTick) в
 * stm32u5xx_hal.c объявлены __weak, здесь они переопределены. HAL_Delay() остаётся
 * штатной: она ждёт по HAL_GetTick().
 */
#include "timebase.h"

#include "main.h"

#include <stdbool.h>
#include <stdint.h>

/* Номинальная частота LSI, Гц (разброс единицы процентов). */
#define TB_LSI_HZ          32000U
/* LPTIM_CFGR.PRESC = 0b101 -> /32. */
#define TB_PRESC_DIV       32U
#define TB_PRESC_BITS      (5UL << LPTIM_CFGR_PRESC_Pos)

#if ((TB_LSI_HZ / TB_PRESC_DIV) != 1000U)
#error "LPTIM1 timebase must tick at exactly 1 kHz (LSI 32 kHz / 32)"
#endif

#define TB_ARR             0xFFFFU
/* Наибольший будильник: половина периода счётчика, чтобы отличить "ещё не пришло" от "уже прошло". */
#define TB_WAKE_MAX_TICKS  0x7FFFU
/* Защита от зависания в ожидании флага (на MSI 4 МГц это десятки-сотни мс). */
#define TB_SYNC_LOOPS      200000U

static volatile bool s_running;
static uint16_t      s_last_cnt;   /* последнее прочитанное значение счётчика */
static uint32_t      s_epoch;      /* число переполнений счётчика (старшие 16 бит времени) */

/* Счётчик в асинхронном домене: читать, пока два чтения подряд не совпадут. */
static uint16_t lptim_count(void)
{
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
    uint32_t n = TB_SYNC_LOOPS;
    while (((LPTIM1->ISR & flag) == 0U) && (n != 0U))
    {
        n--;
    }
    return n != 0U;
}

/* Включить LSI (повторяет HAL_RCC_OscConfig для LSI, но без HAL_GetTick: тика ещё нет). */
static bool lsi_start(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();

    /* Регистр RCC->BDCR под защитой записи домена Backup. */
    SET_BIT(PWR->DBPR, PWR_DBPR_DBP);
    uint32_t n = TB_SYNC_LOOPS;
    while (((PWR->DBPR & PWR_DBPR_DBP) == 0U) && (n != 0U))
    {
        n--;
    }
    if (n == 0U)
    {
        return false;
    }

    /* Делитель LSI /128 дал бы 250 Гц вместо 32 кГц: таймбейз тогда шёл бы в 128 раз медленнее. */
    if ((RCC->BDCR & RCC_BDCR_LSIPREDIV) != 0U)
    {
        return false;
    }

    __HAL_RCC_LSI_ENABLE();
    n = TB_SYNC_LOOPS;
    while (((RCC->BDCR & RCC_BDCR_LSIRDY) == 0U) && (n != 0U))
    {
        n--;
    }
    return n != 0U;
}

static bool lptim_start(void)
{
    __HAL_RCC_LPTIM1_CONFIG(RCC_LPTIM1CLKSOURCE_LSI);
    __HAL_RCC_LPTIM1_CLK_ENABLE();
    __HAL_RCC_LPTIM1_CLK_SLEEP_ENABLE();   /* тактирование LPTIM1 в Sleep и Stop */
    __HAL_RCC_LPTIM1_FORCE_RESET();
    __HAL_RCC_LPTIM1_RELEASE_RESET();

    /* CFGR, CCMR1 и DIER пишутся только при выключенном LPTIM. */
    LPTIM1->CR = 0U;
    LPTIM1->CFGR = TB_PRESC_BITS;             /* /32, внутренний тактовый сигнал */
    LPTIM1->CCMR1 = 0U;                       /* канал 1: сравнение (output compare) */
    LPTIM1->DIER = LPTIM_DIER_CC1IE;          /* прерывание сравнения CC1 (линия NVIC выключена вне сна) */
    LPTIM1->CR = LPTIM_CR_ENABLE;

    LPTIM1->ICR = LPTIM_ICR_ARROKCF;
    LPTIM1->ARR = TB_ARR;
    if (!lptim_wait_flag(LPTIM_ISR_ARROK))
    {
        return false;
    }
    LPTIM1->ICR = LPTIM_ICR_ARROKCF;

    LPTIM1->ICR = LPTIM_ICR_CMP1OKCF;
    LPTIM1->CCR1 = 0U;
    if (!lptim_wait_flag(LPTIM_ISR_CMP1OK))
    {
        return false;
    }
    LPTIM1->ICR = LPTIM_ICR_CMP1OKCF;

    LPTIM1->CR |= LPTIM_CR_CNTSTRT;           /* свободный счёт */
    return true;
}

/* ------------------------- переопределения HAL --------------------------- */

/*
 * Вызывается из HAL_Init() и из HAL_RCC_ClockConfig() (после каждой смены частоты,
 * в том числе после выхода из Stop 2). Счётчик тактируется от LSI и от частоты ядра
 * не зависит, поэтому повторный вызов ничего не делает, а счёт не сбрасывает.
 */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    uwTickPrio = TickPriority;   /* HAL_RCC_ClockConfig() передаёт его обратно */

    if (s_running)
    {
        return HAL_OK;
    }

    if (!lsi_start() || !lptim_start())
    {
        return HAL_ERROR;
    }

    s_epoch = 0U;
    s_last_cnt = lptim_count();
    s_running = true;
    return HAL_OK;
}

/*
 * Время в миллисекундах. Безопасна из прерываний: PRIMASK на время обновления
 * s_epoch/s_last_cnt (несколько микросекунд).
 */
uint32_t HAL_GetTick(void)
{
    if (!s_running)
    {
        return 0U;   /* до HAL_InitTick() тактирование LPTIM1 ещё выключено */
    }

    const uint32_t primask = __get_PRIMASK();
    __disable_irq();

    const uint16_t cnt = lptim_count();
    if (cnt < s_last_cnt)
    {
        s_epoch++;   /* счётчик перешёл через 0xFFFF */
    }
    s_last_cnt = cnt;
    const uint32_t tick = (s_epoch << 16) | cnt;

    __set_PRIMASK(primask);
    return tick;
}

/* SysTick не используется: останавливать и возобновлять нечего. */
void HAL_SuspendTick(void)
{
}

void HAL_ResumeTick(void)
{
}

/* ------------------------------ будильник -------------------------------- */

bool purrgo_timebase_ready(void)
{
    return s_running;
}

bool purrgo_timebase_wake_arm(uint32_t ms)
{
    if (!s_running)
    {
        return false;
    }

    uint32_t ticks = ms;   /* 1 тик = 1 мс */
    if (ticks < 1U)
    {
        ticks = 1U;
    }
    if (ticks > TB_WAKE_MAX_TICKS)
    {
        ticks = TB_WAKE_MAX_TICKS;
    }

    const uint16_t start = lptim_count();

    LPTIM1->ICR = LPTIM_ICR_CMP1OKCF;
    LPTIM1->CCR1 = (uint16_t)(start + ticks);
    if (!lptim_wait_flag(LPTIM_ISR_CMP1OK))
    {
        return false;
    }
    LPTIM1->ICR = LPTIM_ICR_CMP1OKCF;

    /* Сбросить старое совпадение и дождаться, пока флаг действительно снят. */
    LPTIM1->ICR = LPTIM_ICR_CC1CF;
    uint32_t n = TB_SYNC_LOOPS;
    while (((LPTIM1->ISR & LPTIM_ISR_CC1IF) != 0U) && (n != 0U))
    {
        n--;
    }

    NVIC_ClearPendingIRQ(LPTIM1_IRQn);
    NVIC_EnableIRQ(LPTIM1_IRQn);

    /*
     * Пока новое значение CCR1 синхронизировалось в домен LSI, счётчик мог
     * проскочить его: совпадения уже не будет (следующее - через 65 с). Спать нельзя.
     */
    if ((uint32_t)(uint16_t)(lptim_count() - start) >= ticks)
    {
        purrgo_timebase_wake_disarm();
        return false;
    }
    return true;
}

void purrgo_timebase_wake_disarm(void)
{
    NVIC_DisableIRQ(LPTIM1_IRQn);
    LPTIM1->ICR = LPTIM_ICR_CC1CF;
    NVIC_ClearPendingIRQ(LPTIM1_IRQn);
}

/*
 * Линия LPTIM1 включается только на время сна при взведённом PRIMASK, обработчик
 * в штатной работе не вызывается. Он нужен, чтобы случайное прерывание не попало в
 * Default_Handler (бесконечный цикл): просто снимает флаг сравнения.
 */
void LPTIM1_IRQHandler(void)
{
    LPTIM1->ICR = LPTIM_ICR_CC1CF;
}
