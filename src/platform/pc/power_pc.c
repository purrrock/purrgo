#include "purrgo/power.h"
#include "purrgo/battery.h"
#include "purrgo/supply.h"
#include <stdlib.h>

void purrgo_platform_storage_unmount(void)
{
    /* Файлы на хосте закрываются самим ядром, размонтировать нечего. */
}

void purrgo_platform_enter_standby(void)
{
    exit(0);
}

/*
 * На PC напряжение батареи не измеряется (процент - заглушка). Для проверки
 * отображения в эмуляторе: -DPURRGO_PC_BATTERY_PIN_MV=2250 (середина делителя, мВ).
 */
#ifndef PURRGO_PC_BATTERY_PIN_MV
#define PURRGO_PC_BATTERY_PIN_MV 0
#endif

uint16_t purrgo_platform_battery_pin_mv(void)
{
    return (uint16_t)PURRGO_PC_BATTERY_PIN_MV;
}

/* На PC напряжения питания МК нет: всегда нормальное, карту размонтировать нечего. */
bool purrgo_platform_supply_low(void)
{
    return false;
}

void purrgo_platform_storage_suspend(void)
{
}
