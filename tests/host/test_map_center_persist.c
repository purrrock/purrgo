/*
 * Host-тесты сохранения центра карты в PURRGO.CFG:
 *   - при каждой записи PURRGO.CFG (purrgo_config_save()) в LAST_LAT_1E7 / LAST_LON_1E7 попадает
 *     текущий центр карты, а не значение по умолчанию или старое из файла;
 *   - после "перезагрузки" (purrgo_config_load() + purrgo_map_controller_init()) карта
 *     открывается с сохранённого центра;
 *   - выключение (purrgo_system_power_off()) тоже сохраняет центр;
 *   - сдвиг карты сам по себе файл не пишет (запись только вместе с PURRGO.CFG);
 *   - без контроллера карты (purrgo_config_init() сбрасывает хук) значения в app_config не трогаются;
 *   - при остановке записи на SD (низкое питание) файл не пишется.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "purrgo/config.h"
#include "purrgo/map_controller.h"
#include "purrgo/power.h"
#include "purrgo/supply.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

static uint32_t g_now = 100000;
uint32_t purrgo_system_time_ms(void) { return g_now; }

static const char *CFG = "PURRGO.CFG";

/* Значение целого ключа KEY=<число> из PURRGO.CFG; found = 0, если ключа нет. */
static long cfg_value(const char *key, int *found)
{
    uint32_t size = 0;
    const uint8_t *d = fake_fs_data(CFG, &size);
    *found = 0;
    if (d == NULL) { return 0; }

    char *copy = malloc(size + 1u);
    memcpy(copy, d, size);
    copy[size] = '\0';

    char needle[48];
    snprintf(needle, sizeof needle, "\n%s=", key);
    long v = 0;
    char *p = strstr(copy, needle);
    if (p == NULL && strncmp(copy, needle + 1, strlen(needle) - 1) == 0) {
        p = copy - 1;   /* ключ в первой строке */
    }
    if (p != NULL) {
        v = strtol(p + strlen(needle), NULL, 10);
        *found = 1;
    }
    free(copy);
    return v;
}

/* "Включение": загрузка конфига и инициализация карты, как в purrgo_app_init(). */
static void boot(void)
{
    purrgo_config_load();
    purrgo_map_controller_init();
}

static void test_default_center_when_no_file(void)
{
    fake_fs_reset();
    boot();
    CHECK(map_app_get_map_center_lat() == 537135000);
    CHECK(map_app_get_map_center_lon() == 284199000);
}

static void test_save_stores_current_center(void)
{
    int f;

    fake_fs_reset();
    boot();
    const int32_t lat0 = map_app_get_map_center_lat();

    /* Сдвиг вверх (север) на один шаг: центр карты изменился. */
    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_UP));
    const int32_t lat1 = map_app_get_map_center_lat();
    const int32_t lon1 = map_app_get_map_center_lon();
    CHECK(lat1 > lat0);

    /* Сдвиг сам по себе PURRGO.CFG не меняет: в файле ещё центр по умолчанию. */
    CHECK(cfg_value("LAST_LAT_1E7", &f) == lat0 && f);

    /* Любая запись конфига (меню, читалка, ...) приносит актуальный центр. */
    CHECK(purrgo_config_save());
    CHECK(cfg_value("LAST_LAT_1E7", &f) == lat1 && f);
    CHECK(cfg_value("LAST_LON_1E7", &f) == lon1 && f);
    CHECK(app_config.last_lat_1e7 == lat1);
    CHECK(app_config.last_lon_1e7 == lon1);

    /* Вторая запись после нового сдвига снова обновляет значения. */
    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_RIGHT));
    const int32_t lon2 = map_app_get_map_center_lon();
    CHECK(lon2 != lon1);
    CHECK(purrgo_config_save());
    CHECK(cfg_value("LAST_LON_1E7", &f) == lon2 && f);
}

static void test_restart_opens_saved_center(void)
{
    fake_fs_reset();
    boot();
    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_DOWN));
    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_LEFT));
    const int32_t lat = map_app_get_map_center_lat();
    const int32_t lon = map_app_get_map_center_lon();
    CHECK(purrgo_config_save());

    /* "Перезагрузка": состояние в RAM теряется, остаётся файл. */
    memset(&app_config, 0, sizeof app_config);
    boot();
    CHECK(map_app_get_map_center_lat() == lat);
    CHECK(map_app_get_map_center_lon() == lon);
}

static void test_other_setting_change_saves_center(void)
{
    int f;

    fake_fs_reset();
    boot();
    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_UP));
    const int32_t lat = map_app_get_map_center_lat();

    /* Типичная запись из меню: изменили настройку и сохранили. */
    app_config.tz_offset_minutes = 120;
    CHECK(purrgo_config_save());
    CHECK(cfg_value("TZ_MIN", &f) == 120 && f);
    CHECK(cfg_value("LAST_LAT_1E7", &f) == lat && f);
}

static void test_power_off_saves_center(void)
{
    int f;

    fake_fs_reset();
    fake_power_reset();
    boot();
    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_UP));
    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_RIGHT));
    const int32_t lat = map_app_get_map_center_lat();
    const int32_t lon = map_app_get_map_center_lon();

    purrgo_system_power_off();

    CHECK(fake_power_standby_calls() == 1);
    CHECK(cfg_value("LAST_LAT_1E7", &f) == lat && f);
    CHECK(cfg_value("LAST_LON_1E7", &f) == lon && f);
}

static void test_no_controller_leaves_values(void)
{
    int f;

    fake_fs_reset();
    boot();
    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_UP));

    /* purrgo_config_init() сбрасывает хук: значения из app_config пишутся как есть. */
    purrgo_config_init();
    app_config.last_lat_1e7 = 111000000;
    app_config.last_lon_1e7 = 222000000;
    CHECK(purrgo_config_save());
    CHECK(cfg_value("LAST_LAT_1E7", &f) == 111000000 && f);
    CHECK(cfg_value("LAST_LON_1E7", &f) == 222000000 && f);
}

static void test_reload_does_not_apply_stale_center(void)
{
    int f;

    fake_fs_reset();
    boot();
    CHECK(purrgo_map_controller_handle_button(PURRGO_ACTION_UP));
    CHECK(purrgo_config_save());

    /* Файла нет, а контроллер карты уже инициализирован раньше (повторный purrgo_app_init()):
     * записанные по умолчанию значения не должны подменяться устаревшим центром. */
    fake_fs_reset();
    purrgo_config_load();
    CHECK(cfg_value("LAST_LAT_1E7", &f) == 537135000 && f);
    CHECK(cfg_value("LAST_LON_1E7", &f) == 284199000 && f);
}

static void test_extreme_centers_round_trip(void)
{
    int f;

    fake_fs_reset();
    boot();
    /* Центр у полюса и у 180-го меридиана остаётся в допустимых пределах загрузчика. */
    for (int i = 0; i < 4000; i++) {
        if (!purrgo_map_controller_handle_button(PURRGO_ACTION_UP)) { break; }
    }
    CHECK(map_app_get_map_center_lat() <= 900000000);
    CHECK(purrgo_config_save());
    CHECK(cfg_value("LAST_LAT_1E7", &f) == map_app_get_map_center_lat() && f);

    memset(&app_config, 0, sizeof app_config);
    boot();
    CHECK(map_app_get_map_center_lat() == cfg_value("LAST_LAT_1E7", &f));
}

int main(void)
{
    test_default_center_when_no_file();
    test_save_stores_current_center();
    test_restart_opens_saved_center();
    test_other_setting_change_saves_center();
    test_power_off_saves_center();
    test_no_controller_leaves_values();
    test_reload_does_not_apply_stale_center();
    test_extreme_centers_round_trip();
    return test_finish("test_map_center_persist");
}
