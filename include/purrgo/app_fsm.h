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
    APP_STATE_MENU_DIR_SELECT, // Подсостояние: Файловый браузер для выбора каталога с векторными картами
    APP_STATE_MENU_MAP_LAYERS // Подсостояние: Экран настройки видимости слоев карты
} purrgo_state_t;

// Аппаратные кнопки устройства
typedef enum {
    PURRGO_ACTION_UP,
    PURRGO_ACTION_DOWN,
    PURRGO_ACTION_LEFT,
    PURRGO_ACTION_RIGHT,
    PURRGO_ACTION_PLUS,
    PURRGO_ACTION_MINUS,
    PURRGO_ACTION_MENU,
    PURRGO_ACTION_OK,
    PURRGO_ACTION_NONE
} purrgo_action_t;

typedef enum {
    PURRGO_BTN_KEY1_SHORT,
    PURRGO_BTN_KEY2_SHORT,
    PURRGO_BTN_KEY3_SHORT,
    PURRGO_BTN_KEY4_SHORT,
    PURRGO_BTN_KEY1_LONG,
    PURRGO_BTN_KEY2_LONG,
    PURRGO_BTN_KEY3_LONG,
    PURRGO_BTN_KEY4_LONG
} purrgo_btn_t;

#define PURRGO_BTN_LONG_PRESS_MS 500

// Инициализация конечного автомата
void purrgo_app_init(void);

// Обработчик нажатий кнопок (вызывается из ISR или потока опроса GPIO)
void purrgo_app_handle_button(purrgo_btn_t button);

#include "purrgo/sun.h"

// Основной цикл обновления логики
// (Оставлен для обратной совместимости, если используется напрямую)
void purrgo_app_update(const purrgo_gnss_solution_t* current_fix);

// Передача байта из потока GNSS в парсер конечного автомата
void purrgo_app_feed_gnss_byte(uint8_t byte);

// Главный тик приложения (обновление состояния и солнце)
void purrgo_app_tick(uint32_t current_time_ms);

// Геттеры для состояния GNSS и Солнца
const purrgo_gnss_solution_t* purrgo_app_get_gnss_solution(void);
const purrgo_sun_info_t* purrgo_app_get_sun_info(void);

// Уведомление конечного автомата о том, что маркер был успешно отрисован
void purrgo_app_notify_marker_rendered(const purrgo_gnss_solution_t* rendered_fix);

// Получение текущего состояния для слоя диспетчеризации отрисовки (gfx_renderer)
purrgo_state_t purrgo_app_get_state(void);

typedef enum {
    PURRGO_MAP_SCALE_10M,
    PURRGO_MAP_SCALE_20M,
    PURRGO_MAP_SCALE_50M,
    PURRGO_MAP_SCALE_100M,
    PURRGO_MAP_SCALE_200M,
    PURRGO_MAP_SCALE_500M,
    PURRGO_MAP_SCALE_1KM,
    PURRGO_MAP_SCALE_2KM,
    PURRGO_MAP_SCALE_5KM,
    PURRGO_MAP_SCALE_10KM,
    PURRGO_MAP_SCALE_20KM,
    PURRGO_MAP_SCALE_50KM,
    PURRGO_MAP_SCALE_100KM,
    PURRGO_MAP_SCALE_200KM,
    PURRGO_MAP_SCALE_500KM,
    PURRGO_MAP_SCALE_1000KM,
    PURRGO_MAP_SCALE_2000KM,
    PURRGO_MAP_SCALE_5000KM,
    PURRGO_MAP_SCALE_10000KM,
    PURRGO_MAP_SCALE_COUNT
} purrgo_map_scale_t;

/*
 * Перерисовка UI.
 *
 * Единственный владелец флагов - ядро; платформенный цикл только спрашивает
 * purrgo_app_ui_needs_render() и вызывает purrgo_app_ui_render(), ничего
 * не сбрасывая сам:
 *   - ui_dirty (общий флаг) забирает purrgo_app_ui_render() в начале прохода;
 *   - map_dirty (полная перерисовка карты) сбрасывает ui_render_map() только
 *     после успешной отрисовки; при неудаче проход повторяется через
 *     purrgo_app_ui_defer_render() ограниченное число раз.
 */
void purrgo_app_ui_mark_dirty(void);
bool purrgo_app_ui_needs_render(uint32_t now_ms);

// Вызывается из purrgo_app_ui_render(): забрать общий флаг перед отрисовкой.
void purrgo_app_ui_consume_dirty(void);

// Не начинать новый проход отрисовки раньше, чем через delay_ms (повтор после неудачи).
void purrgo_app_ui_defer_render(uint32_t delay_ms);


// Функция применения часового пояса с календарным пересчетом (влияет на локальное время и расчет восхода/заката)
void purrgo_app_apply_timezone(const purrgo_gnss_solution_t* utc, purrgo_gnss_solution_t* local, int16_t tz_offset_minutes);

#endif // PURRGO_APP_FSM_H