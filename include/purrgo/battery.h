#ifndef PURRGO_BATTERY_H
#define PURRGO_BATTERY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Источник питания (пункт меню CONFIG, ключ POWER_SRC в PURRGO.CFG).
 * Числа сохраняются в файле конфигурации, поэтому порядок менять нельзя;
 * новые значения добавляются перед PURRGO_POWER_SRC_COUNT.
 */
typedef enum {
    PURRGO_POWER_SRC_ZINC_CARBON = 0,   /* солевые батарейки */
    PURRGO_POWER_SRC_ALKALINE    = 1,   /* алкалайновые батарейки */
    PURRGO_POWER_SRC_NIMH        = 2,   /* Ni-MH аккумуляторы */
    PURRGO_POWER_SRC_LITHIUM     = 3,   /* литиевые аккумуляторы */
    PURRGO_POWER_SRC_EXTERNAL_5V = 4,   /* внешнее питание 5 В (USB, powerbank) */
    PURRGO_POWER_SRC_COUNT
} purrgo_power_source_t;

/* Источник питания по умолчанию (основной тип элементов - Ni-MH). */
#define PURRGO_POWER_SRC_DEFAULT PURRGO_POWER_SRC_NIMH

/* Число последовательно включённых элементов (три AA/AAA). */
#define PURRGO_BATTERY_CELLS 3

/* Название источника питания для меню (латиница, шрифт 5x7). */
const char* purrgo_power_source_title(purrgo_power_source_t src);

/*
 * Состояние батареи.
 *
 * Напряжение блока измеряет платформа (purrgo_platform_battery_pin_mv():
 * АЦП + делитель, см. hardware_config.h); ядро пересчитывает его по делителю и
 * получает процент по кривой разряда выбранного источника питания. Если
 * платформа напряжения не даёт (PC, АЦП не готов), процент берётся из заглушки
 * PURRGO_BATTERY_STUB_PERCENT (battery.c).
 */
typedef struct {
    int8_t   percent;      /* 0..100; -1 - неизвестно или питание внешнее */
    uint16_t millivolts;   /* напряжение батареи, мВ; 0 - не измерено */
    bool     external;     /* true - питание 5 В, заряда батареи нет */
} purrgo_battery_t;

/*
 * Платформенный хук: напряжение на выводе АЦП (середина делителя), мВ;
 * 0 - не измерено. STM32: apps/stm32/PurrGO_STM32U5/Core/Src/battery_adc.c.
 */
uint16_t purrgo_platform_battery_pin_mv(void);

/*
 * Напряжение блока батарей, мВ, по напряжению на выводе АЦП: учитывает
 * делитель и поправку PURRGO_HW_BATT_* (hardware_config.h). 0 -> 0.
 */
uint32_t purrgo_battery_pack_mv_from_pin(uint32_t pin_mv);

/*
 * Заполняет out текущим состоянием для источника питания из app_config.
 * Возвращает false, если процент неизвестен (percent = -1): при питании 5 В
 * и когда нет данных.
 */
bool purrgo_battery_read(purrgo_battery_t* out);

/*
 * Текст заряда для строки статуса (не длиннее 4 символов + NUL): "85%",
 * "5V" при внешнем питании, "--%" если заряд неизвестен. Читает батарею сама
 * (purrgo_battery_read()), поэтому вызывать её чаще нужной частоты обновления
 * не следует. size >= 6 вмещает любое значение.
 */
void purrgo_battery_format(char* buf, size_t size);

/*
 * Заряд в процентах (0..100) по напряжению батареи из PURRGO_BATTERY_CELLS
 * элементов и кривой разряда источника src. Для PURRGO_POWER_SRC_EXTERNAL_5V
 * и неизвестного src возвращает -1.
 */
int purrgo_battery_percent_from_mv(purrgo_power_source_t src, uint32_t pack_mv);

#endif /* PURRGO_BATTERY_H */
