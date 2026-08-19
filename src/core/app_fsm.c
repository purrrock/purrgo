#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include <stdio.h>

purrgo_config_t app_config;

static purrgo_state_t current_state;
static purrgo_state_t previous_state; // Для возврата из меню при отмене

// Черновые (несохраненные) настройки
static int16_t draft_tz_offset_minutes;

void purrgo_config_init(void) {
    app_config.tz_offset_minutes = 180; // UTC+3:00 по умолчанию (Москва/Минск)
    app_config.log_mode = 0;
    app_config.backlight_on = true;
}

void purrgo_app_init(void) {
    purrgo_config_init();
    current_state = APP_STATE_SATELLITES;
    previous_state = APP_STATE_SATELLITES;
    draft_tz_offset_minutes = app_config.tz_offset_minutes;
}

purrgo_state_t purrgo_app_get_state(void) {
    return current_state;
}

int16_t purrgo_app_get_draft_tz_offset(void) {
    return draft_tz_offset_minutes;
}

void purrgo_app_handle_button(purrgo_btn_t button) {
    // Нажатие кнопки MENU в любом основном режиме переключает в меню CONFIG
    if (button == PURRGO_BTN_MENU) {
        if (current_state != APP_STATE_MENU_CONFIG) {
            // Вход в меню: сохраняем предыдущий экран и копируем активные настройки во временный черновик
            previous_state = current_state;
            current_state = APP_STATE_MENU_CONFIG;
            draft_tz_offset_minutes = app_config.tz_offset_minutes;
        } else {
            // Выход из меню без сохранения: отбрасываем draft и возвращаем прежний экран
            current_state = previous_state;
        }
        return;
    }

    // Обработка ввода внутри полноэкранного меню CONFIG
    if (current_state == APP_STATE_MENU_CONFIG) {
        switch (button) {
            case PURRGO_BTN_PLUS:
                // Увеличение часового пояса шагом в 15 минут (максимум UTC+14:00 = 840 минут)
                if (draft_tz_offset_minutes + 15 <= 840) {
                    draft_tz_offset_minutes += 15;
                }
                break;

            case PURRGO_BTN_MINUS:
                // Уменьшение часового пояса шагом в 15 минут (минимум UTC-12:00 = -720 минут)
                if (draft_tz_offset_minutes - 15 >= -720) {
                    draft_tz_offset_minutes -= 15;
                }
                break;

            case PURRGO_BTN_OK:
                // Применение и сохранение настроек
                app_config.tz_offset_minutes = draft_tz_offset_minutes;
                current_state = previous_state;
                break;

            default:
                break;
        }
        return;
    }

    // Переключение основных навигационных экранов кнопкой OK
    if (button == PURRGO_BTN_OK) {
        switch (current_state) {
            case APP_STATE_SATELLITES:
                current_state = APP_STATE_MAP;
                break;
            case APP_STATE_MAP:
                current_state = APP_STATE_COMPASS;
                break;
            case APP_STATE_COMPASS:
                current_state = APP_STATE_TRIP_COMPUTER;
                break;
            case APP_STATE_TRIP_COMPUTER:
                current_state = APP_STATE_SATELLITES;
                break;
            default:
                break;
        }
    }
}

static bool is_leap_year(uint8_t year) {
    // В рамках проекта используется двухзначный год 2000-2099.
    // Год является високосным, если его двухзначное значение делится на 4 без остатка.
    return (year % 4 == 0);
}

static uint8_t days_in_month(uint8_t month, uint8_t year) {
    if (month == 2) {
        return is_leap_year(year) ? 29 : 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;
    }
    return 31;
}

// Перевод UTC времени в локальное с учетом минутного смещения и полной календарной математикой
void purrgo_app_apply_timezone(const purrgo_gnss_solution_t* utc, purrgo_gnss_solution_t* local, int16_t tz_offset_minutes) {
    *local = *utc;

    if (!local->valid) return;

    // Переводим часы и минуты фикса в общее количество минут от начала суток
    int32_t total_mins = (int32_t)local->hours * 60 + (int32_t)local->minutes + tz_offset_minutes;

    // Коррекция перехода через полночь назад
    while (total_mins < 0) {
        total_mins += 1440;

        if (local->day > 1) {
            local->day--;
        } else {
            if (local->month > 1) {
                local->month--;
            } else {
                local->month = 12;
                if (local->year > 0) {
                    local->year--;
                } else {
                    local->year = 99; // Заворот с 2000 на 2099 год
                }
            }
            local->day = days_in_month(local->month, local->year);
        }
    }

    // Коррекция перехода через полночь вперед
    while (total_mins >= 1440) {
        total_mins -= 1440;

        uint8_t dim = days_in_month(local->month, local->year);
        if (local->day < dim) {
            local->day++;
        } else {
            local->day = 1;
            if (local->month < 12) {
                local->month++;
            } else {
                local->month = 1;
                if (local->year < 99) {
                    local->year++;
                } else {
                    local->year = 0; // Заворот с 2099 на 2000 год
                }
            }
        }
    }

    local->hours = (uint8_t)(total_mins / 60);
    local->minutes = (uint8_t)(total_mins % 60);
}

void purrgo_app_update(const purrgo_gnss_solution_t* current_fix) {
    purrgo_gnss_solution_t display_fix;
    purrgo_app_apply_timezone(current_fix, &display_fix, app_config.tz_offset_minutes);

    switch (current_state) {
        case APP_STATE_SATELLITES:
            break;
        case APP_STATE_MAP:
            break;
        case APP_STATE_COMPASS:
            break;
        case APP_STATE_TRIP_COMPUTER:
            break;
        case APP_STATE_MENU_CONFIG:
            break;
    }
}