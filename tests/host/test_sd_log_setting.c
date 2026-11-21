/*
 * Host-тесты настройки "запись лога на SD" (SD LOG):
 *   - значение по умолчанию (выключено), ключ SD_LOG в PURRGO.CFG: чтение, мусор игнорируется, запись;
 *   - пункт SD LOG главного меню: OK переключает черновик, конфиг и логгер меняются только по BACK (MENU),
 *     настройка сохраняется в PURRGO.CFG;
 *   - purrgo_app_init() включает запись лога на SD, если она включена в конфигурации.
 * Сама запись на карту (кольцо сообщений, файл LOG.TXT) - платформенная часть STM32 и здесь не проверяется.
 */
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include "purrgo/logger.h"
#include "purrgo/menu_controller.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

uint32_t purrgo_system_time_ms(void) { return 0; }

static const char* CFG = "PURRGO.CFG";

/* Позиция пункта SD LOG в подменю CONFIG (после LOG, перед MAP DIR). */
#define SD_LOG_ITEM_INDEX 4

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

static void test_config_key(void)
{
    uint32_t n;
    const uint8_t* d;

    fake_fs_reset();
    purrgo_config_init();
    CHECK(!app_config.sd_log);

    /* Старый файл без ключа: остаётся выключено. */
    fake_fs_put(CFG, "TZ_MIN=60\n");
    CHECK(purrgo_config_load());
    CHECK(!app_config.sd_log);

    fake_fs_reset();
    fake_fs_put(CFG, "SD_LOG=1\n");
    CHECK(purrgo_config_load());
    CHECK(app_config.sd_log);

    fake_fs_reset();
    fake_fs_put(CFG, "SD_LOG=0\n");
    app_config.sd_log = true;
    CHECK(purrgo_config_load());
    CHECK(!app_config.sd_log);

    /* Мусор и недопустимые значения игнорируются: остаётся значение по умолчанию (выключено). */
    fake_fs_reset();
    app_config.sd_log = true;
    fake_fs_put(CFG, "SD_LOG=7\n");
    CHECK(purrgo_config_load());
    CHECK(!app_config.sd_log);
    fake_fs_reset();
    app_config.sd_log = true;
    fake_fs_put(CFG, "SD_LOG=abc\n");
    CHECK(purrgo_config_load());
    CHECK(!app_config.sd_log);

    /* Сохранение и повторное чтение. */
    app_config.sd_log = true;
    CHECK(purrgo_config_save());
    d = fake_fs_data(CFG, &n);
    CHECK(d != NULL && strstr((const char*)d, "SD_LOG=1\n") != NULL);
    app_config.sd_log = false;
    CHECK(purrgo_config_load());
    CHECK(app_config.sd_log);

    app_config.sd_log = false;
    CHECK(purrgo_config_save());
    d = fake_fs_data(CFG, &n);
    CHECK(d != NULL && strstr((const char*)d, "SD_LOG=0\n") != NULL);
}

static void test_menu(void)
{
    char text[64];

    fake_fs_reset();
    purrgo_config_init();
    purrgo_logger_sd_set_enabled(false);
    CHECK(!app_config.sd_log && !purrgo_logger_sd_enabled());

    purrgo_state_t st = enter_config();
    CHECK(!menu_app_get_draft_sd_log());

    /* Пункт найден по тексту строки (а не только по номеру): "SD LOG: OFF". */
    for (int i = 0; i < SD_LOG_ITEM_INDEX; i++) {
        st = press(st, PURRGO_ACTION_DOWN);
    }
    CHECK(menu_app_get_config_cursor() == SD_LOG_ITEM_INDEX);
    menu_app_get_config_item_text(SD_LOG_ITEM_INDEX, text, sizeof text);
    CHECK(strcmp(text, "SD LOG: OFF") == 0);

    /* OK переключает черновик, конфиг и логгер пока не тронуты. */
    st = press(st, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_CONFIG);
    CHECK(menu_app_get_draft_sd_log());
    menu_app_get_config_item_text(SD_LOG_ITEM_INDEX, text, sizeof text);
    CHECK(strcmp(text, "SD LOG: ON") == 0);
    CHECK(!app_config.sd_log);
    CHECK(!purrgo_logger_sd_enabled());

    /* BACK применяет и сохраняет, возврат в главное меню. */
    st = press(st, PURRGO_ACTION_MENU);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(app_config.sd_log);
    CHECK(purrgo_logger_sd_enabled());
    {
        uint32_t n;
        const uint8_t* d = fake_fs_data(CFG, &n);
        CHECK(d != NULL && strstr((const char*)d, "SD_LOG=1\n") != NULL);
    }

    /* Повторный вход: черновик из конфига; выключение тоже применяется по BACK. */
    st = enter_config();
    CHECK(menu_app_get_draft_sd_log());
    for (int i = 0; i < SD_LOG_ITEM_INDEX; i++) {
        st = press(st, PURRGO_ACTION_DOWN);
    }
    st = press(st, PURRGO_ACTION_OK);
    CHECK(!menu_app_get_draft_sd_log());
    CHECK(app_config.sd_log && purrgo_logger_sd_enabled());   /* до BACK без изменений */
    st = press(st, PURRGO_ACTION_MENU);
    CHECK(!app_config.sd_log);
    CHECK(!purrgo_logger_sd_enabled());
}

static void test_boot(void)
{
    /* Запись включена в сохранённой конфигурации: purrgo_app_init() включает её при старте. */
    fake_fs_reset();
    purrgo_logger_sd_set_enabled(false);
    fake_fs_put(CFG, "SD_LOG=1\n");
    purrgo_app_init();
    CHECK(app_config.sd_log);
    CHECK(purrgo_logger_sd_enabled());

    /* Выключена - остаётся выключенной. */
    fake_fs_reset();
    fake_fs_put(CFG, "SD_LOG=0\n");
    purrgo_app_init();
    CHECK(!app_config.sd_log);
    CHECK(!purrgo_logger_sd_enabled());
}

int main(void)
{
    test_config_key();
    test_menu();
    test_boot();
    return test_finish("test_sd_log_setting");
}
