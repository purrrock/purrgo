#ifndef PURRGO_BATTERY_H
#define PURRGO_BATTERY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Источник питания (пункт меню CONFIG, ключ POWER_SRC в PURRGO.CFG).
 * Числа сохраняются в файле конфигурации; порядок значений совпадает с порядком
 * перебора в меню, PURRGO_POWER_SRC_EXTERNAL_5V - последний (у него нет кривой
 * разряда, см. battery.c). При изменении нумерации старый PURRGO.CFG
 * нужно удалить или поправить POWER_SRC.
 */
typedef enum {
    PURRGO_POWER_SRC_ZINC_CARBON = 0,   /* солевые батарейки */
    PURRGO_POWER_SRC_ALKALINE    = 1,   /* алкалайновые батарейки */
    PURRGO_POWER_SRC_NIMH        = 2,   /* Ni-MH аккумуляторы */
    PURRGO_POWER_SRC_LIION_AA    = 3,   /* Li-ion аккумуляторы AA со встроенным понижающим преобразователем (1,5 В) */
    PURRGO_POWER_SRC_LITHIUM     = 4,   /* литиевые батарейки (Li-FeS2, AA, не перезаряжаемые) */
    PURRGO_POWER_SRC_EXTERNAL_5V = 5,   /* внешнее питание 5 В (USB, powerbank) */
    PURRGO_POWER_SRC_COUNT
} purrgo_power_source_t;

/* Источник питания по умолчанию (основной тип элементов - Ni-MH). */
#define PURRGO_POWER_SRC_DEFAULT PURRGO_POWER_SRC_NIMH

/* Число последовательно включённых элементов (три AA/AAA). */
#define PURRGO_BATTERY_CELLS 3

/*
 * Порог остановки записи на SD по напряжению ОДНОГО элемента, мВ. Ниже него запись
 * на карту блокируется (purrgo/supply.h). Задан только там, где кривая разряда
 * снята на устройстве: для остальных источников автоостановки по батарее нет,
 * пока не снята кривая разряда (останавливает только PVD).
 */
#define PURRGO_BATTERY_STOP_CELL_MV_ZINC     1000u   /* солевые батарейки */
#define PURRGO_BATTERY_STOP_CELL_MV_LIION_AA  900u   /* Li-ion AA со встроенным 1,5 В преобразователем */

/*
 * Порог остановки записи на SD по напряжению блока батарей (PURRGO_BATTERY_CELLS
 * элементов), мВ, для источника src; 0 - автоостановки по батарее нет
 * (щелочные, Ni-MH, литиевые Li-FeS2, питание 5 В).
 */
uint16_t purrgo_battery_stop_mv(purrgo_power_source_t src);

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
 * "5V" при внешнем питании, "--%" если заряд неизвестен, "LOW" - запись на SD
 * остановлена из-за низкого питания (purrgo/supply.h; вытесняет остальные тексты).
 * Читает батарею сама
 * (purrgo_battery_read()), поэтому вызывать её чаще нужной частоты обновления
 * не следует. size >= 6 вмещает любое значение.
 */
void purrgo_battery_format(char* buf, size_t size);

/*
 * Монитор заряда для строки статуса - единый для карты и читалки.
 *
 * Опрашивает батарею не чаще PURRGO_BATTERY_STATUS_PERIOD_MS и хранит готовый
 * текст (\"85%\", \"5V\", \"--%\"). Экраны текст не считают, а берут из
 * purrgo_battery_status_text(); тик вызывает purrgo_app_tick() в любом режиме.
 * Смена источника питания в меню подхватывается сразу, без ожидания периода.
 */
#ifndef PURRGO_BATTERY_STATUS_PERIOD_MS
#define PURRGO_BATTERY_STATUS_PERIOD_MS 60000u
#endif

/* Размер буфера текста заряда вместе с NUL. */
#define PURRGO_BATTERY_STATUS_TEXT_SIZE 8

/*
 * Периодический опрос. true - показываемый текст изменился (первый замер после
 * сброса не считается изменением: экраны всё равно рисуются целиком).
 */
bool purrgo_battery_monitor_tick(uint32_t now_ms);

/* Текст заряда для строки статуса: последний замер (при первом обращении - замер сейчас). */
const char* purrgo_battery_status_text(void);

/* Забыть замер (инициализация приложения, тесты). */
void purrgo_battery_monitor_reset(void);

/*
 * Заряд в процентах (0..100) по напряжению батареи из PURRGO_BATTERY_CELLS
 * элементов и кривой разряда источника src. Для PURRGO_POWER_SRC_EXTERNAL_5V
 * и неизвестного src возвращает -1.
 */
int purrgo_battery_percent_from_mv(purrgo_power_source_t src, uint32_t pack_mv);

#endif /* PURRGO_BATTERY_H */
