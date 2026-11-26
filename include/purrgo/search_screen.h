#ifndef PURRGO_SEARCH_SCREEN_H
#define PURRGO_SEARCH_SCREEN_H

/*
 * Экран умного поиска (APP_STATE_SEARCH, пункт SEARCH главного меню), docs/design/poi-search.md:
 * сверху поле ввода, под ним список ближайших объектов, внизу экранная клавиатура. Запрос
 * (категория и/или начало названия: "кафе", "пушкин", "кафе пуш", "улица ленина") разбирает
 * purrgo/map_search.h; список обновляется по мере набора, после небольшой паузы в наборе.
 *
 * Кнопки. Фокус на клавиатуре (как в purrgo/keyboard.h):
 *   K1 / K2 / K4 - влево / вправо / вниз по клавишам, K3 - нажать клавишу;
 *   долгое K1 - стереть, долгое K2 - вверх, долгое K3 - перейти в список, долгое K4 - выход.
 * Фокус на списке:
 *   K1 / K2 (в том числе долгие) - строка вверх / вниз;
 *   K3 - показать объект на карте (автоследование выключено, в центре перекрестие);
 *   долгое K3 - сделать объект целью навигации (WAYPOINT) и показать на карте;
 *   K4 - назад к клавиатуре, долгое K4 - выход в главное меню.
 */

#include <stdbool.h>
#include <stdint.h>

#include "purrgo/app_fsm.h"
#include "purrgo/keyboard.h"

/* Строк списка на экране. */
#define PURRGO_SEARCH_VISIBLE_ROWS 8

/* Пауза после последней нажатой клавиши, по истечении которой запрос уходит в поиск. */
#define PURRGO_SEARCH_DEBOUNCE_MS 600u

/*
 * Вход на экран: освобождает файлы карты (их слоты нужны поиску; карта откроет их заново при
 * следующей отрисовке), открывает search.idx каталога карты и выбирает начало отсчёта (фикс GNSS,
 * иначе последняя позиция, иначе центр карты). false - search.idx нет или он не прошёл проверку:
 * экран не открывается (без search.idx файлы карты остаются открытыми).
 */
bool purrgo_search_screen_enter(void);

/* Кнопка. true - экран нужно перерисовать. next - новое состояние (по умолчанию APP_STATE_SEARCH). */
bool purrgo_search_screen_handle_button(purrgo_btn_t button, purrgo_state_t *next);

/* Такт приложения: пауза набора, порция расчёта. true - список или строка состояния изменились. */
bool purrgo_search_screen_tick(uint32_t now_ms);

/* Идёт расчёт или ждёт запрос после паузы: глубокий сон запрещён. */
bool purrgo_search_screen_busy(void);

/* Данные для отрисовки (ui_search.c). */
const purrgo_keyboard_t *purrgo_search_screen_keyboard(void);
bool purrgo_search_screen_list_focus(void);
int  purrgo_search_screen_cursor(void);
int  purrgo_search_screen_top(void);

#endif /* PURRGO_SEARCH_SCREEN_H */
