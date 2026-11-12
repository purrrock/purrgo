#include "purrgo/ui/ui_dirty.h"
#include "purrgo/app_fsm.h"
#include "purrgo/map_controller.h"
#include "purrgo/config.h"
#include "purrgo/system_time.h"
#include "purrgo/gnss.h"
#include "purrgo/gnss_adapter.h"
#include "purrgo/gnss_ubx.h"
#include "purrgo/sun.h"
#include "purrgo/purrgo_time.h"
#include "purrgo/map_controller.h"
#include "purrgo/trip_computer.h"
#include "purrgo/menu_controller.h"
#include "purrgo/track_logger.h"
#include "purrgo/supply.h"
#include "purrgo/logger.h"
#include "purrgo/geo.h"
#include "purrgo/hardware_config.h"
#include "purrgo/book_reader.h"
#include "purrgo/battery.h"
#include "purrgo/waypoint.h"

#define PURRGO_GNSS_MARKER_UPDATE_DISTANCE_M 5

/*
 * Таймаут фикса: если корректная RMC не приходила дольше этого времени,
 * позиция считается невалидной (приёмник молчит, антенна отключена и т.п.).
 */
#ifndef PURRGO_GNSS_FIX_TIMEOUT_MS
#define PURRGO_GNSS_FIX_TIMEOUT_MS 5000U
#endif

/*
 * Политика перерисовки по данным (GNSS, заряд батареи) - purrgo/ui/ui_dirty.h:
 * нажатия кнопок и смена валидности фикса перерисовывают экран сразу, остальные
 * изменения копятся и выводятся не чаще PURRGO_UI_MIN_DATA_REDRAW_MS. Здесь
 * только пороги, при которых изменение данных GNSS считается заметным.
 */
#define PURRGO_UI_SPEED_THRESHOLD_KNOTS_100 100   /* 1 уз ~ 1,85 км/ч */
#define PURRGO_UI_ALT_THRESHOLD_M           5
/* Цель навигации на экране Trip computer: заметное изменение расстояния и пеленга. */
#define PURRGO_UI_WPT_DIST_THRESHOLD_M      5
#define PURRGO_UI_WPT_BEARING_THRESHOLD_DEG 5

// Текущее состояние конечного автомата
static purrgo_state_t current_state;
static purrgo_gnss_solution_t prev_fix = {0};

static purrgo_gnss_solution_t internal_gnss_solution = {0};
static purrgo_sun_info_t internal_sun_info = {0};
static bool first_fix_obtained = false;
static uint32_t last_sun_update_ms = 0;
static uint32_t last_gnss_update_ms = 0;
static purrgo_gnss_parser_t gnss_parser;
#if PURRGO_GNSS_UBX
static purrgo_ubx_parser_t ubx_parser;
#endif

// Таймаут фикса GNSS
static uint32_t last_rmc_ms = 0;
static uint32_t last_tick_ms = 0;

// Политика перерисовки
static bool render_gate_active = false;
static uint32_t render_not_before_ms = 0;
static int32_t shown_speed_knots_100 = 0;
static int32_t shown_alt_m = 0;
static uint8_t shown_satellites = 0;
static bool shown_wpt_valid = false;
static bool shown_wpt_arrived = false;
static uint32_t shown_wpt_dist_m = 0;
static uint16_t shown_wpt_bearing_deg = 0;
static bool shown_pos_valid = false;
static int32_t shown_lat_1e7 = 0;
static int32_t shown_lon_1e7 = 0;

// Состояние логгера для механизма отката попыток
static uint32_t next_logger_retry_ms = 0;
static uint8_t logger_start_failures = 0;
static track_logger_state_t prev_logger_state = LOGGER_STATE_IDLE;
static uint32_t logger_recording_since_ms = 0;

// Запись должна идти без сбоев столько времени, чтобы сбросить счётчик неудач.
#define PURRGO_LOGGER_STABLE_MS 60000U

static bool last_rendered_pos_valid = false;
static purrgo_gnss_solution_t last_rendered_fix = {0};

// Планирует следующую попытку старта/восстановления логгера: 5 с, 30 с, затем 5 мин.
static void logger_schedule_retry(uint32_t now_ms) {
    if (logger_start_failures < 255U) {
        logger_start_failures++;
    }

    uint32_t delay_ms;
    if (logger_start_failures == 1U) {
        delay_ms = 5000U;
    } else if (logger_start_failures == 2U) {
        delay_ms = 30000U;
    } else {
        delay_ms = 300000U;
    }
    next_logger_retry_ms = now_ms + delay_ms;
}

static bool is_book_state(purrgo_state_t state) {
    return state == APP_STATE_BOOK_LIST || state == APP_STATE_BOOK_READ ||
           state == APP_STATE_BOOK_SEARCH_INPUT;
}

// Экраны, на которых в строке статуса выводится заряд батареи.
static bool state_shows_battery(purrgo_state_t state) {
    return state == APP_STATE_MAP || state == APP_STATE_BOOK_READ;
}

/*
 * Общий итог обработки кнопки для всех режимов: перерисовка нужна, если
 * контроллер принял кнопку (changed) или экран сменился; возврат на карту -
 * ещё и полная перерисовка карты.
 */
static void ui_input_done(purrgo_state_t prev_state, bool changed) {
    if (!changed && current_state == prev_state) {
        return;
    }
    purrgo_ui_dirty_mark(UI_DIRTY_SCREEN);
    if (current_state == APP_STATE_MAP && prev_state != APP_STATE_MAP) {
        purrgo_ui_dirty_mark(UI_DIRTY_MAP);
    }
}

// Решение приёмника "нет фикса": позиция, курс и скорость невалидны.
static void gnss_solution_invalidate(void) {
    internal_gnss_solution.valid = false;
    internal_gnss_solution.course_valid = false;
    internal_gnss_solution.speed_knots = 0;
    internal_gnss_solution.fix_quality = 0;
    internal_gnss_solution.acc_valid = false;   // оценки точности относятся к потерянному решению
    /*
     * fix_type = 0 - "тип фикса неизвестен" (как после старта): значение 1 из GSA
     * ("нет фикса") выставляет только адаптер. Если здесь поставить 1, а приёмник
     * (или мок на PC) не шлёт GSA, то fix_type так и останется 1 и после появления
     * фикса фильтр качества логгера отбросит все точки: файл трека создан, а
     * точек в нём и на экране нет.
     */
    internal_gnss_solution.fix_type = 0;
}

/*
 * Вход в режим чтения: читалка уже остановила трек и GNSS. Здесь сбрасывается
 * состояние навигатора: фикса нет, накопленная перерисовка по данным не нужна.
 */
static void book_mode_entered(void) {
    gnss_solution_invalidate();
    prev_fix = internal_gnss_solution;
    last_rendered_pos_valid = false;
    shown_pos_valid = false;
    purrgo_ui_dirty_defer_cancel();
}

/*
 * Выход из режима чтения: GNSS уже включён читалкой. Приёмнику нужно время на
 * фикс, поэтому таймаут молчания отсчитывается заново, а карта перерисуется
 * при возврате на неё.
 */
static void book_mode_left(void) {
    const uint32_t now = purrgo_system_time_ms();
    last_rmc_ms = now;
    last_tick_ms = now;
    last_gnss_update_ms = now;
    purrgo_ui_dirty_defer_cancel();
    purrgo_ui_dirty_mark(UI_DIRTY_MAP);
}

/*
 * Вход в режим трекера с отключённым дисплеем: запись трека идёт как в режиме
 * EXPEDITION независимо от настройки LOG. Настройка в app_config не меняется:
 * по выходу логгеру возвращается выбранный пользователем режим. Экран один раз
 * перерисовывается в предупреждение (purrgo_app_ui_render), дальше не меняется.
 */
static void tracker_mode_entered(void) {
    purrgo_logger_set_mode(LOGGER_MODE_EXPEDITION);
    render_gate_active = false;
    purrgo_ui_dirty_defer_cancel();
}

/*
 * Выход из режима трекера: логгеру возвращается режим из настроек (при OFF
 * запись останавливается), приёмник работал всё время, поэтому таймаут фикса
 * отсчитывается заново, а карта перерисуется полностью.
 */
static void tracker_mode_left(void) {
    const uint32_t now = purrgo_system_time_ms();

    purrgo_logger_set_mode(app_config.log_mode);
    last_rmc_ms = now;
    last_tick_ms = now;
    last_gnss_update_ms = now;
    last_rendered_pos_valid = false;
    shown_pos_valid = false;
    purrgo_ui_dirty_defer_cancel();
    purrgo_ui_dirty_mark(UI_DIRTY_MAP);
    purrgo_ui_dirty_mark(UI_DIRTY_STATUS);
}

bool purrgo_app_tracker_mode_active(void) {
    return current_state == APP_STATE_TRACKER_SCREEN_OFF;
}

/*
 * Режим чтения в покое: перерисовки нет и ждать нечего, кроме кнопок и редких
 * тиков (сохранение позиции, заряд батареи, питание), которые платформа
 * отрабатывает периодическим пробуждением. На карте, в меню и в трекере GNSS
 * и экран живут по таймеру, глубокий сон там не нужен.
 */
bool purrgo_app_deep_sleep_allowed(uint32_t now_ms) {
    if (!is_book_state(current_state)) {
        return false;
    }
    if (purrgo_book_reader_search_running()) {
        return false;
    }
    return !purrgo_app_ui_needs_render(now_ms);
}

#if PURRGO_GNSS_UBX
/*
 * Начат новый трек (новый GPX-файл): дистанция трека отсчитывается заново - сброс
 * одометра модуля (UBX-NAV-RESETODO). Вызывается логгером из track_logger_service().
 */
static void on_new_track(void) {
    purrgo_gnss_ubx_reset_trip(purrgo_system_time_ms());
}
#endif

void purrgo_app_init(void) {
    next_logger_retry_ms = 0;
    purrgo_gnss_parser_init(&gnss_parser);
#if PURRGO_GNSS_UBX
    purrgo_ubx_parser_init(&ubx_parser);
    purrgo_gnss_ubx_init();
    purrgo_logger_set_new_track_callback(on_new_track);
#endif
    first_fix_obtained = false;
    last_sun_update_ms = 0;
    last_gnss_update_ms = purrgo_system_time_ms();
    last_rmc_ms = last_gnss_update_ms;
    last_tick_ms = last_gnss_update_ms;
    purrgo_ui_dirty_throttle_reset(last_gnss_update_ms);
    purrgo_battery_monitor_reset();
    purrgo_supply_reset();
    render_gate_active = false;
    shown_pos_valid = false;
    logger_start_failures = 0;
    logger_recording_since_ms = 0;
    /*
     * First try to load the persistent configuration.
     * purrgo_config_load() initializes default values and creates
     * PURRGO.CFG when the file does not exist.
     *
     * Therefore the application should NOT call purrgo_config_init()
     * unconditionally here: doing so would overwrite the values
     * loaded from PURRGO.CFG.
     */
    purrgo_config_load();

    purrgo_map_controller_init();
    purrgo_menu_controller_init();
    purrgo_trip_computer_init();

    current_state = APP_STATE_MAP;
}

purrgo_state_t purrgo_app_get_state(void) {
    return current_state;
}

void purrgo_app_ui_defer_render(uint32_t delay_ms) {
    render_not_before_ms = purrgo_system_time_ms() + delay_ms;
    render_gate_active = true;
}

bool purrgo_app_ui_needs_render(uint32_t now_ms) {
    /*
     * Флаг карты имеет смысл только на экране карты: на остальных экранах
     * он остаётся взведённым до возврата и не должен гнать перерисовку.
     */
    bool map_needs = (current_state == APP_STATE_MAP) && purrgo_ui_dirty_test(UI_DIRTY_MAP);

    if (!purrgo_ui_dirty_test(UI_DIRTY_SCREEN) && !map_needs) {
        return false;
    }

    if (render_gate_active) {
        if ((int32_t)(now_ms - render_not_before_ms) < 0) {
            return false;
        }
        render_gate_active = false;
    }
    return true;
}

void purrgo_app_handle_button(purrgo_btn_t button) {
    bool handled = false;
    const purrgo_state_t prev_state = current_state;
    purrgo_state_t next_state = current_state;

    /*
     * Трекер с отключённым дисплеем: единственное действие - выход по
     * одновременному нажатию K1 и K4. Остальные кнопки (случайные нажатия в
     * кармане) игнорируются и экран не перерисовывают.
     */
    if (current_state == APP_STATE_TRACKER_SCREEN_OFF) {
        if (button == PURRGO_BTN_COMBO_K1_K4) {
            current_state = APP_STATE_MAP;
            tracker_mode_left();
            ui_input_done(prev_state, true);
        }
        return;
    }

    /* Комбинация K1+K4 имеет смысл только в режиме трекера. */
    if (button == PURRGO_BTN_COMBO_K1_K4) {
        return;
    }

    /*
     * Читалка: кнопки (короткие и длинные) разбирает сама читалка, а не общая
     * схема "кнопка -> действие". Выход из читалки - в меню CONFIG.
     */
    if (is_book_state(current_state)) {
        render_gate_active = false;
        const bool redraw = purrgo_book_reader_handle_button(current_state, button, &next_state);
        current_state = next_state;
        if (!is_book_state(current_state)) {
            book_mode_left();
            purrgo_menu_controller_on_enter(APP_STATE_MENU_MAIN);
        }
        ui_input_done(prev_state, redraw);
        return;
    }

    // Map new keys to legacy actions/buttons based on state
    purrgo_action_t mapped_button = PURRGO_ACTION_NONE;

    if (current_state == APP_STATE_MAP) {
        switch (button) {
            case PURRGO_BTN_KEY1_SHORT: mapped_button = PURRGO_ACTION_LEFT; break;
            case PURRGO_BTN_KEY2_SHORT: mapped_button = PURRGO_ACTION_RIGHT; break;
            case PURRGO_BTN_KEY3_SHORT: mapped_button = PURRGO_ACTION_UP; break;
            case PURRGO_BTN_KEY4_SHORT: mapped_button = PURRGO_ACTION_DOWN; break;
            case PURRGO_BTN_KEY1_LONG:  mapped_button = PURRGO_ACTION_MINUS; break;
            case PURRGO_BTN_KEY2_LONG:  mapped_button = PURRGO_ACTION_PLUS; break;
            case PURRGO_BTN_KEY3_LONG:  mapped_button = PURRGO_ACTION_OK; break;
            case PURRGO_BTN_KEY4_LONG:  mapped_button = PURRGO_ACTION_MENU; break;
            default: break;
        }
    } else if (current_state == APP_STATE_TRIP_COMPUTER) {
        switch (button) {
            case PURRGO_BTN_KEY4_SHORT:
            case PURRGO_BTN_KEY4_LONG:
                mapped_button = PURRGO_ACTION_MENU;
                break;
            default: break;
        }
    } else if (purrgo_menu_controller_owns_state(current_state)) {
        switch (button) {
            case PURRGO_BTN_KEY1_SHORT:
            case PURRGO_BTN_KEY1_LONG:
                mapped_button = PURRGO_ACTION_UP;
                break;
            case PURRGO_BTN_KEY2_SHORT:
            case PURRGO_BTN_KEY2_LONG:
                mapped_button = PURRGO_ACTION_DOWN;
                break;
            case PURRGO_BTN_KEY3_SHORT:
            case PURRGO_BTN_KEY3_LONG:
                mapped_button = PURRGO_ACTION_OK;
                break;
            case PURRGO_BTN_KEY4_SHORT:
            case PURRGO_BTN_KEY4_LONG:
                mapped_button = PURRGO_ACTION_MENU;
                break;
            default: break;
        }
    }

    // Нераспознанная кнопка ничего не делает и не вызывает перерисовку.
    if (mapped_button == PURRGO_ACTION_NONE) {
        return;
    }

    // Реакция на пользователя не ждёт интервала между обновлениями данных.
    render_gate_active = false;

    // 1 & 2. Dispatch to module based on state
    if (purrgo_menu_controller_owns_state(current_state)) {
        handled = purrgo_menu_controller_handle_button(current_state, mapped_button, &next_state);
        current_state = next_state;
        if (is_book_state(current_state)) {
            book_mode_entered();   // пункт BOOKS: читалка уже выключила GNSS и трек
        } else if (current_state == APP_STATE_TRACKER_SCREEN_OFF) {
            tracker_mode_entered();   // пункт меню TRACKER: трек как EXPEDITION, экран отключается
        }
    } else if (current_state == APP_STATE_MAP) {
        handled = purrgo_map_controller_handle_button(mapped_button);
    } else if (current_state == APP_STATE_TRIP_COMPUTER) {
        handled = purrgo_trip_computer_handle_button(mapped_button);
    }

    if (handled) {
        // Контроллер принял кнопку: состояние экрана могло измениться.
        ui_input_done(prev_state, true);
        return;
    }

    // 3. Циклическое переключение основных экранов (Garmin eTrex Page Loop)
    if (mapped_button == PURRGO_ACTION_MENU) {
        switch (current_state) {
            case APP_STATE_MAP:
                current_state = APP_STATE_TRIP_COMPUTER;
                break;

            case APP_STATE_TRIP_COMPUTER:
                current_state = APP_STATE_MENU_MAIN;
                purrgo_menu_controller_on_enter(APP_STATE_MENU_MAIN);
                break;

            default:
                current_state = APP_STATE_MAP;
                purrgo_ui_dirty_mark(UI_DIRTY_MAP);
                break;
        }
    }

    // Переход между экранами всегда требует перерисовки.
    ui_input_done(prev_state, false);
}

void purrgo_app_notify_marker_rendered(const purrgo_gnss_solution_t* rendered_fix) {
    if (rendered_fix && rendered_fix->valid) {
        last_rendered_fix = *rendered_fix;
        last_rendered_pos_valid = true;
    } else {
        last_rendered_pos_valid = false;
    }
}

const purrgo_gnss_solution_t* purrgo_app_get_gnss_solution(void) {
    return &internal_gnss_solution;
}

const purrgo_sun_info_t* purrgo_app_get_sun_info(void) {
    return first_fix_obtained ? &internal_sun_info : NULL;
}

void purrgo_app_feed_gnss_byte(uint8_t byte) {
#if PURRGO_GNSS_UBX
    /*
     * В потоке вперемешку NMEA-строки и бинарные кадры UBX. Кадр может начаться,
     * только пока NMEA-строка не начата (после её конца разборщик пуст): иначе
     * байт B5 внутри текста сорвал бы разбор. Принятый PVT, как и RMC, означает,
     * что приёмник жив (таймаут фикса).
     */
    if (gnss_parser.length == 0U || purrgo_ubx_parser_active(&ubx_parser)) {
        const purrgo_ubx_feed_t fed = purrgo_ubx_parser_feed(&ubx_parser, byte);

        if (fed == PURRGO_UBX_FEED_FRAME) {
            const uint32_t now = purrgo_system_time_ms();

            if (purrgo_gnss_ubx_process_frame(ubx_parser.msg_class, ubx_parser.msg_id,
                                              ubx_parser.payload, ubx_parser.length,
                                              &internal_gnss_solution, now)) {
                last_rmc_ms = now;
            }
            return;
        }
        if (fed == PURRGO_UBX_FEED_CONSUMED) {
            return;
        }
    }
#endif
    if (purrgo_gnss_parser_feed(&gnss_parser, byte)) {
        if (purrgo_gnss_process_nmea(gnss_parser.line, &internal_gnss_solution)) {
            last_rmc_ms = purrgo_system_time_ms();
        }
        purrgo_gnss_parser_init(&gnss_parser);
    }
}

// Сбрасывает валидность позиции, если приёмник замолчал.
static void gnss_check_fix_timeout(uint32_t now_ms) {
    /*
     * Если главный цикл простоял (блокирующий refresh e-ink занимает секунды),
     * молчание приёмника оценить нельзя: байты ждут в буфере DMA. Даём новый
     * срок, чтобы не мигать "нет фикса" после каждого полного обновления.
     */
    if ((uint32_t)(now_ms - last_tick_ms) > (PURRGO_GNSS_FIX_TIMEOUT_MS / 2U)) {
        last_rmc_ms = now_ms;
    }
    last_tick_ms = now_ms;

    if (internal_gnss_solution.valid &&
        (uint32_t)(now_ms - last_rmc_ms) > PURRGO_GNSS_FIX_TIMEOUT_MS) {
        PURRGO_LOGW("GNSS fix timeout\r\n");
        gnss_solution_invalidate();
    }
}

void purrgo_app_tick(uint32_t current_time_ms) {
    /*
     * Напряжение питания: при просадке ниже порога безопасной записи на SD
     * закрываем трек и размонтируем карту (supply.c). Первым в такте: чем
     * раньше остановим запись, тем больше запас по питанию.
     */
    (void)purrgo_supply_tick(current_time_ms);

    // Читалка: запись позиции чтения на SD по тайм-ауту бездействия и очередной шаг поиска по книге.
    if (purrgo_book_reader_tick(current_time_ms)) {
        purrgo_ui_dirty_mark(UI_DIRTY_SCREEN);
    }

    /*
     * Заряд батареи в строке статуса (карта и читалка): общий монитор, опрос
     * раз в PURRGO_BATTERY_STATUS_PERIOD_MS. Перерисовка только при изменении
     * текста и только там, где заряд виден; идёт через общую отсрочку данных.
     */
    if (purrgo_battery_monitor_tick(current_time_ms) && state_shows_battery(current_state)) {
        purrgo_ui_dirty_mark(UI_DIRTY_STATUS);
        purrgo_ui_dirty_defer();
    }

    gnss_check_fix_timeout(current_time_ms);

    // Накопленные изменения данных выводим не чаще минимального интервала.
    purrgo_ui_dirty_tick(current_time_ms);

    if (current_time_ms - last_gnss_update_ms >= 1000U) {
        last_gnss_update_ms = current_time_ms;

        purrgo_app_update(&internal_gnss_solution);

        if (internal_gnss_solution.valid) {
            if (!first_fix_obtained || (current_time_ms - last_sun_update_ms >= 60000U)) {
                purrgo_sun_calc(
                    internal_gnss_solution.lat_1e7,
                    internal_gnss_solution.lon_1e7,
                    internal_gnss_solution.year % 100U,
                    internal_gnss_solution.month,
                    internal_gnss_solution.day,
                    internal_gnss_solution.hours,
                    internal_gnss_solution.minutes,
                    app_config.tz_offset_minutes,
                    &internal_sun_info
                );
                first_fix_obtained = true;
                last_sun_update_ms = current_time_ms;
            }
        }
    }
}

/*
 * Обслуживание трек-логгера: старт/повторные попытки, запись точек.
 * Вызывается раз в секунду из purrgo_app_update() на всех экранах, кроме читалки.
 */
static void track_logger_service(const purrgo_gnss_solution_t* current_fix) {
    /*
     * Низкое напряжение питания: запись на карту остановлена (supply.c), новые
     * сессии не стартуем и повторных попыток не планируем.
     */
    if (purrgo_supply_storage_blocked()) {
        prev_logger_state = purrgo_logger_get_state();
        return;
    }
    {
        const uint32_t now = purrgo_system_time_ms();
        track_logger_state_t logger_state = purrgo_logger_get_state();
        track_logger_mode_t logger_mode = purrgo_logger_get_mode();

        /*
         * Сбой записи во время сессии (RECORDING -> ERROR): планируем
         * повторную попытку с нарастающей паузой.
         */
        if (logger_state == LOGGER_STATE_ERROR && prev_logger_state == LOGGER_STATE_RECORDING) {
            logger_schedule_retry(now);
        }

        /*
         * Счётчик неудач сбрасывается, только когда запись непрерывно идёт
         * достаточно долго; иначе "переоткрылся и сразу упал" давал бы
         * повторные попытки каждую секунду.
         */
        if (logger_state == LOGGER_STATE_RECORDING) {
            if (prev_logger_state != LOGGER_STATE_RECORDING) {
                logger_recording_since_ms = now;
            } else if (logger_start_failures != 0 &&
                       (uint32_t)(now - logger_recording_since_ms) >= PURRGO_LOGGER_STABLE_MS) {
                logger_start_failures = 0;
            }
        }
        prev_logger_state = logger_state;

        if (current_fix->valid) {
            if (logger_mode != LOGGER_MODE_OFF &&
                (logger_state == LOGGER_STATE_IDLE || logger_state == LOGGER_STATE_ERROR)) {
                if (logger_start_failures == 0 || (int32_t)(now - next_logger_retry_ms) >= 0) {
                    if (purrgo_logger_start(current_fix)) {
                        PURRGO_LOG("Track logging started\n\r");
                        logger_state = purrgo_logger_get_state();
                        prev_logger_state = logger_state;
                        logger_recording_since_ms = now;
                    } else {
                        logger_schedule_retry(now);
                    }
                }
            }

            if (logger_state == LOGGER_STATE_RECORDING) {
                // Передаем координаты в фильтр (он сам решит, записывать ли точку)
                if (purrgo_logger_add_point(current_fix)) {
                    purrgo_ui_dirty_mark(UI_DIRTY_TRACK);
                    /*
                     * Сам флаг TRACK экран не перерисовывает: без этого новые
                     * отрезки ждали бы, пока маркёр сместится или сменится
                     * минута. Перерисовка ставится в очередь и выводится не
                     * чаще PURRGO_UI_MIN_DATA_REDRAW_MS, как остальные данные.
                     */
                    if (current_state == APP_STATE_MAP) {
                        purrgo_ui_dirty_defer();
                    }
                }
            }
        }
    }
}

void purrgo_app_update(const purrgo_gnss_solution_t* current_fix) {
    bool pos_changed_significantly = false;

    /*
     * В режиме чтения GNSS выключен, трек не пишется, а экран не зависит от
     * времени и позиции: любая перерисовка здесь только тратит заряд.
     */
    if (is_book_state(current_state)) {
        return;
    }

    /*
     * Трекер с отключённым дисплеем: работает только запись трека. Всё, что
     * могло бы вызвать перерисовку (смена фикса, минуты, смещение маркера,
     * состояние логгера), пропускается: каждое обновление e-ink - это расход
     * заряда, а экран всё равно не должен меняться.
     */
    if (current_state == APP_STATE_TRACKER_SCREEN_OFF) {
        prev_fix = *current_fix;
        track_logger_service(current_fix);
        return;
    }

    /*
     * Эталонная позиция для сравнения: на карте - позиция, на которой реально
     * нарисован маркер; на остальных экранах маркер не рисуется, поэтому
     * эталоном служит позиция, уже отправленная в очередь на вывод. Иначе
     * после ухода с карты расхождение с последним маркером оставалось бы
     * "значительным" вечно и вызывало перерисовку на каждом интервале.
     */
    /* Расстояние и пеленг до цели (раз в секунду, вместе с остальными данными). */
    purrgo_wpt_update(current_fix);

    /*
     * Снимок данных путевого компьютера (дистанция трека, точность, помехи) - раз в
     * секунду на любом экране, чтобы при открытии экрана данные уже были свежими.
     */
    purrgo_trip_computer_update(current_fix);
    const bool trip_info_changed = purrgo_trip_computer_take_changed();

    const bool use_marker_ref = (current_state == APP_STATE_MAP);
    const bool ref_valid = use_marker_ref ? last_rendered_pos_valid : shown_pos_valid;
    const int32_t ref_lat = use_marker_ref ? last_rendered_fix.lat_1e7 : shown_lat_1e7;
    const int32_t ref_lon = use_marker_ref ? last_rendered_fix.lon_1e7 : shown_lon_1e7;

    if (current_fix->valid) {
        if (!ref_valid) {
            pos_changed_significantly = true;
        } else {
            uint32_t dist = purrgo_geo_distance_m(
                ref_lat, ref_lon,
                current_fix->lat_1e7, current_fix->lon_1e7
            );
            if (dist >= PURRGO_GNSS_MARKER_UPDATE_DISTANCE_M) {
                pos_changed_significantly = true;
            }
        }
    } else {
        // Позиция пропала: нужно убрать маркер/обновить статус
        if (ref_valid) {
            pos_changed_significantly = true;
        }
    }

    /*
     * Политика перерисовки.
     *   - смена валидности фикса: сразу (важное событие для пользователя);
     *   - остальное (время, смещение маркера, скорость/высота/спутники на
     *     экране Trip computer): откладывается до минимального интервала
     *     и учитывается только при заметном изменении значения.
     */
    if (current_fix->valid != prev_fix.valid) {
        purrgo_ui_dirty_now(purrgo_system_time_ms());
    }

    bool data_changed = pos_changed_significantly ||
                        current_fix->minutes != prev_fix.minutes ||
                        current_fix->hours != prev_fix.hours;

    if (current_state == APP_STATE_TRIP_COMPUTER) {
        if (trip_info_changed) {
            data_changed = true;
        }
        int32_t dv = current_fix->speed_knots - shown_speed_knots_100;
        int32_t da = current_fix->alt_m - shown_alt_m;
        if (dv < 0) dv = -dv;
        if (da < 0) da = -da;

        if (dv >= PURRGO_UI_SPEED_THRESHOLD_KNOTS_100 ||
            da >= PURRGO_UI_ALT_THRESHOLD_M ||
            current_fix->satellites_tracked != shown_satellites) {
            data_changed = true;
        }

        const purrgo_wpt_nav_t* wn = purrgo_wpt_nav();
        int32_t dd = (int32_t)wn->nav.distance_to_wp_m - (int32_t)shown_wpt_dist_m;
        int32_t db = (int32_t)wn->nav.bearing_to_wp_deg - (int32_t)shown_wpt_bearing_deg;
        if (dd < 0) dd = -dd;
        if (db < 0) db = -db;
        if (db > 180) db = 360 - db;

        if (wn->valid != shown_wpt_valid ||
            (wn->valid && (wn->nav.is_arrived != shown_wpt_arrived ||
                           dd >= PURRGO_UI_WPT_DIST_THRESHOLD_M ||
                           db >= PURRGO_UI_WPT_BEARING_THRESHOLD_DEG))) {
            data_changed = true;
            shown_wpt_valid = wn->valid;
            shown_wpt_arrived = wn->nav.is_arrived;
            shown_wpt_dist_m = wn->nav.distance_to_wp_m;
            shown_wpt_bearing_deg = wn->nav.bearing_to_wp_deg;
        }
    }

    if (data_changed) {
        purrgo_ui_dirty_defer();
        // Отправленные на экран значения запоминаем в момент постановки в очередь.
        shown_speed_knots_100 = current_fix->speed_knots;
        shown_alt_m = current_fix->alt_m;
        shown_satellites = current_fix->satellites_tracked;
        shown_pos_valid = current_fix->valid;
        shown_lat_1e7 = current_fix->lat_1e7;
        shown_lon_1e7 = current_fix->lon_1e7;
    }

    if (current_fix->valid != prev_fix.valid ||
        current_fix->minutes != prev_fix.minutes ||
        current_fix->hours != prev_fix.hours) {

        purrgo_ui_dirty_mark(UI_DIRTY_STATUS);
    }

    static track_logger_state_t prev_rec_state = LOGGER_STATE_IDLE;
    if (purrgo_logger_get_state() != prev_rec_state) {
        prev_rec_state = purrgo_logger_get_state();
        purrgo_ui_dirty_mark(UI_DIRTY_SCREEN);
        purrgo_ui_dirty_mark(UI_DIRTY_STATUS);
    }

    prev_fix = *current_fix;
    purrgo_gnss_solution_t display_fix;

    // Пересчет UTC времени в локальное с использованием специализированного модуля purrgo_time
    if (!purrgo_time_apply_timezone(current_fix, &display_fix, app_config.tz_offset_minutes)) {
        display_fix = *current_fix; // fallback to UTC if conversion fails (e.g. out of bounds)
    }


    track_logger_service(current_fix);

    // Диспетчеризация фоновой логики приложения в зависимости от активного экрана
    switch (current_state) {
        case APP_STATE_MAP:
            purrgo_map_controller_update(current_fix);
            break;

        case APP_STATE_TRIP_COMPUTER:
            break;  // снимок путевого компьютера обновлён выше, одинаково для всех экранов

        case APP_STATE_MENU_MAIN:
        case APP_STATE_MENU_DIR_SELECT:
        case APP_STATE_MENU_MAP_LAYERS:
        case APP_STATE_MENU_ROUTE_SELECT:
        case APP_STATE_MENU_WPT_SELECT:
            break;  // меню не требует периодического обновления

        case APP_STATE_BOOK_LIST:
        case APP_STATE_BOOK_READ:
        case APP_STATE_BOOK_SEARCH_INPUT:
        case APP_STATE_TRACKER_SCREEN_OFF:
            break;  // обработано выше (ранний выход)
    }
}
