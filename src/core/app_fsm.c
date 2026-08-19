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

    // Логика переключения основных экранов
    switch (current_state) {
        case APP_STATE_SATELLITES:
            if (button == PURRGO_BTN_OK) current_state = APP_STATE_MAP;
            break;
        case APP_STATE_MAP:
            if (button == PURRGO_BTN_OK) current_state = APP_STATE_TRIP_COMPUTER;
            break;
        case APP_STATE_TRIP_COMPUTER:
            if (button == PURRGO_BTN_OK) current_state = APP_STATE_SATELLITES;
            break;
        default:
            break;
    }
}

static bool is_leap_year(uint8_t year) {
    // NMEA year is typically 2 digits. Assume 2000s base if year < 80.
    // However, the rule states to preserve the year field as-is and deduce full year.
    // Usually, 2-digit years are since 2000 in this context, or we can just check divisibility by 4
    // since 2000 was a leap year and years 2004..2096 are leap years if divisible by 4.
    // If year + 2000 is used:
    uint16_t full_year = 2000 + year;
    return (full_year % 4 == 0 && (full_year % 100 != 0 || full_year % 400 == 0));
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

// Перевод UTC времени в локальное с учетом минутного смещения
void purrgo_app_apply_timezone(purrgo_gnss_solution_t* fix, int16_t tz_offset_minutes) {
    if (!fix->valid) return;

    // Переводим часы и минуты фикса в общее количество минут от начала суток
    int32_t total_mins = (int32_t)fix->hours * 60 + (int32_t)fix->minutes + tz_offset_minutes;

    // Коррекция перехода через полночь
    while (total_mins < 0) {
        total_mins += 1440;

        if (fix->day > 1) {
            fix->day--;
        } else {
            if (fix->month > 1) {
                fix->month--;
            } else {
                fix->month = 12;
                if (fix->year > 0) {
                    fix->year--;
                } else {
                    fix->year = 99; // Assume wrap around to 1999 or just stick to 99
                }
            }
            fix->day = days_in_month(fix->month, fix->year);
        }
    }

    while (total_mins >= 1440) {
        total_mins -= 1440;

        uint8_t dim = days_in_month(fix->month, fix->year);
        if (fix->day < dim) {
            fix->day++;
        } else {
            fix->day = 1;
            if (fix->month < 12) {
                fix->month++;
            } else {
                fix->month = 1;
                if (fix->year < 99) {
                    fix->year++;
                } else {
                    fix->year = 0; // Wrap to 00
                }
            }
        }
    }

    fix->hours = (uint8_t)(total_mins / 60);
    fix->minutes = (uint8_t)(total_mins % 60);
}

void purrgo_app_update(const purrgo_gnss_solution_t* current_fix) {
    purrgo_gnss_solution_t display_fix = *current_fix;
    purrgo_app_apply_timezone(&display_fix, app_config.tz_offset_minutes);

    switch (current_state) {
        case APP_STATE_SATELLITES:
            break;
        case APP_STATE_MAP:
            break;
        case APP_STATE_TRIP_COMPUTER:
            break;
        case APP_STATE_MENU_CONFIG:
            break;
    }
}