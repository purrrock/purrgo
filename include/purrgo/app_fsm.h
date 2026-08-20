//file: include/purrgo/app_fsm.h
#ifndef PURRGO_APP_FSM_H
#define PURRGO_APP_FSM_H

#include "purrgo/gnss_types.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * Конечный автомат (FSM) приложения. 
 * Реализует классический циклический интерфейс (Garmin eTrex).
 * Переход между основными экранами (MAP -> TRIP_COMPUTER -> MENU_CONFIG)
 * осуществляется по нажатию кнопки PURRGO_BTN_MENU.
 */
typedef enum {
    APP_STATE_MAP,            // Стартовая страница: Карта (viewport) + Status Bar (сверху) + Панель данных (снизу)
    APP_STATE_TRIP_COMPUTER,  // Маршрутный компьютер: Сетка полей (Data fields) с навигационной статистикой
    APP_STATE_MENU_CONFIG,    // Главное меню настроек: Список параметров (Часовой пояс, выбор директории)
    APP_STATE_MENU_DIR_SELECT // Подсостояние: Файловый браузер для выбора каталога с векторными картами
} purrgo_state_t;

// Аппаратные кнопки устройства
typedef enum {
    PURRGO_BTN_UP,            // Навигация по меню / Панорамирование карты (Y-)
    PURRGO_BTN_DOWN,          // Навигация по меню / Панорамирование карты (Y+)
    PURRGO_BTN_LEFT,          // Изменение значений / Панорамирование карты (X-)
    PURRGO_BTN_RIGHT,         // Изменение значений / Панорамирование карты (X+)
    PURRGO_BTN_PLUS,          // Zoom In карты
    PURRGO_BTN_MINUS,         // Zoom Out карты
    PURRGO_BTN_MENU,          // Циклическая смена основных страниц (Page/Menu) / Отмена (Назад) в подменю
    PURRGO_BTN_OK             // Подтверждение выбора (Enter)
} purrgo_btn_t;

// Инициализация конечного автомата
void purrgo_app_init(void);

// Обработчик нажатий кнопок (вызывается из ISR или потока опроса GPIO)
void purrgo_app_handle_button(purrgo_btn_t button);

// Основной цикл обновления логики
void purrgo_app_update(const purrgo_gnss_solution_t* current_fix);

// Получение текущего состояния для слоя диспетчеризации отрисовки (gfx_renderer)
purrgo_state_t purrgo_app_get_state(void);

// Получение черновика часового пояса для режима редактирования в APP_STATE_MENU_CONFIG
int16_t purrgo_app_get_draft_tz_offset(void);

// Функция применения часового пояса с календарным пересчетом (влияет на локальное время и расчет восхода/заката)
void purrgo_app_apply_timezone(const purrgo_gnss_solution_t* utc, purrgo_gnss_solution_t* local, int16_t tz_offset_minutes);

#endif // PURRGO_APP_FSM_H