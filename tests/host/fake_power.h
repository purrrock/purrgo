#ifndef FAKE_POWER_H
#define FAKE_POWER_H
#include <stdbool.h>
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
/* Вызовы purrgo_gnss_power_on() и порядковый номер последнего вызова. */
int fake_power_gnss_on_calls(void);
int fake_power_gnss_on_seq(void);
/* Напряжение на выводе АЦП, которое вернёт purrgo_platform_battery_pin_mv() (по умолчанию 0). */
void fake_power_set_battery_pin_mv(uint16_t mv);

/*
 * Напряжение на выводе АЦП, при котором purrgo_battery_pack_mv_from_pin() даст
 * (с точностью до округления) pack_mv. Учитывает делитель и поправку из
 * hardware_config.h, поэтому тесты не зависят от калибровки конкретного экземпляра.
 */
uint16_t fake_power_pin_for_pack_mv(uint32_t pack_mv);
/* Низкое напряжение питания, которое вернёт purrgo_platform_supply_low() (по умолчанию false). */
void fake_power_set_supply_low(bool low);
/* Вызовы purrgo_platform_storage_suspend() и порядковый номер последнего вызова. */
/* probe вызывается из purrgo_platform_storage_suspend(). */
void fake_power_set_suspend_probe(void (*probe)(void));
int fake_power_suspend_calls(void);
int fake_power_suspend_seq(void);
#endif
