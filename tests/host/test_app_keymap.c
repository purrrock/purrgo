/*
 * Таблица "экран -> кнопка -> действие" (src/core/ui/app_keymap.c).
 * Ожидаемая раскладка записана здесь явной матрицей, независимо от реализации
 * (она повторяет прежний switch в app_fsm.c): любая правка раскладки должна
 * осознанно менять и этот тест.
 *
 *   карта:            K1..K4 коротко - влево/вправо/вверх/вниз (панорама);
 *                     K1 длинно - минус, K2 длинно - плюс (масштаб), K3 длинно - OK, K4 длинно - меню;
 *   путевой компьютер: только K4 (коротко и длинно) - меню;
 *   меню:             K1 - вверх, K2 - вниз, K3 - OK, K4 - меню; короткое и длинное равнозначны.
 * Комбинация K1+K4 действием не бывает ни на одном экране.
 */
#include "ui/app_keymap.h"
#include "test_common.h"

#define N PURRGO_ACTION_NONE
#define BTN_COUNT ((int)PURRGO_BTN_COMBO_K1_K4 + 1)

/* Порядок столбцов - purrgo_btn_t: K1s K2s K3s K4s K1l K2l K3l K4l COMBO. */
static const purrgo_action_t k_expected[APP_KEYMAP_SCREEN_COUNT][9] = {
    [APP_KEYMAP_MAP] = {
        PURRGO_ACTION_LEFT, PURRGO_ACTION_RIGHT, PURRGO_ACTION_UP, PURRGO_ACTION_DOWN,
        PURRGO_ACTION_MINUS, PURRGO_ACTION_PLUS, PURRGO_ACTION_OK, PURRGO_ACTION_MENU,
        N },
    [APP_KEYMAP_TRIP] = {
        N, N, N, PURRGO_ACTION_MENU,
        N, N, N, PURRGO_ACTION_MENU,
        N },
    [APP_KEYMAP_MENU] = {
        PURRGO_ACTION_UP, PURRGO_ACTION_DOWN, PURRGO_ACTION_OK, PURRGO_ACTION_MENU,
        PURRGO_ACTION_UP, PURRGO_ACTION_DOWN, PURRGO_ACTION_OK, PURRGO_ACTION_MENU,
        N },
};

static void test_matrix_matches_spec(void)
{
    /* Матрица рассчитана на 9 кнопок: добавили кнопку - расширьте и её. */
    CHECK(BTN_COUNT == 9);

    for (int screen = 0; screen < (int)APP_KEYMAP_SCREEN_COUNT; screen++) {
        for (int btn = 0; btn < BTN_COUNT; btn++) {
            const purrgo_action_t got =
                purrgo_app_keymap_action((app_keymap_screen_t)screen, (purrgo_btn_t)btn);
            if (got != k_expected[screen][btn]) {
                printf("screen %d button %d: got %d, expected %d\n",
                       screen, btn, (int)got, (int)k_expected[screen][btn]);
            }
            CHECK(got == k_expected[screen][btn]);
        }
    }
}

static void test_combo_is_never_an_action(void)
{
    for (int screen = 0; screen < (int)APP_KEYMAP_SCREEN_COUNT; screen++) {
        CHECK(purrgo_app_keymap_action((app_keymap_screen_t)screen, PURRGO_BTN_COMBO_K1_K4) == N);
    }
}

static void test_out_of_range_is_safe(void)
{
    /* Неизвестный экран и неизвестная кнопка - NONE, а не нулевое значение (UP). */
    CHECK(purrgo_app_keymap_action(APP_KEYMAP_SCREEN_COUNT, PURRGO_BTN_KEY1_SHORT) == N);
    CHECK(purrgo_app_keymap_action((app_keymap_screen_t)-1, PURRGO_BTN_KEY1_SHORT) == N);
    CHECK(purrgo_app_keymap_action(APP_KEYMAP_MAP, (purrgo_btn_t)99) == N);
    CHECK(purrgo_app_keymap_action(APP_KEYMAP_MENU, (purrgo_btn_t)-1) == N);
}

static void test_menu_short_and_long_equal(void)
{
    for (int k = 0; k < 4; k++) {
        CHECK(purrgo_app_keymap_action(APP_KEYMAP_MENU, (purrgo_btn_t)(PURRGO_BTN_KEY1_SHORT + k)) ==
              purrgo_app_keymap_action(APP_KEYMAP_MENU, (purrgo_btn_t)(PURRGO_BTN_KEY1_LONG + k)));
    }
}

int main(void)
{
    test_matrix_matches_spec();
    test_combo_is_never_an_action();
    test_out_of_range_is_safe();
    test_menu_short_and_long_equal();
    return test_finish("test_app_keymap");
}
