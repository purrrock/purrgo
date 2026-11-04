#ifndef FAKE_POWER_H
#define FAKE_POWER_H
#include <stdint.h>
/*
 * Тестовые платформенные хуки выключения (purrgo_platform_storage_unmount,
 * purrgo_platform_enter_standby): запоминают порядок вызовов и не завершают процесс.
 */
void fake_power_reset(void);
/* probe вызывается из purrgo_platform_storage_unmount() - там тест проверяет состояние. */
void fake_power_set_unmount_probe(void (*probe)(void));
int fake_power_unmount_calls(void);
int fake_power_standby_calls(void);
/* Порядковые номера вызовов (с 1; 0 - не вызывалась) для проверки очерёдности. */
int fake_power_unmount_seq(void);
int fake_power_standby_seq(void);
/* Вызовы purrgo_gnss_power_off() и порядковый номер последнего вызова. */
int fake_power_gnss_off_calls(void);
int fake_power_gnss_off_seq(void);
/* Напряжение на выводе АЦП, которое вернёт purrgo_platform_battery_pin_mv() (по умолчанию 0). */
void fake_power_set_battery_pin_mv(uint16_t mv);
#endif
