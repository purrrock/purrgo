#ifndef PURRGO_APP_FSM_H
#define PURRGO_APP_FSM_H

#include "purrgo/gnss_types.h"
#include <stdbool.h>

// Экраны (состояния) приложения
typedef enum {
    APP_STATE_MAP,            // Векторная карта и трек
    APP_STATE_TRIP_COMPUTER,  // Скорость, дистанция, ETA
    APP_STATE_SATELLITES,     // Уровни сигнала, координаты, фикс
    APP_STATE_SETTINGS        // Настройки (Часовой пояс, режим логгера)
} purrgo_state_t;

// Аппаратные кнопки устройства
typedef enum {
    BTN_UP,
    BTN_DOWN,
    BTN_OK,
    BTN_BACK
} purrgo_btn_t;

// Инициализация конечного автомата
void purrgo_app_init(void);

// Обработчик нажатий кнопок (вызывается из прерываний или поллинга)
void purrgo_app_handle_button(purrgo_btn_t button);

// Основной цикл обновления логики (вызывается периодически)
void purrgo_app_update(const purrgo_gnss_solution_t* current_fix);

// Получение текущего состояния для слоя отрисовки (Renderer)
purrgo_state_t purrgo_app_get_state(void);

#endif // PURRGO_APP_FSM_H