/*
 * Host-тест реакции на низкое напряжение питания (src/core/supply.c):
 *   - короткая просадка (< PURRGO_SUPPLY_LOW_CONFIRM_MS) игнорируется;
 *   - устойчивая просадка по PVD: трек закрыт с футером GPX (RAM-трек сохранён),
 *     GNSS не выключается, карта НЕ размонтируется, чтение с неё работает,
 *     запись PURRGO.CFG отклоняется;
 *   - пока напряжение низкое, трек не стартует;
 *   - после PURRGO_SUPPLY_RECOVER_MS нормального напряжения запись возобновляется
 *     новым файлом;
 *   - порог по батарее зависит от источника: солевые 1000 мВ/элемент, Li-ion AA
 *     900 мВ/элемент, у остальных источников автоостановки нет; срабатывает после
 *     нескольких опросов подряд и держится до смены источника питания.
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


static const char *track_name(void)
{
    const char *n = purrgo_logger_get_active_filename();
    static char copy[64];
    if (n == NULL) { return NULL; }
    strncpy(copy, n, sizeof copy - 1u);
    copy[sizeof copy - 1u] = '\0';
    return copy;
}

/* Чтение с карты работает при заблокированной записи. */
static void check_read_works(const char *path)
{
    purrgo_file_t *f = purrgo_fs_open(path, FS_READ);
    CHECK(f != NULL);
    if (f != NULL) { (void)purrgo_fs_close(f); }
}

/* Подготовка: идёт запись трека, питание в норме, источник питания src. */
static void start_recording(purrgo_power_source_t src)
{
    fake_fs_reset();
    fake_power_reset();
    memset(&app_config, 0, sizeof app_config);
    purrgo_logger_reset();
    purrgo_logger_set_mode(LOGGER_MODE_STANDARD);
    g_now += 100000;
    purrgo_app_init();
    app_config.power_source = src;   /* после init: он загружает конфиг и сбрасывает источник */
    feed_epoch();
    run_ms(5000);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    CHECK(!purrgo_supply_storage_blocked());
}

static void test_supply_low(void)
{
    g_now = 200000;
    start_recording(PURRGO_POWER_SRC_NIMH);
    CHECK(purrgo_app_get_gnss_solution()->valid);
    char first_track[64];
    {
        const char *n = track_name();
        CHECK(n != NULL);
        strncpy(first_track, n ? n : "", sizeof first_track - 1u);
        first_track[sizeof first_track - 1u] = '\0';
    }

    /* Короткая просадка короче порога подтверждения: игнорируется. */
    fake_power_set_supply_low(true);
    run_ms(PURRGO_SUPPLY_LOW_CONFIRM_MS / 2u);
    fake_power_set_supply_low(false);
    run_ms(200);
    CHECK(!purrgo_supply_storage_blocked());
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);

    /* Устойчивая просадка. */
    const size_t ram_before = purrgo_logger_get_track_point_count();
    fake_power_set_supply_low(true);
    run_ms(PURRGO_SUPPLY_LOW_CONFIRM_MS + 20u);
    CHECK(purrgo_supply_storage_blocked());
    CHECK(fake_power_unmount_calls() == 0);                        /* карта остаётся смонтированной */
    CHECK(fake_power_gnss_off_calls() == 0);                       /* GNSS работает */
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);         /* трек закрыт */
    CHECK(fake_fs_open_handles() == 0);
    CHECK(purrgo_logger_get_track_point_count() == ram_before);    /* RAM-трек для экрана цел */
    {
        uint32_t size = 0;
        const uint8_t *d = fake_fs_data(first_track, &size);
        CHECK(d != NULL && size > 0);
        CHECK(d != NULL && buf_contains(d, size, "</gpx>"));       /* футер записан */
    }
    check_read_works(first_track);                                 /* чтение с карты работает */
    CHECK(!purrgo_config_save());                                  /* запись настроек отклонена */

    /* Пока напряжение низкое: трек не стартует, карта не трогается. */
    run_ms(30000);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);
    CHECK(fake_power_unmount_calls() == 0);
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
    CHECK(purrgo_config_save());
    CHECK(fake_power_unmount_calls() == 0);
}

/* Время, за которое гарантированно набирается остановка по батарее. */
#define BATT_STOP_RUN_MS \
    (PURRGO_SUPPLY_BATT_POLL_MS * (PURRGO_SUPPLY_BATT_CONFIRM_POLLS + 1u) + PURRGO_SUPPLY_LOW_CONFIRM_MS + 100u)

static void set_pack_mv(uint32_t pack_mv)
{
    fake_power_set_battery_pin_mv(fake_power_pin_for_pack_mv(pack_mv));
}

/* Порог по батарее для источника src: чуть выше - работает, чуть ниже - стоп. */
static void check_threshold(purrgo_power_source_t src, uint32_t stop_mv)
{
    CHECK(purrgo_battery_stop_mv(src) == stop_mv);

    start_recording(src);
    set_pack_mv(stop_mv + 100u);
    run_ms(BATT_STOP_RUN_MS);
    CHECK(!purrgo_supply_storage_blocked());
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);

    set_pack_mv(stop_mv - 100u);
    run_ms(BATT_STOP_RUN_MS);
    CHECK(purrgo_supply_storage_blocked());
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);
    CHECK(fake_power_unmount_calls() == 0);
    CHECK(fake_power_gnss_off_calls() == 0);
    CHECK(strcmp(purrgo_battery_status_text(), "LOW") == 0);
    CHECK(!purrgo_config_save());
}

static void test_battery_zinc_liion(void)
{
    check_threshold(PURRGO_POWER_SRC_ZINC_CARBON, 3000u);   /* 1000 мВ/элемент */
    check_threshold(PURRGO_POWER_SRC_LIION_AA, 2700u);      /* 900 мВ/элемент */
}

/* Одиночный замер ниже порога (всплеск тока) блокировку не включает. */
static void test_battery_glitch(void)
{
    start_recording(PURRGO_POWER_SRC_ZINC_CARBON);
    set_pack_mv(3300u);
    run_ms(3000);
    set_pack_mv(2800u);
    run_ms(PURRGO_SUPPLY_BATT_POLL_MS);      /* один-два опроса */
    set_pack_mv(3300u);
    run_ms(BATT_STOP_RUN_MS);
    CHECK(!purrgo_supply_storage_blocked());
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
}

/* Остановка по батарее держится при отскоке напряжения; снимается сменой источника. */
static void test_battery_sticky_and_source_change(void)
{
    start_recording(PURRGO_POWER_SRC_ZINC_CARBON);
    set_pack_mv(2800u);
    run_ms(BATT_STOP_RUN_MS);
    CHECK(purrgo_supply_storage_blocked());

    set_pack_mv(3500u);                      /* отскок после снятия нагрузки */
    run_ms(PURRGO_SUPPLY_RECOVER_MS + 5000u);
    CHECK(purrgo_supply_storage_blocked());
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);

    /* Источник сменили на 5 В: запись разрешается, трек - новым файлом. */
    app_config.power_source = PURRGO_POWER_SRC_EXTERNAL_5V;
    run_ms(PURRGO_SUPPLY_RECOVER_MS + 3000u);
    CHECK(!purrgo_supply_storage_blocked());
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    CHECK(fake_power_unmount_calls() == 0);
}

/* Для остальных источников автоостановки по батарее нет (останавливает только PVD). */
static void test_battery_other_sources(void)
{
    const purrgo_power_source_t others[] = {
        PURRGO_POWER_SRC_ALKALINE, PURRGO_POWER_SRC_NIMH,
        PURRGO_POWER_SRC_LITHIUM, PURRGO_POWER_SRC_EXTERNAL_5V,
    };
    for (unsigned i = 0; i < sizeof others / sizeof others[0]; i++) {
        CHECK(purrgo_battery_stop_mv(others[i]) == 0u);
        start_recording(others[i]);
        set_pack_mv(1500u);                  /* заведомо "разряжено" */
        run_ms(BATT_STOP_RUN_MS + 5000u);
        CHECK(!purrgo_supply_storage_blocked());
        CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    }
}

/* Остановка по батарее, затем PVD: одна блокировка; возврат - когда нормальны обе причины. */
static void test_battery_then_vdd(void)
{
    start_recording(PURRGO_POWER_SRC_LIION_AA);
    set_pack_mv(2600u);
    run_ms(BATT_STOP_RUN_MS);
    CHECK(purrgo_supply_storage_blocked());

    fake_power_set_supply_low(true);
    run_ms(1000);
    CHECK(purrgo_supply_storage_blocked());

    fake_power_set_supply_low(false);
    run_ms(PURRGO_SUPPLY_RECOVER_MS + 2000u);
    CHECK(purrgo_supply_storage_blocked());  /* батарея всё ещё причина */

    app_config.power_source = PURRGO_POWER_SRC_NIMH;
    run_ms(PURRGO_SUPPLY_RECOVER_MS + 2000u);
    CHECK(!purrgo_supply_storage_blocked());
}

int main(void)
{
    test_supply_low();
    test_battery_zinc_liion();
    test_battery_glitch();
    test_battery_sticky_and_source_change();
    test_battery_other_sources();
    test_battery_then_vdd();
    return test_finish("test_supply_low");
}
