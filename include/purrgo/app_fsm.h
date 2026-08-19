#ifndef PURRGO_APP_FSM_H
#define PURRGO_APP_FSM_H

#include "purrgo/gnss_types.h"
#include <stdbool.h>

// Экраны (состояния) приложения
typedef enum {
    APP_STATE_MAP,
    APP_STATE_TRIP_COMPUTER,
    APP_STATE_SATELLITES,
    APP_STATE_MENU_CONFIG // Полноэкранный режим настройки
} purrgo_state_t;

// Аппаратные кнопки устройства
typedef enum {
    PURRGO_BTN_UP,
    PURRGO_BTN_DOWN,
    PURRGO_BTN_LEFT,
    PURRGO_BTN_RIGHT,
    PURRGO_BTN_PLUS,
    PURRGO_BTN_MINUS,
    PURRGO_BTN_MENU,
    PURRGO_BTN_OK
} purrgo_btn_t;

// Инициализация конечного автомата
void purrgo_app_init(void);

// Получить текущее черновое значение смещения часового пояса (для отображения в меню до нажатия OK)
int16_t purrgo_app_get_draft_tz_offset(void);

// Обработчик нажатий кнопок (вызывается из прерываний или поллинга)
void purrgo_app_handle_button(purrgo_btn_t button);

// Основной цикл обновления логики (вызывается периодически)
void purrgo_app_update(const purrgo_gnss_solution_t* current_fix);

// Получение текущего состояния для слоя отрисовки (Renderer)
purrgo_state_t purrgo_app_get_state(void);

#endif // PURRGO_APP_FSM_H