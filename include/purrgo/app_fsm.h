#ifndef PURRGO_APP_FSM_H
#define PURRGO_APP_FSM_H

#include "purrgo/gnss_types.h"
#include <stdbool.h>
#include <stdint.h>

// Экраны (состояния) приложения
typedef enum {
    APP_STATE_SATELLITES,    // Спутники, координаты, качество фикса
    APP_STATE_MAP,           // Векторная карта и трек
    APP_STATE_COMPASS,       // Навигационный компас и курс
    APP_STATE_TRIP_COMPUTER, // Путевой компьютер: одометр, скорость
    APP_STATE_MENU_CONFIG    // Полноэкранные настройки (Часовой пояс)
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

// Обработчик нажатий кнопок
void purrgo_app_handle_button(purrgo_btn_t button);

// Основной цикл обновления логики
void purrgo_app_update(const purrgo_gnss_solution_t* current_fix);

// Получение текущего состояния для слоя отрисовки
purrgo_state_t purrgo_app_get_state(void);

// Получение черновика часового пояса для экрана настроек
int16_t purrgo_app_get_draft_tz_offset(void);

// Функция применения часового пояса с календарным пересчетом
void purrgo_app_apply_timezone(const purrgo_gnss_solution_t* utc, purrgo_gnss_solution_t* local, int16_t tz_offset_minutes);

#endif // PURRGO_APP_FSM_H