#ifndef PURRGO_BATTERY_H
#define PURRGO_BATTERY_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Состояние батареи.
 *
 * ЗАГЛУШКА: измерение напряжения (АЦП + делитель) ещё не реализовано, поэтому
 * purrgo_battery_read() возвращает фиксированные значения из battery.c
 * (PURRGO_BATTERY_STUB_PERCENT, напряжение не измеряется). Когда появится
 * реальное измерение, достаточно заменить реализацию purrgo_battery_read():
 * строка состояния и запись в трек уже используют этот интерфейс.
 */
typedef struct {
    int8_t   percent;      /* 0..100; -1 - неизвестно */
    uint16_t millivolts;   /* напряжение батареи, мВ; 0 - не измерено */
} purrgo_battery_t;

/*
 * Заполняет out текущим состоянием. Возвращает false, если состояние
 * неизвестно (percent = -1, millivolts = 0).
 */
bool purrgo_battery_read(purrgo_battery_t* out);

#endif /* PURRGO_BATTERY_H */
