#include "purrgo/ui/ui_dirty.h"
#include "purrgo/app_fsm.h"
#include "purrgo/map_controller.h"
#include "purrgo/config.h"
#include "purrgo/system_time.h"
#include "purrgo/gnss.h"
#include "purrgo/gnss_adapter.h"
#include "purrgo/gnss_ubx.h"
#include "purrgo/gnss_io.h"
#include "purrgo/sun.h"
#include "purrgo/trip_computer.h"
#include "purrgo/menu_controller.h"
#include "purrgo/track_logger.h"
#include "purrgo/supply.h"
#include "purrgo/logger.h"
#include "purrgo/hardware_config.h"
#include "purrgo/book_reader.h"
#include "purrgo/battery.h"
#include "purrgo/waypoint.h"
#include "purrgo/sound.h"
#include "purrgo/hw_fault.h"
#include "nav/logger_retry.h"
#include "ui/app_data_change.h"
#include "ui/app_keymap.h"

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
 * изменения копятся и выводятся не чаще PURRGO_UI_MIN_DATA_REDRAW_MS. Пороги, при
 * которых изменение данных считается заметным, и показанные на экране значения -
 * в ui/app_data_change.h.
 */

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

/* Диагностические строки приёмника в логе в трекере: раз в минуту (на карте - раз в 5 с). */
#define TRACKER_DIAG_LOG_INTERVAL_MS 60000U

/*
 * Счётчик принятых решений (PVT или RMC): растёт на каждое новое решение приёмника. В трекере с PSMOO
 * решения приходят раз в период, а устаревший фикс остаётся действующим до таймаута; логгер получает
 * фикс один раз, а не каждую секунду (иначе при PURRGO_TRACK_DIAG он писал бы в GPX "отброшенные" копии).
 */
static uint32_t gnss_fix_seq = 0;
static uint32_t tracker_logged_seq = 0;

// Политика перерисовки
static bool render_gate_active = false;
static uint32_t render_not_before_ms = 0;
static purrgo_app_data_change_t data_change = {0};

// Политика повторных попыток логгера (nav/logger_retry.h)
static purrgo_logger_retry_t logger_retry = {0};

static bool is_book_state(purrgo_state_t state) {
    return state == APP_STATE_BOOK_LIST || state == APP_STATE_BOOK_READ ||
           state == APP_STATE_BOOK_SEARCH_INPUT;
}

// Экраны, на которых в строке статуса выводится заряд батареи.
static bool state_shows_battery(purrgo_state_t state) {
    return state == APP_STATE_MAP || state == APP_STATE_BOOK_READ;
}

// Экраны с постоянным индикатором блокировки записи из-за низкого питания (purrgo/supply.h).
static bool state_shows_supply_indicator(purrgo_state_t state) {
    return state == APP_STATE_MAP || state == APP_STATE_TRIP_COMPUTER;
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
    purrgo_app_data_change_forget_marker(&data_change);
    purrgo_app_data_change_forget_pos(&data_change);
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
    tracker_logged_seq = gnss_fix_seq - 1U;   /* действующий фикс уходит логгеру один раз сразу */

    /*
     * Экрану и трекеру не нужен фикс каждую секунду: приёмник - в PSMOO (просыпается раз в период).
     * Таймаут фикса длиннее на всё время режима (gnss_fix_timeout_ms); отсчёт молчания начинается
     * заново, чтобы последняя секунда до входа не считалась.
     */
    purrgo_gnss_set_profile(PURRGO_GNSS_PROFILE_TRACKER);
    last_rmc_ms = purrgo_system_time_ms();
#if PURRGO_GNSS_UBX
    purrgo_gnss_ubx_set_log_interval_ms(TRACKER_DIAG_LOG_INTERVAL_MS);   /* реже, чем на карте */
#endif
}

/*
 * Выход из режима трекера: логгеру возвращается режим из настроек (при OFF
 * запись останавливается), приёмник работал всё время, поэтому таймаут фикса
 * отсчитывается заново, а карта перерисуется полностью.
 */
static void tracker_mode_left(void) {
    const uint32_t now = purrgo_system_time_ms();

    /* Приёмник возвращается к данным каждую секунду (будится активностью на RX, см. gnss_io.h). */
    purrgo_gnss_set_profile(PURRGO_GNSS_PROFILE_NAV);
#if PURRGO_GNSS_UBX
    purrgo_gnss_ubx_set_log_interval_ms(0U);
#endif

    purrgo_logger_set_mode(app_config.log_mode);
    last_rmc_ms = now;
    last_tick_ms = now;
    last_gnss_update_ms = now;
    purrgo_app_data_change_forget_marker(&data_change);
    purrgo_app_data_change_forget_pos(&data_change);
    purrgo_ui_dirty_defer_cancel();
    purrgo_ui_dirty_mark(UI_DIRTY_MAP);
    purrgo_ui_dirty_mark(UI_DIRTY_STATUS);
}

bool purrgo_app_tracker_mode_active(void) {
    return current_state == APP_STATE_TRACKER_SCREEN_OFF;
}

/*
 * Приложение в покое: платформа может остановить МК (Stop 2) до кнопки, данных от приёмника или
 * ближайшего периодического тика (платформа будит МК не реже раза в несколько секунд).
 *
 * Читалка: ждать нечего, кроме кнопок и редких тиков (сохранение позиции, заряд батареи, питание).
 * Карта, путевой компьютер, меню, трекер: всё, что зависит от времени (1-секундный такт, отложенная
 * перерисовка по данным, таймауты фикса и логгера), продвигается по приходу данных GNSS и по
 * периодическому пробуждению; перерисовки по таймеру короче этого периода в ядре нет, кроме паузы
 * после неудачного рендера (render_gate_active) - на её время спать нельзя.
 *
 * Условия платформы (дисплей, кнопки, вход GNSS) проверяет она сама.
 */
bool purrgo_app_deep_sleep_allowed(uint32_t now_ms) {
    if (purrgo_book_reader_search_running()) {
        return false;
    }
    if (render_gate_active) {
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
    purrgo_logger_retry_reset(&logger_retry);
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
    purrgo_app_data_change_reset(&data_change);
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

    /* Запись лога на SD, если включена в настройках (сам файл откроется, когда карта будет готова). */
    purrgo_logger_sd_set_enabled(app_config.sd_log);

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

/*
 * Какой таблицей кнопок пользуется экран. false - у экрана нет раскладки "кнопка ->
 * действие" (читалка и трекер разбирают кнопки сами и сюда не доходят).
 */
static bool state_keymap_screen(purrgo_state_t state, app_keymap_screen_t* screen) {
    if (state == APP_STATE_MAP) {
        *screen = APP_KEYMAP_MAP;
    } else if (state == APP_STATE_TRIP_COMPUTER) {
        *screen = APP_KEYMAP_TRIP;
    } else if (purrgo_menu_controller_owns_state(state)) {
        *screen = APP_KEYMAP_MENU;
    } else {
        return false;
    }
    return true;
}

/*
 * Читалка: кнопки (короткие и длинные) разбирает сама читалка, а не общая
 * схема "кнопка -> действие". Выход из читалки - в меню CONFIG.
 */
static void handle_book_button(purrgo_btn_t button, purrgo_state_t prev_state) {
    purrgo_state_t next_state = current_state;

    render_gate_active = false;
    const bool redraw = purrgo_book_reader_handle_button(current_state, button, &next_state);
    current_state = next_state;
    if (!is_book_state(current_state)) {
        book_mode_left();
        purrgo_menu_controller_on_enter(APP_STATE_MENU_MAIN);
    }
    ui_input_done(prev_state, redraw);
}

/*
 * Действие передаётся контроллеру экрана. true - контроллер принял кнопку (состояние
 * экрана могло измениться). Меню может сменить состояние: вход в читалку или трекер
 * подготавливает режим.
 */
static bool dispatch_action(purrgo_action_t action) {
    if (purrgo_menu_controller_owns_state(current_state)) {
        purrgo_state_t next_state = current_state;
        const bool handled = purrgo_menu_controller_handle_button(current_state, action, &next_state);

        current_state = next_state;
        if (is_book_state(current_state)) {
            book_mode_entered();   // пункт BOOKS: читалка уже выключила GNSS и трек
        } else if (current_state == APP_STATE_TRACKER_SCREEN_OFF) {
            tracker_mode_entered();   // пункт меню TRACKER: трек как EXPEDITION, экран отключается
        }
        return handled;
    }
    if (current_state == APP_STATE_MAP) {
        return purrgo_map_controller_handle_button(action);
    }
    if (current_state == APP_STATE_TRIP_COMPUTER) {
        return purrgo_trip_computer_handle_button(action);
    }
    return false;
}

// Циклическое переключение основных экранов (Garmin eTrex Page Loop): карта -> путевой компьютер -> меню.
static void page_loop_next(void) {
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

void purrgo_app_handle_button(purrgo_btn_t button) {
    const purrgo_state_t prev_state = current_state;

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
     * Тихий щелчок на каждое принятое событие кнопки: E-Ink обновляется секунды, и звук
     * подтверждает, что нажатие дошло до прибора (purrgo/sound.h; при SOUND = OFF - тишина).
     */
    purrgo_sound_play(PURRGO_SOUND_CLICK);

    if (is_book_state(current_state)) {
        handle_book_button(button, prev_state);
        return;
    }

    // Кнопка -> действие по таблице текущего экрана (ui/app_keymap.c)
    purrgo_action_t action = PURRGO_ACTION_NONE;
    app_keymap_screen_t keymap_screen;

    if (state_keymap_screen(current_state, &keymap_screen)) {
        action = purrgo_app_keymap_action(keymap_screen, button);
    }

    // Нераспознанная кнопка ничего не делает и не вызывает перерисовку.
    if (action == PURRGO_ACTION_NONE) {
        return;
    }

    // Реакция на пользователя не ждёт интервала между обновлениями данных.
    render_gate_active = false;

    if (dispatch_action(action)) {
        // Контроллер принял кнопку: состояние экрана могло измениться.
        ui_input_done(prev_state, true);
        return;
    }

    // Контроллер кнопку не принял: "меню" листает основные экраны.
    if (action == PURRGO_ACTION_MENU) {
        page_loop_next();
    }

    // Переход между экранами всегда требует перерисовки.
    ui_input_done(prev_state, false);
}

void purrgo_app_notify_marker_rendered(const purrgo_gnss_solution_t* rendered_fix) {
    purrgo_app_data_change_marker_rendered(&data_change, rendered_fix);
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
                gnss_fix_seq++;
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
            gnss_fix_seq++;
        }
        purrgo_gnss_parser_init(&gnss_parser);
    }
}

/*
 * Возраст метки времени относительно now_ms. Метка может оказаться НОВЕЕ now_ms: главный
 * цикл берёт время такта в начале прохода, а байты GNSS разбираются позже (опрос дисплея,
 * вывод монитора), и кадр получает метку на 1 мс и более больше. Беззнаковая разность тогда
 * даёт ~4,29e9 и ложный таймаут ("GNSS fix timeout"): фикс пропадал на секунду и возвращался
 * со следующим кадром. Такая метка считается свежей (возраст 0).
 */
static uint32_t stamp_age_ms(uint32_t now_ms, uint32_t stamp_ms) {
    const int32_t diff = (int32_t)(now_ms - stamp_ms);

    return (diff > 0) ? (uint32_t)diff : 0U;
}

/*
 * Срок, после которого молчание приёмника считается потерей фикса. В трекере с PSMOO приёмник молчит
 * между фиксами (период PURRGO_GNSS_TRACKER_PERIOD_S), поэтому там срок длиннее.
 */
static uint32_t gnss_fix_timeout_ms(void) {
#if PURRGO_GNSS_TRACKER_ACTIVE
    if (current_state == APP_STATE_TRACKER_SCREEN_OFF) {
        return PURRGO_GNSS_TRACKER_FIX_TIMEOUT_MS;
    }
#endif
    return PURRGO_GNSS_FIX_TIMEOUT_MS;
}

// Сбрасывает валидность позиции, если приёмник замолчал.
static void gnss_check_fix_timeout(uint32_t now_ms) {
    const uint32_t timeout_ms = gnss_fix_timeout_ms();

    /*
     * Если главный цикл простоял (блокирующий refresh e-ink занимает секунды),
     * молчание приёмника оценить нельзя: байты ждут в буфере DMA. Даём новый
     * срок, чтобы не мигать "нет фикса" после каждого полного обновления.
     */
    if (stamp_age_ms(now_ms, last_tick_ms) > (PURRGO_GNSS_FIX_TIMEOUT_MS / 2U)) {
        last_rmc_ms = now_ms;
    }
    last_tick_ms = now_ms;

    if (internal_gnss_solution.valid &&
        stamp_age_ms(now_ms, last_rmc_ms) > timeout_ms) {
        PURRGO_LOGW("GNSS fix timeout\r\n");
        gnss_solution_invalidate();
    }
}

/*
 * Восход/закат: считаются по первому фиксу и затем раз в минуту. Первый фикс после
 * включения - ещё и сигнал (PURRGO_SOUND_FIX_BEEP).
 */
static void sun_service(uint32_t now_ms) {
    if (!internal_gnss_solution.valid) {
        return;
    }
    if (first_fix_obtained && (now_ms - last_sun_update_ms < 60000U)) {
        return;
    }

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
#if PURRGO_SOUND_FIX_BEEP
    if (!first_fix_obtained) {
        purrgo_sound_play(PURRGO_SOUND_INFO);   /* первый фикс после включения */
    }
#endif
    first_fix_obtained = true;
    last_sun_update_ms = now_ms;
}

void purrgo_app_tick(uint32_t current_time_ms) {
    /* Сигнал о сбое железа (SD, GNSS, E-Ink, АЦП...), заказанный purrgo_hw_fault(), в том числе из прерывания. */
    purrgo_hw_fault_service();

    /*
     * Напряжение питания: при просадке ниже порога безопасной записи на SD
     * закрываем трек (supply.c). Первым в такте: чем раньше остановим запись,
     * тем больше запас по питанию.
     *
     * Остановка записи (и смена причины) - важное событие: индикатор на карте и
     * в путевом компьютере выводится сразу, без отсрочки перерисовки по данным,
     * чтобы пользователь узнал о стопе до потери трека.
     */
    if (purrgo_supply_tick(current_time_ms) && state_shows_supply_indicator(current_state)) {
        purrgo_ui_dirty_mark(UI_DIRTY_STATUS);
        purrgo_ui_dirty_now(current_time_ms);
    }

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
        sun_service(current_time_ms);
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
        purrgo_logger_retry_sync(&logger_retry, purrgo_logger_get_state());
        return;
    }

    const uint32_t now = purrgo_system_time_ms();
    track_logger_state_t logger_state = purrgo_logger_get_state();
    const track_logger_mode_t logger_mode = purrgo_logger_get_mode();

    /* Сбой SD во время сессии - без звука, сигнал только при критическом разряде (supply.c). */
    purrgo_logger_retry_observe(&logger_retry, now, logger_state);

    if (!current_fix->valid) {
        return;
    }

    if (logger_mode != LOGGER_MODE_OFF &&
        (logger_state == LOGGER_STATE_IDLE || logger_state == LOGGER_STATE_ERROR) &&
        purrgo_logger_retry_start_allowed(&logger_retry, now)) {
        if (purrgo_logger_start(current_fix)) {
            PURRGO_LOG("Track logging started\n\r");
            logger_state = purrgo_logger_get_state();
            purrgo_logger_retry_on_start_ok(&logger_retry, now, logger_state);
        } else {
            purrgo_logger_retry_on_start_failed(&logger_retry, now);
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

/*
 * Политика перерисовки по данным раз в секунду.
 *   - смена валидности фикса: сразу (важное событие для пользователя);
 *   - остальное (время, смещение маркера, скорость/высота/спутники на
 *     экране Trip computer): откладывается до минимального интервала
 *     и учитывается только при заметном изменении значения
 *     (ui/app_data_change.h).
 * В конце запоминает fix как prev_fix: следующий вызов сравнивает с ним.
 */
static void update_redraw_policy(const purrgo_gnss_solution_t* fix, bool trip_info_changed) {
    const bool valid_changed = fix->valid != prev_fix.valid;
    const bool clock_changed = fix->minutes != prev_fix.minutes || fix->hours != prev_fix.hours;

    if (valid_changed) {
        purrgo_ui_dirty_now(purrgo_system_time_ms());
    }

    // Эталон сравнения позиции: на карте - нарисованный маркер, иначе - уже отправленная на вывод позиция.
    bool data_changed = clock_changed ||
                        purrgo_app_data_change_pos_moved(&data_change, current_state == APP_STATE_MAP, fix);

    if (current_state == APP_STATE_TRIP_COMPUTER) {
        if (trip_info_changed) {
            data_changed = true;
        }
        if (purrgo_app_data_change_trip_changed(&data_change, fix, purrgo_wpt_nav())) {
            data_changed = true;
        }
    }

    if (data_changed) {
        purrgo_ui_dirty_defer();
        // Отправленные на экран значения запоминаем в момент постановки в очередь.
        purrgo_app_data_change_commit(&data_change, fix);
    }

    if (valid_changed || clock_changed) {
        purrgo_ui_dirty_mark(UI_DIRTY_STATUS);
    }

    static track_logger_state_t prev_rec_state = LOGGER_STATE_IDLE;
    if (purrgo_logger_get_state() != prev_rec_state) {
        prev_rec_state = purrgo_logger_get_state();
        purrgo_ui_dirty_mark(UI_DIRTY_SCREEN);
        purrgo_ui_dirty_mark(UI_DIRTY_STATUS);
    }

    prev_fix = *fix;
}

void purrgo_app_update(const purrgo_gnss_solution_t* current_fix) {
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
        if (gnss_fix_seq != tracker_logged_seq) {
            tracker_logged_seq = gnss_fix_seq;
            track_logger_service(current_fix);
        } else {
            /* Нового решения нет: обслуживаем логгер (повторы, состояние), точку не подаём. */
            static const purrgo_gnss_solution_t no_fix;
            track_logger_service(&no_fix);
        }
        return;
    }

    /* Расстояние и пеленг до цели (раз в секунду, вместе с остальными данными). */
    purrgo_wpt_update(current_fix);

    /*
     * Снимок данных путевого компьютера (дистанция трека, точность, помехи) - раз в
     * секунду на любом экране, чтобы при открытии экрана данные уже были свежими.
     */
    purrgo_trip_computer_update(current_fix);
    const bool trip_info_changed = purrgo_trip_computer_take_changed();

    update_redraw_policy(current_fix, trip_info_changed);
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
