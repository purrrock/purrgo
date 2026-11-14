/*
 * Измерение напряжения батареи на STM32U585: ADC1_IN16 (PB1, V_MAIN_BAT),
 * резисторный делитель + конденсатор 100 нФ (см. purrgo/hardware_config.h).
 * Реализует платформенный хук purrgo_platform_battery_pin_mv() (purrgo/battery.h):
 * возвращает напряжение на выводе PB1 в мВ, пересчёт по делителю и процент
 * делает ядро (src/core/battery.c).
 *
 * Как измеряется:
 *   - опорой АЦП служит питание VDDA (3,3 В от TPS63802 или платы), оно не
 *     точное, поэтому перед измерением батареи измеряется внутренний канал
 *     VREFINT и по заводской калибровке вычисляется реальное VDDA;
 *   - на каждый канал по BATT_SAMPLES преобразований, берётся среднее;
 *   - время выборки максимальное: источник - делитель на мегаомных резисторах;
 *   - между измерениями АЦП полностью обесточен: HAL_ADC_DeInit() выключает
 *     регулятор АЦП (ADVREGEN=0, DEEPPWD=1), сбрасывает VREFEN (источник
 *     VREFINT) и тактирование ADC12; поэтому перед каждым измерением
 *     повторяется HAL_ADC_Init() и калибровка (в deep power-down она теряется);
 *   - измерение не чаще PURRGO_HW_BATT_PERIOD_MS, между ними возвращается
 *     последнее значение (вызывают из отрисовки строки состояния и логгера).
 */
#include "purrgo/battery.h"
#include "purrgo/hardware_config.h"
#include "purrgo/logger.h"
#include "adc.h"
#include "main.h"

#include <stdbool.h>
#include <stdint.h>

#define BATT_SAMPLES        16u
#define BATT_POLL_TIMEOUT   10u     /* мс на одно преобразование */

/* Допустимый диапазон VDDA, мВ: вне него VREFINT считается неисправным. */
#define BATT_VDDA_MIN_MV    2000u
#define BATT_VDDA_MAX_MV    3600u

static bool     s_have_value;
static uint16_t s_last_pin_mv;
static uint32_t s_last_tick_ms;

/*
 * Среднее BATT_SAMPLES преобразований канала channel (14 бит).
 * ADC1 после вызова выключен.
 */
static bool adc_read_average(uint32_t channel, uint32_t* out_avg)
{
    ADC_ChannelConfTypeDef cfg = {0};
    cfg.Channel      = channel;
    cfg.Rank         = ADC_REGULAR_RANK_1;
    cfg.SamplingTime = ADC_SAMPLETIME_814CYCLES;
    cfg.SingleDiff   = ADC_SINGLE_ENDED;
    cfg.OffsetNumber = ADC_OFFSET_NONE;
    cfg.Offset       = 0;

    if (HAL_ADC_ConfigChannel(&hadc1, &cfg) != HAL_OK) {
        return false;
    }

    uint32_t sum = 0;
    bool ok = true;

    for (uint32_t i = 0; i < BATT_SAMPLES; i++) {
        if (HAL_ADC_Start(&hadc1) != HAL_OK ||
            HAL_ADC_PollForConversion(&hadc1, BATT_POLL_TIMEOUT) != HAL_OK) {
            ok = false;
            break;
        }
        sum += HAL_ADC_GetValue(&hadc1);
    }

    (void)HAL_ADC_Stop(&hadc1);

    if (ok) {
        *out_avg = sum / BATT_SAMPLES;
    }
    return ok;
}

/*
 * Одно измерение с АЦП, который уже проинициализирован и откалиброван:
 * напряжение на PB1, мВ; 0 при ошибке.
 */
static uint16_t measure_pin_mv_adc_on(void)
{
    uint32_t vref_raw = 0;
    uint32_t bat_raw = 0;

    if (!adc_read_average(ADC_CHANNEL_VREFINT, &vref_raw) || vref_raw == 0u ||
        !adc_read_average(ADC_CHANNEL_16, &bat_raw)) {
        PURRGO_LOGE("BATT: ADC read failed\r\n");
        return 0;
    }

    /* Реальное VDDA по VREFINT и заводской калибровке (VREFINT_CAL при 3,0 В). */
    const uint32_t vdda_mv =
        __HAL_ADC_CALC_VREFANALOG_VOLTAGE(hadc1.Instance, vref_raw, ADC_RESOLUTION_14B);

    if (vdda_mv < BATT_VDDA_MIN_MV || vdda_mv > BATT_VDDA_MAX_MV) {
        PURRGO_LOGE("BATT: VDDA out of range: %u mV\r\n", (unsigned)vdda_mv);
        return 0;
    }

    const uint32_t full_scale = 16383u;   /* 14 бит */
    const uint32_t pin_mv = (bat_raw * vdda_mv + full_scale / 2u) / full_scale;

    PURRGO_LOGD("BATT: vdda=%u mV pin=%u mV pack=%u mV\r\n", (unsigned)vdda_mv,
                (unsigned)pin_mv,
                (unsigned)purrgo_battery_pack_mv_from_pin(pin_mv));

    return (uint16_t)pin_mv;
}

/*
 * Одно измерение: включить АЦП, откалибровать, измерить, обесточить АЦП
 * полностью (регулятор, VREFINT, тактирование). 0 при ошибке.
 */
static uint16_t measure_pin_mv(void)
{
    uint16_t mv = 0;

    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        PURRGO_LOGE("BATT: ADC init failed\r\n");
    } else if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET_LINEARITY,
                                           ADC_SINGLE_ENDED) != HAL_OK) {
        PURRGO_LOGE("BATT: ADC calibration failed\r\n");
    } else {
        mv = measure_pin_mv_adc_on();
    }

    (void)HAL_ADC_DeInit(&hadc1);
    return mv;
}

uint16_t purrgo_platform_battery_pin_mv(void)
{
    const uint32_t now = HAL_GetTick();

    if (s_have_value && (now - s_last_tick_ms) < PURRGO_HW_BATT_PERIOD_MS) {
        return s_last_pin_mv;
    }

    const uint16_t mv = measure_pin_mv();

    /* Сбой: возвращаем прошлое значение (если было), повтор через период. */
    s_last_tick_ms = now;
    if (mv != 0u) {
        s_last_pin_mv = mv;
        s_have_value = true;
    }
    return s_have_value ? s_last_pin_mv : 0u;
}
