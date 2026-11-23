#ifndef PURRGO_MENU_INTERNAL_H
#define PURRGO_MENU_INTERNAL_H

/*
 * Внутренний интерфейс между диспетчером меню (menu_controller.c) и экранами
 * (menu_main.c, menu_config.c, menu_layers.c, menu_dir_browser.c, menu_waypoint.c, menu_poi.c). Снаружи ядра не нужен.
 */

#include "purrgo/menu_controller.h"
#include "purrgo/route.h"

/*
 * Описание экрана меню. Один экран - одно состояние FSM.
 * Чтобы добавить экран: завести состояние в purrgo_state_t, описать
 * menu_screen_t в новом файле и добавить его в таблицу в menu_controller.c.
 */
typedef struct {
    purrgo_state_t state;

    /* Вход на экран (новый черновик, сброс курсора). Может быть NULL. */
    void (*on_enter)(void);

    /*
     * Нажатие кнопки. Возвращает true, если кнопка принята. Новое состояние
     * записывается в *next (по умолчанию уже равно текущему).
     */
    bool (*on_button)(purrgo_action_t button, purrgo_state_t* next);
} menu_screen_t;

/*
 * Пункт списочного экрана меню (главное меню и CONFIG описываются таблицами таких пунктов).
 */
typedef struct {
    /* Заголовок. Показывается, если format == NULL. */
    const char* title;

    /* Формирует текст строки со значением (UTF-8). Может быть NULL. */
    void (*format)(char* buf, size_t size);

    /*
     * Нажатие SEL. Для перехода на другой экран вызвать menu_goto(next, ...).
     * Может быть NULL (пункт только показывает значение).
     */
    void (*on_select)(purrgo_state_t* next);
} menu_item_t;

extern const menu_screen_t menu_screen_main;
extern const menu_screen_t menu_screen_config;
extern const menu_screen_t menu_screen_map_layers;
extern const menu_screen_t menu_screen_dir_select;
extern const menu_screen_t menu_screen_route_select;
extern const menu_screen_t menu_screen_wpt_select;
extern const menu_screen_t menu_screen_poi_category;
extern const menu_screen_t menu_screen_poi_results;

/*
 * Переход на другой экран меню "вперёд": записывает состояние в *next и
 * выполняет вход на экран (on_enter). Для возврата "назад" достаточно просто
 * записать *next: черновик главного меню при этом сохраняется.
 */
void menu_goto(purrgo_state_t* next, purrgo_state_t state);

/*
 * Что покажет экран APP_STATE_MENU_ROUTE_SELECT при следующем входе: файлы
 * каталога ROUTES или записанные треки (TRACKS). Вызывается перед menu_goto().
 */
void menu_route_select_set_source(purrgo_route_src_t src);

/*
 * Сдвиг курсора списка на delta (-1 или +1). wrap = true - перенос через край
 * списка, иначе курсор упирается в границу.
 */
static inline int menu_cursor_move(int cursor, int delta, int count, bool wrap)
{
    int c = cursor + delta;

    if (c < 0) {
        c = wrap ? count - 1 : 0;
    }
    else if (c >= count) {
        c = wrap ? 0 : count - 1;
    }
    return c;
}

#endif /* PURRGO_MENU_INTERNAL_H */
