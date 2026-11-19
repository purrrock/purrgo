/*
 * Звуковые сигналы: пьезоизлучатель на TIM16 (CH1 + CH1N), см. sound_stm32.h.
 *
 * Тон - ШИМ частоты PURRGO_SOUND_FREQ_HZ на CH1 и его комплемент на CH1N, поэтому на излучателе
 * между выводами +-3,3 В. Громкость задаёт скважность (hardware_config.h). Между шагами и после
 * сигнала ШИМ останавливается, а оба вывода переводятся в обычный GPIO-выход Low: на излучателе
 * 0 В без постоянной составляющей, ток не течёт (CubeMX-овский режим idle у остановленных каналов
 * не используется, чтобы не зависеть от его тонкостей).
 */
#include "sound_stm32.h"

#include "purrgo/sound.h"
#include "purrgo/hardware_config.h"
#include "purrgo/logger.h"

#include <stddef.h>

#if defined(__has_include)
#  if __has_include("tim.h")
#    include "tim.h"
#  endif
#endif

#if defined(HAL_TIM_MODULE_ENABLED) && defined(TIM16) && defined(__HAL_TIM_SET_AUTORELOAD)
#  define PURRGO_SOUND_HW 1
#else
#  define PURRGO_SOUND_HW 0
#endif

/* Выводы излучателя: метки CubeMX BUZZER_P / BUZZER_N, иначе PA6 / PB6. */
#if PURRGO_SOUND_HW
#  ifndef BUZZER_P_Pin
#    define BUZZER_P_Pin       GPIO_PIN_6
#    define BUZZER_P_GPIO_Port GPIOA
#  endif
#  ifndef BUZZER_N_Pin
#    define BUZZER_N_Pin       GPIO_PIN_6
#    define BUZZER_N_GPIO_Port GPIOB
#  endif
#endif

typedef struct {
    uint16_t ms;     /* длительность шага */
    uint8_t  duty;   /* скважность, %; 0 - пауза */
} sound_step_t;

#define STEP(ms_, duty_) { (uint16_t)(ms_), (uint8_t)(duty_) }

static const sound_step_t k_error[]   = { STEP(PURRGO_SOUND_ERROR_MS, PURRGO_SOUND_DUTY_LOUD_PCT) };
static const sound_step_t k_arrived[] = {
    STEP(PURRGO_SOUND_ARRIVED_MS,     PURRGO_SOUND_DUTY_LOUD_PCT),
    STEP(PURRGO_SOUND_ARRIVED_GAP_MS, 0),
    STEP(PURRGO_SOUND_ARRIVED_MS,     PURRGO_SOUND_DUTY_LOUD_PCT),
};
static const sound_step_t k_info[]    = { STEP(PURRGO_SOUND_INFO_MS,  PURRGO_SOUND_DUTY_MID_PCT) };
static const sound_step_t k_click[]   = { STEP(PURRGO_SOUND_CLICK_MS, PURRGO_SOUND_DUTY_QUIET_PCT) };

typedef struct {
    const sound_step_t* steps;
    uint8_t             count;
} sound_pattern_t;

#define PATTERN(a) { (a), (uint8_t)(sizeof(a) / sizeof((a)[0])) }

static const sound_pattern_t k_patterns[PURRGO_SOUND_COUNT] = {
    [PURRGO_SOUND_CLICK]   = PATTERN(k_click),
    [PURRGO_SOUND_INFO]    = PATTERN(k_info),
    [PURRGO_SOUND_ARRIVED] = PATTERN(k_arrived),
    [PURRGO_SOUND_ERROR]   = PATTERN(k_error),
};

static struct {
    purrgo_sound_t sound;      /* PURRGO_SOUND_NONE - ничего не звучит */
    uint8_t        step;
    uint32_t       step_end_ms;
    bool           pwm_on;
    bool           ready;
    uint32_t       tick_hz;    /* частота счёта таймера после предделителя */
} s_snd;


#if PURRGO_SOUND_HW

/* Частота тактирования TIM16 (APB2): при предделителе APB2 не 1 таймеры работают на удвоенной. */
static uint32_t timer_clock_hz(void)
{
    const uint32_t ppre2 = (RCC->CFGR2 & RCC_CFGR2_PPRE2) >> RCC_CFGR2_PPRE2_Pos;   /* 0..3 - без деления */
    const uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();

    return (ppre2 < 4U) ? pclk2 : (pclk2 * 2U);
}

/* Оба вывода - обычный выход в Low: на излучателе 0 В. */
static void pins_low(void)
{
    GPIO_InitTypeDef gpio = {0};

    HAL_GPIO_WritePin(BUZZER_P_GPIO_Port, BUZZER_P_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BUZZER_N_GPIO_Port, BUZZER_N_Pin, GPIO_PIN_RESET);

    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = BUZZER_P_Pin;
    HAL_GPIO_Init(BUZZER_P_GPIO_Port, &gpio);
    gpio.Pin = BUZZER_N_Pin;
    HAL_GPIO_Init(BUZZER_N_GPIO_Port, &gpio);
}

static void pwm_stop(void)
{
    if (!s_snd.pwm_on) {
        return;
    }
    (void)HAL_TIMEx_PWMN_Stop(&htim16, TIM_CHANNEL_1);
    (void)HAL_TIM_PWM_Stop(&htim16, TIM_CHANNEL_1);
    pins_low();
    s_snd.pwm_on = false;
}

static void pwm_start(uint8_t duty_pct)
{
    uint32_t arr = (s_snd.tick_hz / PURRGO_SOUND_FREQ_HZ);

    if (arr < 2U) {
        arr = 2U;
    }
    if (arr > 65536U) {
        arr = 65536U;
    }
    uint32_t ccr = (arr * (uint32_t)duty_pct) / 100U;
    if (ccr == 0U) {
        ccr = 1U;
    }

    pwm_stop();

    __HAL_TIM_SET_AUTORELOAD(&htim16, arr - 1U);
    __HAL_TIM_SET_COMPARE(&htim16, TIM_CHANNEL_1, ccr);
    __HAL_TIM_SET_COUNTER(&htim16, 0U);
    htim16.Instance->EGR = TIM_EGR_UG;   /* значения из теневых регистров в рабочие */

    /* Выводы обратно в альтернативную функцию TIM16 (CubeMX: HAL_TIM_MspPostInit). */
    HAL_TIM_MspPostInit(&htim16);

    (void)HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1);
    (void)HAL_TIMEx_PWMN_Start(&htim16, TIM_CHANNEL_1);
    s_snd.pwm_on = true;
}

#else   /* TIM16 ещё не настроен в CubeMX: заглушка */

static void pwm_stop(void)             { s_snd.pwm_on = false; }
static void pwm_start(uint8_t duty)    { (void)duty; s_snd.pwm_on = true; }

#endif


static void sound_finish(void)
{
    pwm_stop();
    s_snd.sound = PURRGO_SOUND_NONE;
}

static void sound_begin_step(uint32_t now_ms)
{
    const sound_pattern_t* pat = &k_patterns[s_snd.sound];
    const sound_step_t* step = &pat->steps[s_snd.step];

    s_snd.step_end_ms = now_ms + step->ms;
    if (step->duty != 0U) {
        pwm_start(step->duty);
    } else {
        pwm_stop();
    }
}


void purrgo_sound_stm32_init(void)
{
#if PURRGO_SOUND_HW
    const uint32_t clk = timer_clock_hz();
    const uint32_t psc = (uint32_t)htim16.Instance->PSC + 1U;

    s_snd.tick_hz = clk / psc;
    pins_low();
    PURRGO_LOG("Sound: TIM16 %lu Hz tick, tone %u Hz\r\n",
               (unsigned long)s_snd.tick_hz, (unsigned)PURRGO_SOUND_FREQ_HZ);
    s_snd.ready = (s_snd.tick_hz >= 2U * PURRGO_SOUND_FREQ_HZ);
    if (!s_snd.ready) {
        PURRGO_LOGE("Sound: TIM16 prescaler too large for the tone, sound disabled\r\n");
    }
#else
    PURRGO_LOG("Sound: TIM16 is not configured in CubeMX, sound is a stub\r\n");
    s_snd.ready = true;
#endif
    s_snd.sound = PURRGO_SOUND_NONE;
    s_snd.pwm_on = false;
}

void purrgo_platform_sound_play(purrgo_sound_t sound)
{
    if (!s_snd.ready || sound <= PURRGO_SOUND_NONE || sound >= PURRGO_SOUND_COUNT) {
        return;
    }
    /* Приоритет = порядок в purrgo_sound_t: менее важный сигнал не прерывает более важный. */
    if (s_snd.sound != PURRGO_SOUND_NONE && sound < s_snd.sound) {
        return;
    }

    PURRGO_LOG("Sound: %s\r\n", purrgo_sound_name(sound));
#if !PURRGO_SOUND_HW
    return;   /* заглушка: только лог */
#else
    s_snd.sound = sound;
    s_snd.step = 0U;
    sound_begin_step(HAL_GetTick());
#endif
}

void purrgo_sound_stm32_service(uint32_t now_ms)
{
    if (s_snd.sound == PURRGO_SOUND_NONE) {
        return;
    }
    if ((int32_t)(now_ms - s_snd.step_end_ms) < 0) {
        return;
    }

    const sound_pattern_t* pat = &k_patterns[s_snd.sound];

    s_snd.step++;
    if (s_snd.step >= pat->count) {
        sound_finish();
        return;
    }
    sound_begin_step(s_snd.step_end_ms);   /* от расчётного конца шага: пауза не накапливает дрейф */
}

bool purrgo_sound_stm32_busy(void)
{
    return s_snd.sound != PURRGO_SOUND_NONE;
}

uint32_t purrgo_sound_stm32_wait_ms(uint32_t now_ms)
{
    if (s_snd.sound == PURRGO_SOUND_NONE) {
        return UINT32_MAX;
    }
    const int32_t left = (int32_t)(s_snd.step_end_ms - now_ms);

    return (left <= 0) ? 0U : (uint32_t)left;
}
