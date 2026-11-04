#ifndef PURRGO_MENU_CONTROLLER_H
#define PURRGO_MENU_CONTROLLER_H

/*
 * Контроллер меню устройства: главное меню и его подэкраны (выбор каталога
 * карт, выбор маршрута, слои карты).
 *
 * Устройство экранов:
 *   - menu_controller.c - диспетчер: по состоянию находит экран в таблице
 *     и передаёт ему кнопки и событие входа;
 *   - menu_main.c       - главное меню (таблица пунктов);
 *   - menu_layers.c     - экран слоёв карты;
 *   - menu_dir_browser.c - экраны выбора каталога карт и файла маршрута.
 *
 * Как добавить пункт в главное меню - см. комментарий в menu_main.c.
 */

#include "purrgo/app_fsm.h"
#include "purrgo/fs_hal.h"
#include "purrgo/config.h"
#include "purrgo/battery.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/* ---------------------------------------------------------------------------
 * Управление (вызывается из app_fsm.c)
 * ------------------------------------------------------------------------- */

void purrgo_menu_controller_init(void);

/*
 * true, если состояние обслуживается меню (главное меню или его подэкран).
 */
bool purrgo_menu_controller_owns_state(purrgo_state_t state);

/*
 * Вызывается при входе на экран меню: создаёт новый черновик, сбрасывает
 * курсор, загружает список файлов и т. п.
 */
void purrgo_menu_controller_on_enter(purrgo_state_t state);

/*
 * Обработка кнопки. Возвращает true, если кнопка принята; новое состояние
 * записывается в *next_state_out (по умолчанию - текущее).
 */
bool purrgo_menu_controller_handle_button(
    purrgo_state_t current_state,
    purrgo_action_t button,
    purrgo_state_t* next_state_out
);


/* ---------------------------------------------------------------------------
 * Данные для отрисовки экранов (используются файлами ui и тестами)
 * ------------------------------------------------------------------------- */

/* Главное меню. */
int menu_app_get_main_count(void);
int menu_app_get_main_cursor(void);
/* Текст пункта index (UTF-8, значение берётся из черновика настроек). */
void menu_app_get_main_item_text(int index, char* buf, size_t size);

/* Черновики настроек, которые правятся в главном меню. */
int16_t menu_app_get_draft_tz_offset(void);
purrgo_power_source_t menu_app_get_draft_power_source(void);

/*
 * Выбор каталога карт / файла маршрута. В режиме выбора маршрута запись
 * списка с пустым именем означает "(none)" (снять маршрут).
 */
int menu_app_get_dir_list(purrgo_fs_dirent_t** list_out);
int menu_app_get_dir_cursor(void);

/* Слои карты. */
int menu_app_get_map_layers_cursor(void);
/* Маска видимости слоёв в черновике: purrgo_layer_on(mask, PURRGO_LAYER_*). */
uint16_t menu_app_get_draft_layers(void);

#endif /* PURRGO_MENU_CONTROLLER_H */
