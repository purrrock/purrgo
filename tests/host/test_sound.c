/*
 * Host-тесты звуковых сигналов (purrgo/sound.h, sound.c и места вызова в ядре):
 *   - настройка SOUND: OFF / LOW / HIGH, по умолчанию OFF, ключ SOUND=0|1|2 в PURRGO.CFG
 *     (чтение, мусор, запись);
 *   - при OFF платформе ничего не передаётся, в том числе ERROR и щелчок кнопок; при LOW и HIGH
 *     платформа получает выбранную громкость;
 *   - пункт SOUND главного меню: OK переключает OFF -> LOW -> HIGH, громкость действует сразу
 *     (щелчки при OFF молчат уже до BACK), в файл сохраняется по BACK; при выборе LOW/HIGH играется
 *     проверочный сигнал INFO, при выборе OFF звук обрывается;
 *   - ERROR при критическом разряде (supply.c): один раз за остановку;
 *   - остановка записи трека (сбой SD, переход в читалку, выключение) звука не даёт;
 *   - сбои железа (hw_fault.h): строка HW FAULT в лог и один сигнал ERROR за эпизод сбоя узла;
 *     OFF - молча; повтор того же узла - без сигнала до purrgo_hw_fault_clear(); просадка питания
 *     (сигнал разряда уже был) - без второго сигнала;
 *   - выключение обрывает звучащий сигнал (purrgo_sound_stop() до остановки логгера и до сна);
 *   - ARRIVED при входе в радиус цели (waypoint.c): один раз, повторный только после отхода
 *     дальше PURRGO_WPT_ARRIVAL_REARM_M; цель, заданная уже внутри радиуса, молчит;
 *   - INFO при первом фиксе GNSS после включения (app_fsm.c);
 *   - CLICK на нажатие кнопки (app_fsm.c).
 * Сам звук (TIM16) - платформенная часть STM32 и здесь не проверяется: платформенный хук
 * заменён записывающей заглушкой (tests/host/fake_power.c).
 */
#include <stdio.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include "purrgo/gnss_types.h"
#include "purrgo/hw_fault.h"
#include "purrgo/menu_controller.h"
#include "purrgo/power.h"
#include "purrgo/sound.h"
#include "purrgo/supply.h"
#include "purrgo/track_logger.h"
#include "purrgo/waypoint.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

static const char* CFG = "PURRGO.CFG";

/* Позиция пункта SOUND в главном меню (после SD LOG, перед MAP DETAILS). */
#define SOUND_ITEM_INDEX 2

static purrgo_state_t press(purrgo_state_t st, purrgo_action_t a)
{
    purrgo_state_t next = st;
    (void)purrgo_menu_controller_handle_button(st, a, &next);
    return next;
}

/* Из главного меню в подменю CONFIG (последний пункт главного меню). */
static purrgo_state_t enter_config(void)
{
    purrgo_state_t st = APP_STATE_MENU_MAIN;

    purrgo_menu_controller_on_enter(APP_STATE_MENU_MAIN);
    for (int i = 0; i < menu_app_get_main_count() - 1; i++) {
        st = press(st, PURRGO_ACTION_DOWN);
    }
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_CONFIG);
    return st;
}

static void feed(const char* body)
{
    char line[160];
    unsigned cs = 0;
    for (const char* p = body; *p; p++) { cs ^= (unsigned char)*p; }
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

/* Чистое состояние: пустая карта, свежий конфиг, счётчики сигналов обнулены. */
static void fresh(void)
{
    fake_fs_reset();
    fake_power_reset();
    purrgo_hw_fault_reset();
    purrgo_config_init();
    purrgo_supply_reset();
    purrgo_wpt_clear();
}


static void test_policy(void)
{
    fresh();
    CHECK(app_config.sound_level == PURRGO_SOUND_LEVEL_OFF);   /* по умолчанию выключено */

    /* OFF: платформа не получает ничего - ни ERROR, ни щелчок, ни остальные. */
    for (int snd = (int)PURRGO_SOUND_CLICK; snd < (int)PURRGO_SOUND_COUNT; snd++) {
        purrgo_sound_play((purrgo_sound_t)snd);
    }
    CHECK(fake_sound_total() == 0);

    /* Недопустимое значение громкости тоже молчит. */
    app_config.sound_level = (purrgo_sound_level_t)77;
    purrgo_sound_play(PURRGO_SOUND_ERROR);
    CHECK(fake_sound_total() == 0);

    app_config.sound_level = PURRGO_SOUND_LEVEL_LOW;
    purrgo_sound_play(PURRGO_SOUND_ARRIVED);
    CHECK(fake_sound_total() == 1);
    CHECK(fake_sound_last() == PURRGO_SOUND_ARRIVED);
    CHECK(fake_sound_last_level() == PURRGO_SOUND_LEVEL_LOW);

    app_config.sound_level = PURRGO_SOUND_LEVEL_HIGH;
    purrgo_sound_play(PURRGO_SOUND_ARRIVED);
    CHECK(fake_sound_total() == 2);
    CHECK(fake_sound_last_level() == PURRGO_SOUND_LEVEL_HIGH);

    /* Остановка доходит до платформы при любой громкости. */
    purrgo_sound_stop();
    CHECK(fake_sound_stops() == 1);

    CHECK(strcmp(purrgo_sound_level_name(PURRGO_SOUND_LEVEL_OFF), "OFF") == 0);
    CHECK(strcmp(purrgo_sound_level_name(PURRGO_SOUND_LEVEL_LOW), "LOW") == 0);
    CHECK(strcmp(purrgo_sound_level_name(PURRGO_SOUND_LEVEL_HIGH), "HIGH") == 0);
    CHECK(strcmp(purrgo_sound_level_name((purrgo_sound_level_t)9), "?") == 0);
    fake_sound_reset();

    /* Недопустимые значения отбрасываются. */
    app_config.sound_level = PURRGO_SOUND_LEVEL_HIGH;
    purrgo_sound_play(PURRGO_SOUND_NONE);
    purrgo_sound_play(PURRGO_SOUND_COUNT);
    purrgo_sound_play((purrgo_sound_t)99);
    CHECK(fake_sound_total() == 0);

    CHECK(strcmp(purrgo_sound_name(PURRGO_SOUND_ERROR), "ERROR") == 0);
    CHECK(strcmp(purrgo_sound_name((purrgo_sound_t)99), "?") == 0);

    /* Приоритет = порядок значений: ERROR важнее ARRIVED, ARRIVED важнее INFO, INFO важнее CLICK. */
    CHECK(PURRGO_SOUND_ERROR > PURRGO_SOUND_ARRIVED);
    CHECK(PURRGO_SOUND_ARRIVED > PURRGO_SOUND_INFO);
    CHECK(PURRGO_SOUND_INFO > PURRGO_SOUND_CLICK);
    CHECK(PURRGO_SOUND_CLICK > PURRGO_SOUND_NONE);
}

static void test_config_key(void)
{
    uint32_t n;
    const uint8_t* d;

    fresh();
    CHECK(app_config.sound_level == PURRGO_SOUND_LEVEL_OFF);

    /* Старый файл без ключа: выключено. */
    fake_fs_put(CFG, "TZ_MIN=60\n");
    CHECK(purrgo_config_load());
    CHECK(app_config.sound_level == PURRGO_SOUND_LEVEL_OFF);

    fake_fs_reset();
    fake_fs_put(CFG, "SOUND=1\n");
    CHECK(purrgo_config_load());
    CHECK(app_config.sound_level == PURRGO_SOUND_LEVEL_LOW);

    fake_fs_reset();
    fake_fs_put(CFG, "SOUND=2\n");
    CHECK(purrgo_config_load());
    CHECK(app_config.sound_level == PURRGO_SOUND_LEVEL_HIGH);

    fake_fs_reset();
    fake_fs_put(CFG, "SOUND=0\n");
    app_config.sound_level = PURRGO_SOUND_LEVEL_HIGH;
    CHECK(purrgo_config_load());
    CHECK(app_config.sound_level == PURRGO_SOUND_LEVEL_OFF);

    /* Мусор и недопустимые значения: остаётся значение по умолчанию (выключено). */
    fake_fs_reset();
    fake_fs_put(CFG, "SOUND=7\n");
    CHECK(purrgo_config_load());
    CHECK(app_config.sound_level == PURRGO_SOUND_LEVEL_OFF);
    fake_fs_reset();
    fake_fs_put(CFG, "SOUND=-1\n");
    CHECK(purrgo_config_load());
    CHECK(app_config.sound_level == PURRGO_SOUND_LEVEL_OFF);
    fake_fs_reset();
    fake_fs_put(CFG, "SOUND=abc\n");
    CHECK(purrgo_config_load());
    CHECK(app_config.sound_level == PURRGO_SOUND_LEVEL_OFF);

    /* Сохранение и повторное чтение всех трёх значений. */
    static const char* const keys[3] = { "SOUND=0\n", "SOUND=1\n", "SOUND=2\n" };
    for (int lvl = 0; lvl < 3; lvl++) {
        app_config.sound_level = (purrgo_sound_level_t)lvl;
        CHECK(purrgo_config_save());
        d = fake_fs_data(CFG, &n);
        CHECK(d != NULL && strstr((const char*)d, keys[lvl]) != NULL);
        app_config.sound_level = (purrgo_sound_level_t)((lvl + 1) % 3);
        CHECK(purrgo_config_load());
        CHECK(app_config.sound_level == (purrgo_sound_level_t)lvl);
    }
}

static void test_menu(void)
{
    char text[64];

    fresh();
    purrgo_state_t st = enter_config();
    CHECK(menu_app_get_draft_sound_level() == PURRGO_SOUND_LEVEL_OFF);

    for (int i = 0; i < SOUND_ITEM_INDEX; i++) {
        st = press(st, PURRGO_ACTION_DOWN);
    }
    CHECK(menu_app_get_config_cursor() == SOUND_ITEM_INDEX);
    menu_app_get_config_item_text(SOUND_ITEM_INDEX, text, sizeof text);
    CHECK(strcmp(text, "SOUND: OFF") == 0);

    /* OK: OFF -> LOW. Громкость действует сразу, играется проверочный сигнал на LOW. */
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_CONFIG);
    CHECK(menu_app_get_draft_sound_level() == PURRGO_SOUND_LEVEL_LOW);
    menu_app_get_config_item_text(SOUND_ITEM_INDEX, text, sizeof text);
    CHECK(strcmp(text, "SOUND: LOW") == 0);
    CHECK(app_config.sound_level == PURRGO_SOUND_LEVEL_LOW);
    CHECK(fake_sound_count(PURRGO_SOUND_INFO) == 1);
    CHECK(fake_sound_last_level() == PURRGO_SOUND_LEVEL_LOW);

    /* OK: LOW -> HIGH, проверочный сигнал на HIGH. */
    st = press(st, PURRGO_ACTION_OK);
    CHECK(menu_app_get_draft_sound_level() == PURRGO_SOUND_LEVEL_HIGH);
    menu_app_get_config_item_text(SOUND_ITEM_INDEX, text, sizeof text);
    CHECK(strcmp(text, "SOUND: HIGH") == 0);
    CHECK(app_config.sound_level == PURRGO_SOUND_LEVEL_HIGH);
    CHECK(fake_sound_count(PURRGO_SOUND_INFO) == 2);
    CHECK(fake_sound_last_level() == PURRGO_SOUND_LEVEL_HIGH);

    /* BACK сохраняет в файл, возврат в главное меню. */
    st = press(st, PURRGO_ACTION_MENU);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(app_config.sound_level == PURRGO_SOUND_LEVEL_HIGH);
    {
        uint32_t n;
        const uint8_t* d = fake_fs_data(CFG, &n);
        CHECK(d != NULL && strstr((const char*)d, "SOUND=2\n") != NULL);
    }

    /* Повторный вход: черновик из конфига. OK: HIGH -> OFF, звук обрывается и молчит сразу. */
    fake_sound_reset();
    st = enter_config();
    CHECK(menu_app_get_draft_sound_level() == PURRGO_SOUND_LEVEL_HIGH);
    for (int i = 0; i < SOUND_ITEM_INDEX; i++) {
        st = press(st, PURRGO_ACTION_DOWN);
    }
    fake_sound_reset();
    st = press(st, PURRGO_ACTION_OK);
    CHECK(menu_app_get_draft_sound_level() == PURRGO_SOUND_LEVEL_OFF);
    menu_app_get_config_item_text(SOUND_ITEM_INDEX, text, sizeof text);
    CHECK(strcmp(text, "SOUND: OFF") == 0);
    CHECK(app_config.sound_level == PURRGO_SOUND_LEVEL_OFF);   /* до BACK, а уже тихо */
    CHECK(fake_sound_total() - fake_sound_count(PURRGO_SOUND_CLICK) == 0);   /* проверочного сигнала нет */
    CHECK(fake_sound_stops() == 1);

    /* Пока в меню, кнопки молчат (раньше щёлкали до BACK, включая сам BACK). */
    fake_sound_reset();
    st = press(st, PURRGO_ACTION_DOWN);
    purrgo_app_handle_button(PURRGO_BTN_KEY1_SHORT);
    CHECK(fake_sound_total() == 0);

    st = press(st, PURRGO_ACTION_MENU);
    CHECK(app_config.sound_level == PURRGO_SOUND_LEVEL_OFF);
    {
        uint32_t n;
        const uint8_t* d = fake_fs_data(CFG, &n);
        CHECK(d != NULL && strstr((const char*)d, "SOUND=0\n") != NULL);
    }
    CHECK(fake_sound_total() == 0);
}

static void test_supply_error(void)
{
    fresh();
    g_now = 500000;
    app_config.sound_level = PURRGO_SOUND_LEVEL_HIGH;

    /* Короткая просадка короче PURRGO_SUPPLY_LOW_CONFIRM_MS: ни остановки, ни сигнала. */
    fake_power_set_supply_low(true);
    (void)purrgo_supply_tick(g_now);
    g_now += PURRGO_SUPPLY_LOW_CONFIRM_MS / 2u;
    (void)purrgo_supply_tick(g_now);
    CHECK(!purrgo_supply_storage_blocked());
    CHECK(fake_sound_count(PURRGO_SOUND_ERROR) == 0);

    /* Устойчивая просадка: остановка записи и один длинный сигнал. */
    g_now += PURRGO_SUPPLY_LOW_CONFIRM_MS;
    (void)purrgo_supply_tick(g_now);
    CHECK(purrgo_supply_storage_blocked());
    CHECK(fake_sound_count(PURRGO_SOUND_ERROR) == 1);
    CHECK(fake_sound_total() == 1);
    CHECK(fake_sound_last_level() == PURRGO_SOUND_LEVEL_HIGH);

    /* Пока блокировка держится, сигнал не повторяется. */
    for (int i = 0; i < 20; i++) {
        g_now += 1000;
        (void)purrgo_supply_tick(g_now);
    }
    CHECK(fake_sound_count(PURRGO_SOUND_ERROR) == 1);

    /* OFF: остановка записи происходит, но молча. */
    fresh();
    g_now += 100000;
    fake_power_set_supply_low(true);
    (void)purrgo_supply_tick(g_now);
    g_now += PURRGO_SUPPLY_LOW_CONFIRM_MS;
    (void)purrgo_supply_tick(g_now);
    CHECK(purrgo_supply_storage_blocked());
    CHECK(fake_sound_total() == 0);
    fake_power_set_supply_low(false);
}

/* Решение GNSS на расстоянии dlat_1e7 (1000 = 0,0001 градуса = около 11 м) севернее цели. */
static void wpt_fix(purrgo_gnss_solution_t* fix, int32_t dlat_1e7)
{
    memset(fix, 0, sizeof *fix);
    fix->valid = true;
    fix->lat_1e7 = 539000000 + dlat_1e7;
    fix->lon_1e7 = 275590000;
}

static void test_arrival(void)
{
    purrgo_gnss_solution_t fix;

    fresh();
    app_config.sound_level = PURRGO_SOUND_LEVEL_HIGH;
    CHECK(purrgo_wpt_set(539000000, 275590000, "T"));

    /* Далеко (около 110 м): сигнала нет. */
    wpt_fix(&fix, 10000);
    purrgo_wpt_update(&fix);
    CHECK(!purrgo_wpt_nav()->nav.is_arrived);
    CHECK(fake_sound_total() == 0);

    /* Вход в радиус (около 11 м): один двойной сигнал. */
    wpt_fix(&fix, 1000);
    purrgo_wpt_update(&fix);
    CHECK(purrgo_wpt_nav()->nav.is_arrived);
    CHECK(fake_sound_count(PURRGO_SOUND_ARRIVED) == 1);

    /* Остался в радиусе: без повторов. */
    for (int i = 0; i < 10; i++) { purrgo_wpt_update(&fix); }
    CHECK(fake_sound_count(PURRGO_SOUND_ARRIVED) == 1);

    /* Нет фикса: состояние сохраняется, сигнала нет. */
    {
        purrgo_gnss_solution_t nofix;
        wpt_fix(&nofix, 1000);
        nofix.valid = false;
        purrgo_wpt_update(&nofix);
        CHECK(!purrgo_wpt_nav()->valid);
        purrgo_wpt_update(&fix);
        CHECK(fake_sound_count(PURRGO_SOUND_ARRIVED) == 1);
    }

    /* Гистерезис: вышел за радиус (около 22 м), но не дальше PURRGO_WPT_ARRIVAL_REARM_M (30 м). */
    wpt_fix(&fix, 2000);
    purrgo_wpt_update(&fix);
    CHECK(!purrgo_wpt_nav()->nav.is_arrived);
    wpt_fix(&fix, 1000);
    purrgo_wpt_update(&fix);
    CHECK(fake_sound_count(PURRGO_SOUND_ARRIVED) == 1);   /* дрожание границы: без сигнала */

    /* Отошёл далеко (около 55 м) и вернулся: второй сигнал. */
    wpt_fix(&fix, 5000);
    purrgo_wpt_update(&fix);
    wpt_fix(&fix, 1000);
    purrgo_wpt_update(&fix);
    CHECK(fake_sound_count(PURRGO_SOUND_ARRIVED) == 2);

    /* Цель задана, когда человек уже в радиусе (например HERE): сигнала нет. */
    fake_sound_reset();
    CHECK(purrgo_wpt_set(539000000, 275590000, "HERE"));
    wpt_fix(&fix, 500);
    purrgo_wpt_update(&fix);
    CHECK(purrgo_wpt_nav()->nav.is_arrived);
    CHECK(fake_sound_total() == 0);
    /* ...а после отхода и возврата сигнал есть. */
    wpt_fix(&fix, 5000);
    purrgo_wpt_update(&fix);
    wpt_fix(&fix, 500);
    purrgo_wpt_update(&fix);
    CHECK(fake_sound_count(PURRGO_SOUND_ARRIVED) == 1);

    /* Новая цель сбрасывает состояние: вход в радиус новой цели снова сигналит. */
    fake_sound_reset();
    CHECK(purrgo_wpt_set(539100000, 275590000, "N"));
    wpt_fix(&fix, 500);
    purrgo_wpt_update(&fix);                 /* далеко от новой цели (около 1,1 км) */
    CHECK(fake_sound_total() == 0);
    wpt_fix(&fix, 100000);                   /* 0,01 градуса севернее: в радиусе новой цели */
    purrgo_wpt_update(&fix);
    CHECK(fake_sound_count(PURRGO_SOUND_ARRIVED) == 1);

    /* Звук выключен: прибытие молча. */
    fake_sound_reset();
    app_config.sound_level = PURRGO_SOUND_LEVEL_OFF;
    purrgo_wpt_clear();
    CHECK(purrgo_wpt_set(539000000, 275590000, "Q"));
    wpt_fix(&fix, 10000);
    purrgo_wpt_update(&fix);
    wpt_fix(&fix, 100);
    purrgo_wpt_update(&fix);
    CHECK(purrgo_wpt_nav()->nav.is_arrived);
    CHECK(fake_sound_total() == 0);

    purrgo_wpt_clear();
}

static void test_first_fix_and_click(void)
{
    fresh();
    g_now = 800000;
    purrgo_app_init();
    app_config.sound_level = PURRGO_SOUND_LEVEL_LOW;   /* после init: он загружает конфиг */
    fake_sound_reset();

    /* Нет фикса - нет сигнала (тики без подачи данных GNSS). */
    for (int i = 0; i < 300; i++) {
        g_now += 10;
        purrgo_app_tick(g_now);
    }
    CHECK(!purrgo_app_get_gnss_solution()->valid);
    CHECK(fake_sound_count(PURRGO_SOUND_INFO) == 0);

    /* Первый фикс после включения: один короткий сигнал. */
    feed_epoch();
    run_ms(5000);
    CHECK(purrgo_app_get_gnss_solution()->valid);
#if PURRGO_SOUND_FIX_BEEP
    CHECK(fake_sound_count(PURRGO_SOUND_INFO) == 1);
    /* Дальше фикс обновляется, но повторных сигналов нет. */
    run_ms(70000);
    CHECK(fake_sound_count(PURRGO_SOUND_INFO) == 1);
#else
    CHECK(fake_sound_count(PURRGO_SOUND_INFO) == 0);
#endif

    /* Нажатие кнопки: тихий щелчок на каждое событие. */
    fake_sound_reset();
    purrgo_app_handle_button(PURRGO_BTN_KEY1_SHORT);
    CHECK(fake_sound_count(PURRGO_SOUND_CLICK) == 1);
    purrgo_app_handle_button(PURRGO_BTN_KEY2_LONG);
    CHECK(fake_sound_count(PURRGO_SOUND_CLICK) == 2);
    CHECK(fake_sound_total() == 2);

    /* OFF: кнопки молча. */
    fake_sound_reset();
    app_config.sound_level = PURRGO_SOUND_LEVEL_OFF;
    purrgo_app_handle_button(PURRGO_BTN_KEY1_SHORT);
    purrgo_app_handle_button(PURRGO_BTN_KEY2_LONG);
    CHECK(fake_sound_total() == 0);
}

/* Остановка записи трека (читалка, выключение, смена режима) звука не даёт. */
static void test_logger_stop_silent(void)
{
    fresh();
    g_now = 1200000;
    purrgo_app_init();
    app_config.sound_level = PURRGO_SOUND_LEVEL_HIGH;
    feed_epoch();
    run_ms(5000);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    fake_sound_reset();

    /* Обычная остановка записи, как при переходе в читалку или выключении. */
    purrgo_logger_stop();
    run_ms(3000);
    CHECK(fake_sound_count(PURRGO_SOUND_ERROR) == 0);

    /* Запись снова пошла: тоже тихо (новый трек не сигналит). */
    run_ms(5000);
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    CHECK(fake_sound_count(PURRGO_SOUND_ERROR) == 0);
}

/* Выключение: звучащий сигнал обрывается до остановки логгера и ещё раз перед сном. */
static void test_power_off_stops_sound(void)
{
    fresh();
    g_now = 2000000;
    app_config.sound_level = PURRGO_SOUND_LEVEL_HIGH;
    purrgo_sound_play(PURRGO_SOUND_ERROR);
    CHECK(fake_sound_stops() == 0);
    purrgo_system_power_off();
    CHECK(fake_sound_stops() >= 2);
    CHECK(fake_power_standby_calls() == 1);
}

/* Сбои железа: один сигнал ERROR за эпизод сбоя узла (hw_fault.h). */
static void test_hw_fault(void)
{
    fresh();
    g_now = 3000000;

    /* OFF: сбой фиксируется (лог, эпизод), но не звучит. */
    purrgo_hw_fault(PURRGO_HWF_SD, "I/O error %d", 5);
    CHECK(purrgo_hw_fault_active(PURRGO_HWF_SD));
    purrgo_hw_fault_service();
    CHECK(fake_sound_total() == 0);
    purrgo_hw_fault_clear(PURRGO_HWF_SD);
    CHECK(!purrgo_hw_fault_active(PURRGO_HWF_SD));

    /* LOW и HIGH: сообщение само не звучит, звук играет главный цикл (purrgo_hw_fault_service). */
    app_config.sound_level = PURRGO_SOUND_LEVEL_LOW;
    purrgo_hw_fault(PURRGO_HWF_SD, "mount error %d", 13);
    CHECK(fake_sound_total() == 0);
    purrgo_hw_fault_service();
    CHECK(fake_sound_count(PURRGO_SOUND_ERROR) == 1);
    CHECK(fake_sound_total() == 1);
    CHECK(fake_sound_last_level() == PURRGO_SOUND_LEVEL_LOW);

    /* Тот же узел продолжает сбоить (повторные попытки монтирования): без новых сигналов. */
    for (int i = 0; i < 10; i++) {
        purrgo_hw_fault(PURRGO_HWF_SD, "mount error %d", 13);
        purrgo_hw_fault_service();
    }
    CHECK(fake_sound_total() == 1);

    /* Новый сбой другого узла - свой эпизод и свой сигнал. */
    purrgo_hw_fault(PURRGO_HWF_GNSS, "link failed");
    purrgo_hw_fault_service();
    CHECK(fake_sound_total() == 2);

    /* Два узла сбойнули до главного цикла: один сигнал на всех. */
    fake_sound_reset();
    purrgo_hw_fault(PURRGO_HWF_DISPLAY, "op failed");
    purrgo_hw_fault(PURRGO_HWF_BATTERY, "ADC read failed");
    purrgo_hw_fault_service();
    purrgo_hw_fault_service();
    CHECK(fake_sound_count(PURRGO_SOUND_ERROR) == 1);

    /* Узел починился: следующий сбой звучит заново. */
    fake_sound_reset();
    purrgo_hw_fault_clear(PURRGO_HWF_SD);
    CHECK(!purrgo_hw_fault_active(PURRGO_HWF_SD));
    purrgo_hw_fault(PURRGO_HWF_SD, "I/O error %d", 1);
    purrgo_hw_fault_service();
    CHECK(fake_sound_count(PURRGO_SOUND_ERROR) == 1);

    /* Сигнал играет purrgo_app_tick(): не нужно звать сервис отдельно. */
    fake_sound_reset();
    purrgo_hw_fault_clear(PURRGO_HWF_SYSTEM);
    purrgo_hw_fault(PURRGO_HWF_SYSTEM, "LPTIM1 timebase is not running");
    g_now += 100;
    purrgo_app_tick(g_now);
    CHECK(fake_sound_count(PURRGO_SOUND_ERROR) == 1);

    /* OFF в момент сбоя: молча, и сигнал не копится "на потом". */
    fake_sound_reset();
    purrgo_hw_fault_reset();
    app_config.sound_level = PURRGO_SOUND_LEVEL_OFF;
    purrgo_hw_fault(PURRGO_HWF_GNSS, "no data");
    purrgo_hw_fault_service();
    app_config.sound_level = PURRGO_SOUND_LEVEL_HIGH;
    purrgo_hw_fault_service();
    CHECK(fake_sound_total() == 0);

    /* Неверный узел: в лог пишется, в учёт не попадает, ничего не ломается. */
    purrgo_hw_fault((purrgo_hw_t)77, "bad node");
    purrgo_hw_fault_clear((purrgo_hw_t)77);
    CHECK(!purrgo_hw_fault_active((purrgo_hw_t)77));
    CHECK(strcmp(purrgo_hw_name(PURRGO_HWF_SD), "SD") == 0);
    CHECK(strcmp(purrgo_hw_name(PURRGO_HWF_GNSS), "GNSS") == 0);
    CHECK(strcmp(purrgo_hw_name((purrgo_hw_t)77), "?") == 0);
}

/* Критический разряд уже просигналил: сбои железа от просадки питания второго сигнала не дают. */
static void test_hw_fault_during_supply_low(void)
{
    fresh();
    g_now = 4000000;
    app_config.sound_level = PURRGO_SOUND_LEVEL_HIGH;

    fake_power_set_supply_low(true);
    (void)purrgo_supply_tick(g_now);
    g_now += PURRGO_SUPPLY_LOW_CONFIRM_MS;
    (void)purrgo_supply_tick(g_now);
    CHECK(purrgo_supply_storage_blocked());
    CHECK(fake_sound_count(PURRGO_SOUND_ERROR) == 1);

    purrgo_hw_fault(PURRGO_HWF_SD, "I/O error %d", 3);
    purrgo_hw_fault_service();
    CHECK(fake_sound_count(PURRGO_SOUND_ERROR) == 1);   /* только сигнал разряда */
    CHECK(purrgo_hw_fault_active(PURRGO_HWF_SD));       /* но в учёте и в логе сбой есть */
    fake_power_set_supply_low(false);
}

int main(void)
{
    test_policy();
    test_config_key();
    test_menu();
    test_supply_error();
    test_arrival();
    test_first_fix_and_click();
    test_logger_stop_silent();
    test_power_off_stops_sound();
    test_hw_fault();
    test_hw_fault_during_supply_low();
    return test_finish("test_sound");
}
