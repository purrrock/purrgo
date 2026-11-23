#ifndef PURRGO_APP_KEYMAP_H
#define PURRGO_APP_KEYMAP_H

#include "purrgo/app_fsm.h"

/*
 * Таблица соответствия "экран -> кнопка -> действие". Четыре физические кнопки
 * (короткое и длинное нажатие) на каждом экране означают своё: на карте это
 * панорамирование и масштаб, в меню - вверх/вниз/выбор. Здесь вся эта раскладка
 * собрана в одну таблицу данных (app_keymap.c); новый экран добавляется записью в
 * таблицу, а не веткой в общем switch app_fsm.c.
 *
 * Кнопки читалки и комбинация выхода из трекера сюда не входят: их разбирают
 * читалка и сам app_fsm (у них нет "действий", только прямая обработка).
 */
typedef enum {
    APP_KEYMAP_MAP,    /* карта (APP_STATE_MAP) */
    APP_KEYMAP_TRIP,   /* путевой компьютер (APP_STATE_TRIP_COMPUTER) */
    APP_KEYMAP_MENU,   /* любой экран, которым владеет menu_controller */
    APP_KEYMAP_MENU_OK_LONG, /* то же, но долгое K3 - PURRGO_ACTION_OK_LONG (результаты поиска объектов) */
    APP_KEYMAP_SCREEN_COUNT
} app_keymap_screen_t;

/*
 * Действие, в которое кнопка превращается на экране screen. PURRGO_ACTION_NONE -
 * кнопка на этом экране ничего не значит (и экран не перерисовывается).
 */
purrgo_action_t purrgo_app_keymap_action(app_keymap_screen_t screen, purrgo_btn_t button);

#endif /* PURRGO_APP_KEYMAP_H */
