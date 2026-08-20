// file: src/core/app_fsm.c
#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include "purrgo/purrgo_time.h"
#include <stdio.h>

// Глобальный экземпляр конфигурации устройства
purrgo_config_t app_config;

// Текущее состояние конечного автомата
static purrgo_state_t current_state;

// Черновые (несохраненные) настройки времени для режима редактирования в меню
static int16_t draft_tz_offset_minutes;

void purrgo_config_init(void) {
    app_config.tz_offset_minutes = 180; // UTC+3:00 по умолчанию (Москва/Минск)
    app_config.log_mode = 0;
    app_config.backlight_on = true;     // Флаг активности подсветки
}

void purrgo_app_init(void) {
    purrgo_config_init();
    // Стартовый экран согласно новой концепции UI — Карта
    current_state = APP_STATE_MAP;
    draft_tz_offset_minutes = app_config.tz_offset_minutes;
}

purrgo_state_t purrgo_app_get_state(void) {
    return current_state;
}

int16_t purrgo_app_get_draft_tz_offset(void) {
    return draft_tz_offset_minutes;
}

void purrgo_app_handle_button(purrgo_btn_t button) {
    // 1. Обработка ввода в режиме редактирования настроек
    if (current_state == APP_STATE_MENU_CONFIG) {
        switch (button) {
            case PURRGO_BTN_PLUS:
            case PURRGO_BTN_RIGHT:
                // Увеличение часового пояса шагом в 15 минут (максимум UTC+14:00 = 840 минут)
                if (draft_tz_offset_minutes + 15 <= 840) {
                    draft_tz_offset_minutes += 15;
                }
                break;

            case PURRGO_BTN_MINUS:
            case PURRGO_BTN_LEFT:
                // Уменьшение часового пояса шагом в 15 минут (минимум UTC-12:00 = -720 минут)
                if (draft_tz_offset_minutes - 15 >= -720) {
                    draft_tz_offset_minutes -= 15;
                }
                break;

            case PURRGO_BTN_OK:
                // Сохранение часового пояса и возврат на главный экран (Карта)
                app_config.tz_offset_minutes = draft_tz_offset_minutes;
                current_state = APP_STATE_MAP;
                break;

            case PURRGO_BTN_MENU:
                // Отмена изменений и возврат на главный экран по зацикливанию
                current_state = APP_STATE_MAP;
                break;

            default:
                break;
        }
        return;
    }

    // 2. Обработка ввода в подменю выбора директории карт
    if (current_state == APP_STATE_MENU_DIR_SELECT) {
        if (button == PURRGO_BTN_MENU) {
            // Кнопка MENU выполняет роль "Назад" для возврата в меню настроек
            current_state = APP_STATE_MENU_CONFIG;
        }
        return;
    }

    // 3. Циклическое переключение основных экранов (Garmin eTrex Page Loop)
    if (button == PURRGO_BTN_MENU) {
        switch (current_state) {
            case APP_STATE_MAP:
                current_state = APP_STATE_TRIP_COMPUTER;
                break;

            case APP_STATE_TRIP_COMPUTER:
                current_state = APP_STATE_MENU_CONFIG;
                // При входе в меню сбрасываем черновик на текущее сохраненное значение
                draft_tz_offset_minutes = app_config.tz_offset_minutes;
                break;

            default:
                current_state = APP_STATE_MAP;
                break;
        }
    }
}

void purrgo_app_update(const purrgo_gnss_solution_t* current_fix) {
    purrgo_gnss_solution_t display_fix;

    // Пересчет UTC времени в локальное с использованием специализированного модуля purrgo_time
    purrgo_time_apply_timezone(current_fix, &display_fix, app_config.tz_offset_minutes);

    // Диспетчеризация фоновой логики приложения в зависимости от активного экрана
    switch (current_state) {
        case APP_STATE_MAP:
            // Фоновые расчеты для карты (панорамирование, проверке BBox, подгрузка кластеров)
            break;

        case APP_STATE_TRIP_COMPUTER:
            // Расчет агрегированных данных одометра и маршрута
            break;

        case APP_STATE_MENU_CONFIG:
            break;

        case APP_STATE_MENU_DIR_SELECT:
            break;
    }
}