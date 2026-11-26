/*
 * Пункты главного меню SEARCH и SMART SEARCH (src/core/ui/menu_main.c, search_screen.c):
 * SEARCH остаётся прежним поиском по категориям и от search.idx не зависит; SMART SEARCH открывает
 * экран умного поиска только при исправном search.idx каталога карты, иначе остаётся в меню и не
 * закрывает файлы карты; выход из экрана - в главное меню, поиск закрыт, слоты ФС свободны.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include "purrgo/map_search.h"
#include "purrgo/menu_controller.h"
#include "purrgo/search_screen.h"
#include "fake_fs.h"
#include "fake_power.h"
#include "test_common.h"

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

#define MAP "maps/s"

static purrgo_state_t press(purrgo_state_t st, purrgo_action_t a)
{
    purrgo_state_t next = st;
    (void)purrgo_menu_controller_handle_button(st, a, &next);
    return next;
}

static void put_index(void)
{
    static const char *const names[] = { "search.idx", "pois.db", "roads.db", "waterways.db", "water.db", "landuse.db" };

    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        char host[128], dev[64];
        FILE *f;
        long n;
        uint8_t *d;

        snprintf(host, sizeof host, TEST_DATA_DIR "/search-map/%s", names[i]);
        snprintf(dev, sizeof dev, MAP "/%s", names[i]);
        f = fopen(host, "rb");
        CHECK(f != NULL);
        if (f == NULL) { return; }
        fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
        d = malloc((size_t)n);
        CHECK(fread(d, 1, (size_t)n, f) == (size_t)n);
        fclose(f);
        fake_fs_put_bin(dev, d, (uint32_t)n);
        free(d);
    }
}

/* Курсор главного меню на пункт с заданным названием. */
static void cursor_to(const char *title)
{
    char text[64];

    purrgo_menu_controller_on_enter(APP_STATE_MENU_MAIN);
    for (int i = 0; i < menu_app_get_main_count(); i++) {
        menu_app_get_main_item_text(i, text, sizeof text);
        if (strcmp(text, title) == 0) { return; }
        (void)press(APP_STATE_MENU_MAIN, PURRGO_ACTION_DOWN);
    }
    CHECK(0 && "menu item not found");
}

static void test_menu_items(void)
{
    char text[64];
    int search = -1, smart = -1;

    fake_fs_reset();
    purrgo_config_init();
    purrgo_menu_controller_on_enter(APP_STATE_MENU_MAIN);
    for (int i = 0; i < menu_app_get_main_count(); i++) {
        menu_app_get_main_item_text(i, text, sizeof text);
        if (strcmp(text, "SEARCH") == 0) { search = i; }
        if (strcmp(text, "SMART SEARCH") == 0) { smart = i; }
    }
    CHECK(search >= 0 && smart == search + 1);
}

static void test_search_is_categories(void)
{
    purrgo_state_t st;

    /* без search.idx и с ним SEARCH ведёт в список категорий */
    for (int with_idx = 0; with_idx < 2; with_idx++) {
        fake_fs_reset();
        purrgo_config_init();
        strcpy(app_config.map_dir, MAP);
        if (with_idx) { put_index(); }
        cursor_to("SEARCH");
        st = press(APP_STATE_MENU_MAIN, PURRGO_ACTION_OK);
        CHECK(st == APP_STATE_MENU_POI_CATEGORY);
        CHECK(!purrgo_map_search_is_open());
    }
}

static void test_smart_search(void)
{
    purrgo_state_t st, next;

    /* нет search.idx: остаёмся в меню */
    fake_fs_reset();
    purrgo_config_init();
    strcpy(app_config.map_dir, MAP);
    cursor_to("SMART SEARCH");
    st = press(APP_STATE_MENU_MAIN, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_MAIN);
    CHECK(!purrgo_map_search_is_open() && fake_fs_open_handles() == 0);

    /* повреждённый search.idx (не тот формат): тоже остаёмся в меню, слоты свободны */
    fake_fs_put_bin(MAP "/search.idx", (const uint8_t *)"junk", 4);
    st = press(APP_STATE_MENU_MAIN, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_MENU_MAIN && fake_fs_open_handles() == 0);

    /* исправный: экран поиска */
    fake_fs_reset();
    purrgo_config_init();
    strcpy(app_config.map_dir, MAP);
    put_index();
    cursor_to("SMART SEARCH");
    st = press(APP_STATE_MENU_MAIN, PURRGO_ACTION_OK);
    CHECK(st == APP_STATE_SEARCH);
    CHECK(purrgo_map_search_is_open());
    CHECK(!purrgo_search_screen_list_focus());
    CHECK(!purrgo_search_screen_busy());

    /* список пуст: долгое K3 фокус в список не переводит */
    next = APP_STATE_SEARCH;
    CHECK(purrgo_search_screen_handle_button(PURRGO_BTN_KEY3_LONG, &next));
    CHECK(next == APP_STATE_SEARCH && !purrgo_search_screen_list_focus());

    /* такт без изменений запроса ничего не перерисовывает */
    g_now += 5000u;
    CHECK(!purrgo_search_screen_tick(g_now));

    /* долгое K4: в главное меню, поиск закрыт, слоты свободны */
    CHECK(purrgo_search_screen_handle_button(PURRGO_BTN_KEY4_LONG, &next));
    CHECK(next == APP_STATE_MENU_MAIN);
    CHECK(!purrgo_map_search_is_open() && fake_fs_open_handles() == 0);
}

int main(void)
{
    test_menu_items();
    test_search_is_categories();
    test_smart_search();
    return test_finish("test_smart_search_menu");
}
