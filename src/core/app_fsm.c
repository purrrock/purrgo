#include "purrgo/app_fsm.h"
#include "purrgo/config.h"

// Глобальный экземпляр настроек
purrgo_config_t app_config;

static purrgo_state_t current_state;

void purrgo_config_init(void) {
    app_config.timezone_offset_h = 3; // По умолчанию UTC+3 (например, Москва/Минск)
    app_config.log_mode = LOGGER_MODE_STANDARD;
    app_config.backlight_on = true;
}

void purrgo_app_init(void) {
    purrgo_config_init();
    current_state = APP_STATE_SATELLITES; // При включении показываем поиск спутников
}

purrgo_state_t purrgo_app_get_state(void) {
    return current_state;
}

void purrgo_app_handle_button(purrgo_btn_t button) {
    // Глобальная обработка кнопок (смена экранов)
    // В дальнейшем логика усложнится: кнопки UP/DOWN в меню будут листать пункты,
    // а на карте - менять масштаб.
    
    switch (current_state) {
        case APP_STATE_MAP:
            if (button == BTN_BACK) current_state = APP_STATE_SATELLITES;
            if (button == BTN_OK)   current_state = APP_STATE_TRIP_COMPUTER;
            break;
            
        case APP_STATE_TRIP_COMPUTER:
            if (button == BTN_BACK) current_state = APP_STATE_MAP;
            if (button == BTN_OK)   current_state = APP_STATE_SETTINGS;
            break;
            
        case APP_STATE_SATELLITES:
            if (button == BTN_OK)   current_state = APP_STATE_MAP;
            break;
            
        case APP_STATE_SETTINGS:
            if (button == BTN_BACK) current_state = APP_STATE_TRIP_COMPUTER;
            // Пример: Нажатие UP/DOWN меняет часовой пояс
            if (button == BTN_UP && app_config.timezone_offset_h < 14) {
                app_config.timezone_offset_h++;
            }
            if (button == BTN_DOWN && app_config.timezone_offset_h > -12) {
                app_config.timezone_offset_h--;
            }
            break;
    }
}

// Вспомогательная функция для перевода UTC времени в локальное
static void apply_timezone(purrgo_gnss_solution_t* fix) {
    if (!fix->valid) return;

    int32_t local_hour = fix->hours + app_config.timezone_offset_h;
    
    // Базовая коррекция часов и перехода через полночь (для UI)
    // Для полноценного календаря требуется расчет с учетом дней в месяце и високосных лет.
    if (local_hour >= 24) {
        fix->hours = (uint8_t)(local_hour - 24);
        fix->day++; // Упрощенно: без учета конца месяца
    } else if (local_hour < 0) {
        fix->hours = (uint8_t)(local_hour + 24);
        fix->day--; // Упрощенно: без учета начала месяца
    } else {
        fix->hours = (uint8_t)local_hour;
    }
}

void purrgo_app_update(const purrgo_gnss_solution_t* current_fix) {
    // Создаем локальную копию фикса, чтобы не мутировать глобальные данные парсера
    purrgo_gnss_solution_t display_fix = *current_fix;
    
    apply_timezone(&display_fix);

    // Логика обновления активного экрана. Отрисовка здесь не производится, 
    // она будет вызвана отдельным модулем (например, display_render), 
    // который запросит текущий state через purrgo_app_get_state().
    
    switch (current_state) {
        case APP_STATE_SATELLITES:
            // Подготовка данных для экрана спутников (локальное время, координаты)
            break;
        case APP_STATE_MAP:
            // Расчет позиции на карте, обновление путевых точек
            break;
        case APP_STATE_TRIP_COMPUTER:
            // Обновление одометра и расчет скорости
            break;
        case APP_STATE_SETTINGS:
            // Логика экрана настроек
            break;
    }
}