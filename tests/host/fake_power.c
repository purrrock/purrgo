/*
 * Тестовые платформенные хуки выключения (замена src/platform/pc/power_pc.c,
 * который завершает процесс).
 */
#include "purrgo/power.h"
#include "purrgo/battery.h"
#include "fake_power.h"
#include <stddef.h>

static int s_seq, s_unmount_calls, s_standby_calls, s_unmount_seq, s_standby_seq;
static int s_gnss_off_calls, s_gnss_off_seq, s_gnss_on_calls, s_gnss_on_seq;
static void (*s_probe)(void);
static uint16_t s_battery_pin_mv;

void fake_power_reset(void)
{
    s_seq = s_unmount_calls = s_standby_calls = s_unmount_seq = s_standby_seq = 0;
    s_gnss_off_calls = s_gnss_off_seq = 0;
    s_gnss_on_calls = s_gnss_on_seq = 0;
    s_probe = NULL;
    s_battery_pin_mv = 0;
}
void fake_power_set_unmount_probe(void (*probe)(void)) { s_probe = probe; }
int fake_power_unmount_calls(void) { return s_unmount_calls; }
int fake_power_standby_calls(void) { return s_standby_calls; }
int fake_power_unmount_seq(void) { return s_unmount_seq; }
int fake_power_standby_seq(void) { return s_standby_seq; }
int fake_power_gnss_off_calls(void) { return s_gnss_off_calls; }
int fake_power_gnss_off_seq(void) { return s_gnss_off_seq; }
int fake_power_gnss_on_calls(void) { return s_gnss_on_calls; }
int fake_power_gnss_on_seq(void) { return s_gnss_on_seq; }

/* Замена purrgo_gnss_power_on/off() из платформенного gnss_io.c (в host-тесты он не входит). */
void purrgo_gnss_power_on(void)
{
    s_gnss_on_calls++;
    s_gnss_on_seq = ++s_seq;
}
void purrgo_gnss_power_off(void)
{
    s_gnss_off_calls++;
    s_gnss_off_seq = ++s_seq;
}

void purrgo_platform_storage_unmount(void)
{
    s_unmount_calls++;
    s_unmount_seq = ++s_seq;
    if (s_probe) { s_probe(); }
}

void purrgo_platform_enter_standby(void)
{
    s_standby_calls++;
    s_standby_seq = ++s_seq;
}

void fake_power_set_battery_pin_mv(uint16_t mv) { s_battery_pin_mv = mv; }
uint16_t purrgo_platform_battery_pin_mv(void) { return s_battery_pin_mv; }
