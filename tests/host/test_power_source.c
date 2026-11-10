/*
 * Host-тесты выбора источника питания:
 *   - значение по умолчанию (Ni-MH), чтение и запись POWER_SRC в PURRGO.CFG;
 *   - порядок пунктов главного меню: BOOKS, POWER SOURCE, POWER OFF, TRACKER (SCREEN OFF), TZ, ..., MAP LAYERS;
 *   - OK по пункту POWER SOURCE перебирает шесть источников по кругу, MENU применяет
 *     выбор и сохраняет его (до MENU app_config не меняется);
 *   - кривые разряда: границы, монотонность, разные результаты для разных
 *     источников при одном напряжении, питание 5 В без процентов;
 *   - purrgo_battery_read() использует источник из app_config; напряжение
 *     на выводе АЦП (хук платформы) пересчитывается по делителю.
 */
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/battery.h"
#include "purrgo/config.h"
#include "purrgo/menu_controller.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

static const char* CFG = "PURRGO.CFG";

static purrgo_state_t press(purrgo_state_t st, purrgo_action_t a)
{
    purrgo_state_t next = st;
    (void)purrgo_menu_controller_handle_button(st, a, &next);
    return next;
}

static void test_config_key(void)
{
    uint32_t n;
    const uint8_t* d;

    fake_fs_reset();
    purrgo_config_init();
    CHECK(app_config.power_source == PURRGO_POWER_SRC_NIMH);

    /* Старый файл без ключа POWER_SRC: остаётся значение по умолчанию. */
    fake_fs_put(CFG, "TZ_MIN=60\n");
    CHECK(purrgo_config_load());
    CHECK(app_config.power_source == PURRGO_POWER_SRC_DEFAULT);

    /* Все допустимые значения читаются. */
    for (int i = 0; i < PURRGO_POWER_SRC_COUNT; i++) {
        char text[32];
        text[0] = '\0';
        strcat(text, "POWER_SRC=");
        char digit[2] = { (char)('0' + i), '\0' };
        strcat(text, digit);
        strcat(text, "\n");
        fake_fs_reset();
        fake_fs_put(CFG, text);
        CHECK(purrgo_config_load());
        CHECK((int)app_config.power_source == i);
    }

    /* Недопустимые значения игнорируются. */
    fake_fs_reset();
    fake_fs_put(CFG, "POWER_SRC=9\n");
    CHECK(purrgo_config_load());
    CHECK(app_config.power_source == PURRGO_POWER_SRC_DEFAULT);
    fake_fs_reset();
    fake_fs_put(CFG, "POWER_SRC=-1\n");
    CHECK(purrgo_config_load());
    CHECK(app_config.power_source == PURRGO_POWER_SRC_DEFAULT);

    /* Сохранение. */
    app_config.power_source = PURRGO_POWER_SRC_ALKALINE;
    CHECK(purrgo_config_save());
    d = fake_fs_data(CFG, &n);
    CHECK(d != NULL && strstr((const char*)d, "POWER_SRC=1\n") != NULL);
    app_config.power_source = PURRGO_POWER_SRC_NIMH;
    CHECK(purrgo_config_load());
    CHECK(app_config.power_source == PURRGO_POWER_SRC_ALKALINE);
}

static void test_menu(void)
{
    fake_fs_reset();
    purrgo_config_init();
    app_config.power_source = PURRGO_POWER_SRC_LIION_AA;

    purrgo_menu_controller_on_enter(APP_STATE_MENU_MAIN);

    /* Первый пункт - BOOKS (режим чтения, см. test_book_reader.c), курсор стоит на нём. */
    CHECK(menu_app_get_main_cursor() == 0);
    purrgo_state_t st = APP_STATE_MENU_MAIN;
    CHECK(menu_app_get_draft_power_source() == PURRGO_POWER_SRC_LIION_AA);

    /* Второй пункт - источник питания. */
    st = press(st, PURRGO_ACTION_DOWN);
    CHECK(menu_app_get_main_cursor() == 1);
    CHECK(menu_app_get_draft_power_source() == PURRGO_POWER_SRC_LIION_AA);

    /* OK перебирает источники по кругу: LI-ION AA -> LITHIUM -> 5V -> ZINC -> ALKALINE -> NIMH */
    st = press(st, PURRGO_ACTION_OK);
    CHECK(menu_app_get_draft_power_source() == PURRGO_POWER_SRC_LITHIUM);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(menu_app_get_draft_power_source() == PURRGO_POWER_SRC_EXTERNAL_5V);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(menu_app_get_draft_power_source() == PURRGO_POWER_SRC_ZINC_CARBON);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(menu_app_get_draft_power_source() == PURRGO_POWER_SRC_ALKALINE);
    st = press(st, PURRGO_ACTION_OK);
    CHECK(menu_app_get_draft_power_source() == PURRGO_POWER_SRC_NIMH);
    CHECK(st == APP_STATE_MENU_MAIN);

    /* До MENU конфиг не меняется. */
    CHECK(app_config.power_source == PURRGO_POWER_SRC_LIION_AA);

    /*
     * Порядок остальных пунктов: 2 POWER OFF, 3 TRACKER (SCREEN OFF), 4 TZ, ...,
     * 11 MAP LAYERS - последний (WAYPOINT добавлен после ROUTE).
     */
    st = press(st, PURRGO_ACTION_DOWN);
    CHECK(menu_app_get_main_cursor() == 2);
    st = press(st, PURRGO_ACTION_DOWN);
    CHECK(menu_app_get_main_cursor() == 3);                 /* TRACKER: OK не нажимаем */
    st = press(st, PURRGO_ACTION_DOWN);
    CHECK(menu_app_get_main_cursor() == 4);
    const int16_t tz0 = menu_app_get_draft_tz_offset();
    st = press(st, PURRGO_ACTION_OK);
    CHECK(menu_app_get_draft_tz_offset() == tz0 + 15);      /* пункт 4 - TZ */
    for (int i = 0; i < 10; i++) {   /* с запасом: курсор упирается в последний пункт */
        st = press(st, PURRGO_ACTION_DOWN);
    }
    CHECK(menu_app_get_main_cursor() == menu_app_get_main_count() - 1);
    CHECK(menu_app_get_main_count() == 12);
    st = press(st, PURRGO_ACTION_OK);                          /* MAP LAYERS */
    CHECK(st == APP_STATE_MENU_MAP_LAYERS);
    st = press(st, PURRGO_ACTION_MENU);                        /* назад в CONFIG */
    CHECK(st == APP_STATE_MENU_MAIN);

    /* Новый вход в меню: черновик заново из конфига, курсор на первом пункте. */
    purrgo_menu_controller_on_enter(APP_STATE_MENU_MAIN);
    CHECK(menu_app_get_main_cursor() == 0);
    CHECK(menu_app_get_draft_power_source() == PURRGO_POWER_SRC_LIION_AA);

    /* Выбор 5V и MENU: применяется и сохраняется. */
    st = APP_STATE_MENU_MAIN;
    st = press(st, PURRGO_ACTION_DOWN);                        /* BOOKS -> POWER */
    st = press(st, PURRGO_ACTION_OK);                          /* LI-ION AA -> LITHIUM */
    st = press(st, PURRGO_ACTION_OK);                          /* LITHIUM -> 5V */
    st = press(st, PURRGO_ACTION_MENU);
    CHECK(st == APP_STATE_MAP);
    CHECK(app_config.power_source == PURRGO_POWER_SRC_EXTERNAL_5V);
    uint32_t n;
    const uint8_t* d = fake_fs_data(CFG, &n);
    CHECK(d != NULL && strstr((const char*)d, "POWER_SRC=5\n") != NULL);
}

static void test_curves(void)
{
    const uint32_t cells = PURRGO_BATTERY_CELLS;

    for (int src = 0; src < PURRGO_POWER_SRC_EXTERNAL_5V; src++) {
        /* Граничные значения. */
        CHECK(purrgo_battery_percent_from_mv((purrgo_power_source_t)src, 0) == 0);
        CHECK(purrgo_battery_percent_from_mv((purrgo_power_source_t)src, 3u * 2500u) == 100);

        /* Монотонность: больше напряжение - не меньше заряд. */
        int prev = 0;
        for (uint32_t mv = 0; mv <= 3u * 2000u; mv += 10u) {
            int pc = purrgo_battery_percent_from_mv((purrgo_power_source_t)src, mv);
            CHECK(pc >= 0 && pc <= 100);
            CHECK(pc >= prev);
            prev = pc;
        }
    }

    /* Питание 5 В и неизвестный источник: процентов нет. */
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_EXTERNAL_5V, 4800) == -1);
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_COUNT, 4000) == -1);

    /* Один и тот же уровень 1,25 В на элемент даёт разный заряд: у Ni-MH
     * кривая плоская и этот уровень ещё "середина", у солевых и щелочных - уже
     * близко к концу, у литиевых - почти пусто. */
    const uint32_t v = 1250u * cells;
    const int zinc = purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_ZINC_CARBON, v);
    const int alk  = purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_ALKALINE, v);
    const int nimh = purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_NIMH, v);
    const int lith = purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_LIION_AA, v);
    CHECK(nimh == 60);
    CHECK(zinc != alk);
    CHECK(nimh > zinc && nimh > alk);
    CHECK(lith < zinc);

    /* Li-ion AA со стабилизатором 1,5 В: "ступенька" - 100 % пока напряжение
     * держится, быстрое падение к 0 в конце. */
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_LIION_AA, 3u * 1510u) == 100);
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_LIION_AA, 3u * 1483u) == 100);  /* минимум на плато */
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_LIION_AA, 3u * 1400u) == 5);    /* первое падение */
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_LIION_AA, 3u * 1366u) == 4);    /* ступень 1,37 В */
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_LIION_AA, 3u * 824u) == 1);     /* ступень 0,82 В */
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_LIION_AA, 3u * 680u) == 0);     /* перед отключением */

    /* Солевые: точки снятой кривой (полный разряд, 18,96 ч). */
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_ZINC_CARBON, 3u * 1600u) == 100);
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_ZINC_CARBON, 3u * 1290u) == 50);
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_ZINC_CARBON, 3u * 1120u) == 14);
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_ZINC_CARBON, 3u * 800u) == 0);

    /* Литиевые батарейки (Li-FeS2): свежий элемент выше 1,5 В, плато 1,5-1,6 В,
     * на 1,25 В почти пусто; кривая отличается от Li-ion AA. */
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_LITHIUM, 3u * 1800u) == 100);
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_LITHIUM, 3u * 1600u) == 65);
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_LITHIUM, 3u * 1500u) == 25);
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_LITHIUM, 3u * 1200u) == 0);
    /* 1,5 В: у Li-ion AA со стабилизатором ещё 100 %, у батарейки - уже 25 %. */
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_LIION_AA, 3u * 1500u) == 100);
    CHECK(purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_LITHIUM, 3u * 1500u) <
          purrgo_battery_percent_from_mv(PURRGO_POWER_SRC_LIION_AA, 3u * 1500u));

    /* Заголовки для меню. */
    CHECK(strcmp(purrgo_power_source_title(PURRGO_POWER_SRC_LIION_AA), "LI-ION AA") == 0);
    CHECK(strcmp(purrgo_power_source_title(PURRGO_POWER_SRC_LITHIUM), "LITHIUM") == 0);
    CHECK(strcmp(purrgo_power_source_title(PURRGO_POWER_SRC_EXTERNAL_5V), "5V") == 0);
    CHECK(strcmp(purrgo_power_source_title(PURRGO_POWER_SRC_NIMH), "NIMH") == 0);
    CHECK(strcmp(purrgo_power_source_title(PURRGO_POWER_SRC_COUNT), "?") == 0);
}

static void test_read(void)
{
    purrgo_battery_t b;

    purrgo_config_init();

    /* Батареи: заглушка (напряжение не измеряется), external = false. */
    for (int src = 0; src < PURRGO_POWER_SRC_EXTERNAL_5V; src++) {
        app_config.power_source = (purrgo_power_source_t)src;
        CHECK(purrgo_battery_read(&b));
        CHECK(b.percent == 100 && b.millivolts == 0 && !b.external);
    }

    /* 5V: процентов нет, питание внешнее. */
    app_config.power_source = PURRGO_POWER_SRC_EXTERNAL_5V;
    CHECK(!purrgo_battery_read(&b));
    CHECK(b.percent == -1 && b.external);

    CHECK(!purrgo_battery_read(NULL));
}

static void test_pin_voltage(void)
{
    purrgo_battery_t b;

    /* Делитель 1 МΩ + 1 МΩ, поправка 1000: напряжение блока вдвое больше, чем на PB1. */
    CHECK(purrgo_battery_pack_mv_from_pin(0) == 0);
    {
        /* Делитель и поправка из hardware_config.h: пересчёт туда-обратно сходится (+-3 мВ). */
        const uint32_t back = purrgo_battery_pack_mv_from_pin(fake_power_pin_for_pack_mv(4500));
        CHECK(back >= 4497 && back <= 4503);
        CHECK(purrgo_battery_pack_mv_from_pin(2000) > purrgo_battery_pack_mv_from_pin(1999));
    }

    purrgo_config_init();

    /* Измеренное напряжение выбирает процент по кривой выбранного источника. */
    fake_power_reset();
    fake_power_set_battery_pin_mv(fake_power_pin_for_pack_mv(3750));   /* 1250 мВ на элемент */
    app_config.power_source = PURRGO_POWER_SRC_NIMH;
    CHECK(purrgo_battery_read(&b));
    CHECK(b.millivolts >= 3747 && b.millivolts <= 3753 && b.percent == 60 && !b.external);

    app_config.power_source = PURRGO_POWER_SRC_ZINC_CARBON;
    CHECK(purrgo_battery_read(&b));
    CHECK(b.millivolts >= 3747 && b.millivolts <= 3753 && b.percent == 38);

    app_config.power_source = PURRGO_POWER_SRC_LIION_AA;
    CHECK(purrgo_battery_read(&b));
    CHECK(b.percent == 4);

    /* 5V: напряжение на выводе не важно, процентов нет. */
    app_config.power_source = PURRGO_POWER_SRC_EXTERNAL_5V;
    CHECK(!purrgo_battery_read(&b));
    CHECK(b.percent == -1 && b.millivolts == 0 && b.external);

    /* Не измерено (0): заглушка 100 %. */
    fake_power_set_battery_pin_mv(0);
    app_config.power_source = PURRGO_POWER_SRC_NIMH;
    CHECK(purrgo_battery_read(&b));
    CHECK(b.percent == 100 && b.millivolts == 0);
}

int main(void)
{
    test_config_key();
    test_menu();
    test_curves();
    test_read();
    test_pin_voltage();
    return test_finish("test_power_source");
}
