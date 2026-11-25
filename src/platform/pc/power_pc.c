#include "purrgo/power.h"
#include "purrgo/battery.h"
#include "purrgo/supply.h"
#include "purrgo/pc_power.h"
#include "purrgo/hardware_config.h"
#include <stdint.h>
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
 * На PC напряжения нет: по умолчанию батарея «не измерена» (процент - заглушка),
 * VDD в норме. Значения задаёт эмулятор из командной строки (purrgo/pc_power.h).
 */
static uint16_t s_pin_mv;
static bool s_supply_low;
static bool s_ext_power;

/*
 * Блок -> вывод АЦП: обратное преобразование к purrgo_battery_pack_mv_from_pin()
 * (делитель и калибровка из hardware_config.h), чтобы ядро получило ровно то
 * напряжение блока, которое задали в параметре.
 */
void purrgo_pc_power_set_battery_mv(uint32_t pack_mv)
{
    const uint64_t num = (uint64_t)pack_mv * 1000u * PURRGO_HW_BATT_DIV_R_BOTTOM_OHM;
    const uint64_t den = (uint64_t)PURRGO_HW_BATT_CAL_PERMILLE
        * (PURRGO_HW_BATT_DIV_R_TOP_OHM + PURRGO_HW_BATT_DIV_R_BOTTOM_OHM);
    uint64_t pin = (num + den / 2u) / den;

    if (pin > UINT16_MAX) {
        pin = UINT16_MAX;
    }
    s_pin_mv = (uint16_t)pin;
}

void purrgo_pc_power_set_supply_low(bool low)
{
    s_supply_low = low;
}

void purrgo_pc_power_set_ext_power(bool present)
{
    s_ext_power = present;
}

bool purrgo_platform_ext_power_present(void)
{
    return s_ext_power;
}

uint16_t purrgo_platform_battery_pin_mv(void)
{
    return s_pin_mv;
}

bool purrgo_platform_supply_low(void)
{
    return s_supply_low;
}
