#ifndef PURRGO_STM32_SUPPLY_H
#define PURRGO_STM32_SUPPLY_H

/*
 * Контроль напряжения питания VDD на STM32U585 (PVD). Реализация хука ядра
 * purrgo_platform_supply_low() (purrgo/supply.h) - stm32/port/power/supply_pvd.c проекта.
 *
 * Порог задаётся макросом PURRGO_SUPPLY_PVD_LEVEL (PWR_PVDLEVEL_0..6, примерно
 * 2,0 / 2,2 / 2,4 / 2,5 / 2,6 / 2,8 / 2,9 В). По умолчанию PWR_PVDLEVEL_6 (около 2,9 В):
 * microSD требует не ниже 2,7 В, остаётся запас на закрытие трека и размонтирование.
 * Точные пороги и гистерезис - в datasheet STM32U585 (VPVD0..VPVD6, VHYST_PVD).
 */

/**
 * @brief Включить PVD и дождаться готовности компаратора.
 *
 * Вызывать один раз при старте, до монтирования карты (purrgo_sd_init()):
 * пока PVD не включён, purrgo_platform_supply_low() возвращает false.
 */
void purrgo_supply_stm32_init(void);

#endif /* PURRGO_STM32_SUPPLY_H */
