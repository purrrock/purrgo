#ifndef PURRGO_PC_POWER_H
#define PURRGO_PC_POWER_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Имитация питания для ПК-эмулятора (src/platform/pc/power_pc.c).
 *
 * На ПК напряжений нет, поэтому эмулятор задаёт их из параметров командной
 * строки (--battery-mv, --supply-low), чтобы проверить на экране процент заряда
 * и индикатор остановки записи (purrgo/supply.h). Менять значения можно в любой
 * момент: ядро читает их через purrgo_platform_battery_pin_mv() и
 * purrgo_platform_supply_low().
 */

/*
 * Напряжение блока батарей, мВ (то, что показал бы АЦП после пересчёта по
 * делителю). 0 - «не измерено» (по умолчанию): процент заглушка, остановки по
 * батарее нет.
 */
void purrgo_pc_power_set_battery_mv(uint32_t pack_mv);

/* true - имитировать просадку VDD ниже порога безопасной записи (как PVD). */
void purrgo_pc_power_set_supply_low(bool low);

#endif /* PURRGO_PC_POWER_H */
