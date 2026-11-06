/*
 * Host-тест реакции на низкое напряжение питания (src/core/supply.c):
 *   - короткая просадка (< PURRGO_SUPPLY_LOW_CONFIRM_MS) игнорируется;
 *   - устойчивая просадка: трек закрыт с футером GPX (RAM-трек сохранён), файлы
 *     карты закрыты, карта размонтирована (purrgo_platform_storage_suspend) -
 *     именно в таком порядке, GNSS не выключается;
 *   - пока напряжение низкое, трек не стартует и повторных попыток не делает;
 *   - после PURRGO_SUPPLY_RECOVER_MS нормального напряжения запись возобновляется
 *     новым файлом; короткое "выздоровление" посреди просадки не считается.
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/battery.h"
#include "purrgo/config.h"
#include "purrgo/fs_hal.h"
#include "purrgo/supply.h"
#include "purrgo/track_logger.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

/* Переносимый аналог GNU memmem (при -std=c11 он не объявлен). */
static bool buf_contains(const uint8_t *hay, uint32_t hay_len, const char *needle)
{
    size_t n = strlen(needle);
    if (n == 0u) { return true; }
    if (hay_len < n) { return false; }
    for (uint32_t i = 0; i + n <= hay_len; i++) {
        if (memcmp(hay + i, needle, n) == 0) { return true; }
    }
    return false;
}

static void feed(const char *body)
{
    char line[160];
    unsigned cs = 0;
    for (const char *p = body; *p; p++) { cs ^= (unsigned char)*p; }
    snprintf(line, sizeof line, "$%s*%02X", body, cs);
    for (size_t i = 0; line[i]; i++) { purrgo_app_feed_gnss_byte((uint8_t)line[i]); }
    purrgo_app_feed_gnss_byte('\r');
    purrgo_app_feed_gnss_byte('\n');
}

static void feed_epoch(void)
{
    feed("GPRMC,093015.00,A,3133.54000,N,03528.39000,E,0.50,90.00,191026,,,A");
    feed("GPGGA,093015.00,3133.54000,N,03528.39000,E,1,08,1.1,100.0,M,18.9,M,,");
}

/* Шаг 10 мс: фикс обновляется раз в секунду, чтобы не истекал таймаут GNSS. */
static void run_ms(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 10) {
        g_now += 10;
        if (g_now % 1000u == 0u) { feed_epoch(); }
        purrgo_app_tick(g_now);
    }
}

static struct {
    track_logger_state_t logger_state;
    int open_handles;
    size_t ram_points;
} g_at_suspend;

static void suspend_probe(void)
{
    g_at_suspend.logger_state = purrgo_logger_get_state();
    g_at_suspend.open_handles = fake_fs_open_handles();
    g_at_suspend.ram_points = purrgo_logger_get_track_point_count();
}

static const char *track_name(void)
{
    const char *n = purrgo_logger_get_active_filename();
    static char copy[64];
    if (n == NULL) { return NULL; }
    strncpy(copy, n, sizeof copy - 1u);
    copy[sizeof copy - 1u] = '\0';
    return copy;
}

static void test_supply_low(void)
{
    fake_fs_reset();
    fake_power_reset();
    memset(&app_config, 0, sizeof app_config);
    purrgo_logger_reset();
    purrgo_logger_set_mode(LOGGER_MODE_STANDARD);
    g_now = 200000;
    purrgo_app_init();
    feed_epoch();
    run_ms(5000);
    CHECK(purrgo_app_get_gnss_solution()->valid);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    char first_track[64];
    {
        const char *n = track_name();
        CHECK(n != NULL);
        strncpy(first_track, n ? n : "", sizeof first_track - 1u);
        first_track[sizeof first_track - 1u] = '\0';
    }
    CHECK(!purrgo_supply_storage_blocked());

    /* Короткая просадка короче порога подтверждения: игнорируется. */
    fake_power_set_supply_low(true);
    run_ms(PURRGO_SUPPLY_LOW_CONFIRM_MS / 2u);
    fake_power_set_supply_low(false);
    run_ms(200);
    CHECK(!purrgo_supply_storage_blocked());
    CHECK(fake_power_suspend_calls() == 0);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);

    /* Устойчивая просадка. */
    const size_t ram_before = purrgo_logger_get_track_point_count();
    fake_power_set_suspend_probe(suspend_probe);
    fake_power_set_supply_low(true);
    run_ms(PURRGO_SUPPLY_LOW_CONFIRM_MS + 20u);
    CHECK(purrgo_supply_storage_blocked());
    CHECK(fake_power_suspend_calls() == 1);
    CHECK(fake_power_gnss_off_calls() == 0);                       /* GNSS работает */
    CHECK(g_at_suspend.logger_state == LOGGER_STATE_IDLE);         /* трек закрыт до размонтирования */
    CHECK(g_at_suspend.open_handles == 0);                         /* ни одного открытого файла */
    CHECK(g_at_suspend.ram_points == ram_before);                  /* RAM-трек для экрана цел */
    CHECK(purrgo_logger_get_track_point_count() == ram_before);
    {
        uint32_t size = 0;
        const uint8_t *d = fake_fs_data(first_track, &size);
        CHECK(d != NULL && size > 0);
        CHECK(d != NULL && buf_contains(d, size, "</gpx>"));   /* футер записан */
    }

    /* Пока напряжение низкое: ни старта трека, ни повторных вызовов размонтирования. */
    run_ms(30000);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);
    CHECK(fake_power_suspend_calls() == 1);
    CHECK(fake_fs_open_handles() == 0);

    /* Короткое "выздоровление" посреди просадки запись не возобновляет. */
    fake_power_set_supply_low(false);
    run_ms(PURRGO_SUPPLY_RECOVER_MS / 2u);
    CHECK(purrgo_supply_storage_blocked());
    fake_power_set_supply_low(true);
    run_ms(500);
    fake_power_set_supply_low(false);
    run_ms(PURRGO_SUPPLY_RECOVER_MS / 2u);
    CHECK(purrgo_supply_storage_blocked());
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);

    /* Устойчивое восстановление: запись разрешена, трек стартует новым файлом. */
    run_ms(PURRGO_SUPPLY_RECOVER_MS + 2000u);
    CHECK(!purrgo_supply_storage_blocked());
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    {
        const char *n = track_name();
        CHECK(n != NULL && strcmp(n, first_track) != 0);
    }
    CHECK(fake_power_suspend_calls() == 1);
}

/* Подготовка: идёт запись трека, питание в норме. */
static void start_recording(void)
{
    fake_fs_reset();
    fake_power_reset();
    memset(&app_config, 0, sizeof app_config);   /* источник питания - не 5 В */
    purrgo_logger_reset();
    purrgo_logger_set_mode(LOGGER_MODE_STANDARD);
    purrgo_supply_set_battery_stop_mv(0);
    g_now += 100000;
    purrgo_app_init();
    feed_epoch();
    run_ms(5000);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    CHECK(!purrgo_supply_storage_blocked());
}

/* Значения на выводе АЦП, дающие напряжение блока ниже порога и заведомо выше порога + гистерезис. */
#define STOP_MV 3300u
static uint16_t pin_low, pin_high;

static void find_pins(void)
{
    pin_low = 1000;
    pin_high = 3000;
    CHECK(purrgo_battery_pack_mv_from_pin(pin_low) < STOP_MV);
    CHECK(purrgo_battery_pack_mv_from_pin(pin_high) >= STOP_MV + PURRGO_SUPPLY_BATT_HYST_MV);
}

/* Остановка по АЦП, затем VDD: карта отключается один раз, возврат - когда нормальны обе причины. */
static void test_battery_then_vdd(void)
{
    start_recording();
    find_pins();
    purrgo_supply_set_battery_stop_mv(STOP_MV);

    fake_power_set_battery_pin_mv(pin_high);
    run_ms(3000);
    CHECK(!purrgo_supply_storage_blocked());          /* батареи в норме */

    fake_power_set_battery_pin_mv(pin_low);
    run_ms(PURRGO_SUPPLY_BATT_POLL_MS + PURRGO_SUPPLY_LOW_CONFIRM_MS + 100u);
    CHECK(purrgo_supply_storage_blocked());
    CHECK(fake_power_suspend_calls() == 1);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);
    CHECK(strcmp(purrgo_battery_status_text(), "LOW") == 0);   /* индикатор в строке статуса */

    /* Следом падает и VDD: повторной остановки нет. */
    fake_power_set_supply_low(true);
    run_ms(1000);
    CHECK(fake_power_suspend_calls() == 1);

    /* Батареи ожили (напряжение выше порога + гистерезис), но VDD ещё низкий: заблокировано. */
    fake_power_set_battery_pin_mv(pin_high);
    run_ms(PURRGO_SUPPLY_RECOVER_MS + 3000u);
    CHECK(purrgo_supply_storage_blocked());

    /* VDD в норме: после паузы запись возобновляется, индикатор гаснет. */
    fake_power_set_supply_low(false);
    run_ms(PURRGO_SUPPLY_RECOVER_MS + 2000u);
    CHECK(!purrgo_supply_storage_blocked());
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    CHECK(strcmp(purrgo_battery_status_text(), "LOW") != 0);
    CHECK(fake_power_suspend_calls() == 1);
}

/* Остановка по VDD, затем АЦП: тоже один раз. Гистерезис АЦП: чуть выше порога - ещё низко. */
static void test_vdd_then_battery(void)
{
    start_recording();
    find_pins();
    purrgo_supply_set_battery_stop_mv(STOP_MV);
    fake_power_set_battery_pin_mv(pin_high);

    fake_power_set_supply_low(true);
    run_ms(PURRGO_SUPPLY_LOW_CONFIRM_MS + 100u);
    CHECK(purrgo_supply_storage_blocked());
    CHECK(fake_power_suspend_calls() == 1);

    fake_power_set_battery_pin_mv(pin_low);
    run_ms(PURRGO_SUPPLY_BATT_POLL_MS + 200u);
    CHECK(fake_power_suspend_calls() == 1);

    /* VDD восстановился, а батареи по АЦП всё ещё ниже порога: заблокировано. */
    fake_power_set_supply_low(false);
    run_ms(PURRGO_SUPPLY_RECOVER_MS + 3000u);
    CHECK(purrgo_supply_storage_blocked());
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);

    /* Питание 5 В: по АЦП не судим. */
    app_config.power_source = PURRGO_POWER_SRC_EXTERNAL_5V;
    run_ms(PURRGO_SUPPLY_RECOVER_MS + 3000u);
    CHECK(!purrgo_supply_storage_blocked());
    purrgo_supply_set_battery_stop_mv(0);
}

/* Порог по умолчанию выключен: напряжение батарей запись не останавливает. */
static void test_battery_reason_off(void)
{
    start_recording();
    find_pins();
    fake_power_set_battery_pin_mv(pin_low);
    run_ms(5000);
    CHECK(!purrgo_supply_storage_blocked());
    CHECK(fake_power_suspend_calls() == 0);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
}

int main(void)
{
    test_supply_low();
    test_battery_then_vdd();
    test_vdd_then_battery();
    test_battery_reason_off();
    return test_finish("test_supply_low");
}
