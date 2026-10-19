#include "purrgo/ui/ui_map_dirty.h"
#include "purrgo/ui/ui_status_bar.h"
#include "purrgo/app_fsm.h"
#include "purrgo/map_controller.h"
#include "purrgo/config.h"
#include "purrgo/system_time.h"
#include "purrgo/gnss.h"
#include "purrgo/gnss_adapter.h"
#include "purrgo/sun.h"
#include "purrgo/purrgo_time.h"
#include "purrgo/map_controller.h"
#include "purrgo/trip_computer.h"
#include "purrgo/config_controller.h"
#include "purrgo/track_logger.h"
#include "purrgo/logger.h"
#include "purrgo/geo.h"
#include "purrgo/hardware_config.h"

#define PURRGO_GNSS_MARKER_UPDATE_DISTANCE_M 5

/*
 * Таймаут фикса: если корректная RMC не приходила дольше этого времени,
 * позиция считается невалидной (приёмник молчит, антенна отключена и т.п.).
 */
#ifndef PURRGO_GNSS_FIX_TIMEOUT_MS
#define PURRGO_GNSS_FIX_TIMEOUT_MS 5000U
#endif

/*
 * Политика перерисовки по данным GNSS (скорость, высота, спутники, смещение
 * маркера, минуты). Нажатия кнопок и смена валидности фикса перерисовывают
 * экран сразу. Остальные изменения копятся и выводятся не чаще, чем раз в
 * PURRGO_UI_MIN_DATA_REDRAW_MS, и только если значение изменилось заметно.
 */
#ifndef PURRGO_UI_MIN_DATA_REDRAW_MS
#if (PURRGO_HW_DISPLAY_BACKEND == PURRGO_DISPLAY_BACKEND_EINK)
#define PURRGO_UI_MIN_DATA_REDRAW_MS 5000U
#else
#define PURRGO_UI_MIN_DATA_REDRAW_MS 1000U
#endif
#endif

#define PURRGO_UI_SPEED_THRESHOLD_KNOTS_100 100   /* 1 уз ~ 1,85 км/ч */
#define PURRGO_UI_ALT_THRESHOLD_M           5

// Текущее состояние конечного автомата
static purrgo_state_t current_state;
static bool ui_dirty = true;
static purrgo_gnss_solution_t prev_fix = {0};

static purrgo_gnss_solution_t internal_gnss_solution = {0};
static purrgo_sun_info_t internal_sun_info = {0};
static bool first_fix_obtained = false;
static uint32_t last_sun_update_ms = 0;
static uint32_t last_gnss_update_ms = 0;
static purrgo_gnss_parser_t gnss_parser;

// Таймаут фикса GNSS
static uint32_t last_rmc_ms = 0;
static uint32_t last_tick_ms = 0;

// Политика перерисовки
static bool data_redraw_pending = false;
static uint32_t last_data_redraw_ms = 0;
static bool render_gate_active = false;
static uint32_t render_not_before_ms = 0;
static int32_t shown_speed_knots_100 = 0;
static int32_t shown_alt_m = 0;
static uint8_t shown_satellites = 0;
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

void purrgo_app_init(void) {
    next_logger_retry_ms = 0;
    purrgo_gnss_parser_init(&gnss_parser);
    first_fix_obtained = false;
    last_sun_update_ms = 0;
    last_gnss_update_ms = purrgo_system_time_ms();
    last_rmc_ms = last_gnss_update_ms;
    last_tick_ms = last_gnss_update_ms;
    last_data_redraw_ms = last_gnss_update_ms;
    data_redraw_pending = false;
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
    purrgo_config_controller_init();
    purrgo_trip_computer_init();

    current_state = APP_STATE_MAP;
}

purrgo_state_t purrgo_app_get_state(void) {
    return current_state;
}

void purrgo_app_ui_mark_dirty(void) {
    ui_dirty = true;
}

void purrgo_app_ui_consume_dirty(void) {
    ui_dirty = false;
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
    bool map_needs = (current_state == APP_STATE_MAP) && purrgo_ui_map_is_dirty();

    if (!ui_dirty && !map_needs) {
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
    } else if (current_state == APP_STATE_MENU_CONFIG || current_state == APP_STATE_MENU_DIR_SELECT || current_state == APP_STATE_MENU_MAP_LAYERS) {
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
    if (current_state == APP_STATE_MENU_CONFIG || current_state == APP_STATE_MENU_DIR_SELECT || current_state == APP_STATE_MENU_MAP_LAYERS) {
        handled = purrgo_config_controller_handle_button(current_state, mapped_button, &next_state);
        current_state = next_state;
    } else if (current_state == APP_STATE_MAP) {
        handled = purrgo_map_controller_handle_button(mapped_button);
    } else if (current_state == APP_STATE_TRIP_COMPUTER) {
        handled = purrgo_trip_computer_handle_button(mapped_button);
    }

    if (handled) {
        // Контроллер принял кнопку: состояние экрана могло измениться.
        ui_dirty = true;
        if (current_state == APP_STATE_MAP && prev_state != APP_STATE_MAP) {
            purrgo_ui_map_mark_dirty();
        }
        return;
    }

    // 3. Циклическое переключение основных экранов (Garmin eTrex Page Loop)
    if (mapped_button == PURRGO_ACTION_MENU) {
        switch (current_state) {
            case APP_STATE_MAP:
                current_state = APP_STATE_TRIP_COMPUTER;
                break;

            case APP_STATE_TRIP_COMPUTER:
                current_state = APP_STATE_MENU_CONFIG;
                purrgo_config_controller_on_enter(APP_STATE_MENU_CONFIG);
                break;

            default:
                current_state = APP_STATE_MAP;
                purrgo_ui_map_mark_dirty();
                break;
        }
    }

    // Переход между экранами всегда требует перерисовки.
    if (current_state != prev_state) {
        ui_dirty = true;
        if (current_state == APP_STATE_MAP) {
            purrgo_ui_map_mark_dirty();
        }
    }
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
        PURRGO_LOG("GNSS fix timeout\r\n");
        internal_gnss_solution.valid = false;
        internal_gnss_solution.course_valid = false;
        internal_gnss_solution.speed_knots = 0;
        internal_gnss_solution.fix_quality = 0;
        internal_gnss_solution.fix_type = 1;
    }
}

void purrgo_app_tick(uint32_t current_time_ms) {
    gnss_check_fix_timeout(current_time_ms);

    // Накопленные изменения данных выводим не чаще минимального интервала.
    if (data_redraw_pending &&
        (uint32_t)(current_time_ms - last_data_redraw_ms) >= PURRGO_UI_MIN_DATA_REDRAW_MS) {
        data_redraw_pending = false;
        last_data_redraw_ms = current_time_ms;
        ui_dirty = true;
    }

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

void purrgo_app_update(const purrgo_gnss_solution_t* current_fix) {
    bool pos_changed_significantly = false;

    /*
     * Эталонная позиция для сравнения: на карте - позиция, на которой реально
     * нарисован маркер; на остальных экранах маркер не рисуется, поэтому
     * эталоном служит позиция, уже отправленная в очередь на вывод. Иначе
     * после ухода с карты расхождение с последним маркером оставалось бы
     * "значительным" вечно и вызывало перерисовку на каждом интервале.
     */
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
        ui_dirty = true;
        data_redraw_pending = false;
        last_data_redraw_ms = purrgo_system_time_ms();
    }

    bool data_changed = pos_changed_significantly ||
                        current_fix->minutes != prev_fix.minutes ||
                        current_fix->hours != prev_fix.hours;

    if (current_state == APP_STATE_TRIP_COMPUTER) {
        int32_t dv = current_fix->speed_knots - shown_speed_knots_100;
        int32_t da = current_fix->alt_m - shown_alt_m;
        if (dv < 0) dv = -dv;
        if (da < 0) da = -da;

        if (dv >= PURRGO_UI_SPEED_THRESHOLD_KNOTS_100 ||
            da >= PURRGO_UI_ALT_THRESHOLD_M ||
            current_fix->satellites_tracked != shown_satellites) {
            data_changed = true;
        }
    }

    if (data_changed) {
        data_redraw_pending = true;
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

        purrgo_ui_status_bar_mark_dirty();
    }

    static track_logger_state_t prev_rec_state = LOGGER_STATE_IDLE;
    if (purrgo_logger_get_state() != prev_rec_state) {
        prev_rec_state = purrgo_logger_get_state();
        ui_dirty = true;
        purrgo_ui_status_bar_mark_dirty();
    }

    prev_fix = *current_fix;
    purrgo_gnss_solution_t display_fix;

    // Пересчет UTC времени в локальное с использованием специализированного модуля purrgo_time
    if (!purrgo_time_apply_timezone(current_fix, &display_fix, app_config.tz_offset_minutes)) {
        display_fix = *current_fix; // fallback to UTC if conversion fails (e.g. out of bounds)
    }


// === БЛОК ТРЕК-ЛОГГЕРА ===
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
                    purrgo_map_controller_mark_track_dirty();
                }
            }
        }
    }
// ============================================

    // Диспетчеризация фоновой логики приложения в зависимости от активного экрана
    switch (current_state) {
        case APP_STATE_MAP:
            purrgo_map_controller_update(current_fix);
            break;

        case APP_STATE_TRIP_COMPUTER:
            purrgo_trip_computer_update(current_fix);
            break;

        case APP_STATE_MENU_CONFIG:
        case APP_STATE_MENU_DIR_SELECT:
        case APP_STATE_MENU_MAP_LAYERS:
            purrgo_config_controller_update(current_fix);
            break;
    }
}
