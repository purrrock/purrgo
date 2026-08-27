#include "purrgo/app_fsm.h"
#include "purrgo/config.h"
#include "purrgo/purrgo_time.h"
#include "purrgo/map_controller.h"
#include "purrgo/trip_computer.h"
#include "purrgo/config_controller.h"

// Текущее состояние конечного автомата
static purrgo_state_t current_state;
static bool ui_dirty = true;
static purrgo_gnss_solution_t prev_fix = {0};

void purrgo_app_init(void) {
    /*
     * First try to load the persistent configuration.
     *
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

bool purrgo_app_ui_is_dirty(void) {
    return ui_dirty;
}

void purrgo_app_ui_clear_dirty(void) {
    ui_dirty = false;
}

void purrgo_app_handle_button(purrgo_btn_t button) {
    ui_dirty = true; // Любое нажатие кнопки требует перерисовки UI
    bool handled = false;
    purrgo_state_t next_state = current_state;

    // 1 & 2. Dispatch to module based on state
    if (current_state == APP_STATE_MENU_CONFIG || current_state == APP_STATE_MENU_DIR_SELECT) {
        handled = purrgo_config_controller_handle_button(current_state, button, &next_state);
        current_state = next_state;
    } else if (current_state == APP_STATE_MAP) {
        handled = purrgo_map_controller_handle_button(button);
    } else if (current_state == APP_STATE_TRIP_COMPUTER) {
        handled = purrgo_trip_computer_handle_button(button);
    }

    if (handled) {
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
                purrgo_config_controller_on_enter(APP_STATE_MENU_CONFIG);
                break;

            default:
                current_state = APP_STATE_MAP;
                purrgo_app_map_mark_dirty();
                break;
        }
    }
}

void purrgo_app_update(const purrgo_gnss_solution_t* current_fix) {
    // Check if relevant navigation data has changed to trigger a UI redraw
    if (current_fix->valid != prev_fix.valid ||
        current_fix->minutes != prev_fix.minutes ||
        current_fix->hours != prev_fix.hours ||
        current_fix->lat_1e7 != prev_fix.lat_1e7 ||
        current_fix->lon_1e7 != prev_fix.lon_1e7 ||
        current_fix->alt_m != prev_fix.alt_m ||
        current_fix->speed_knots != prev_fix.speed_knots ||
        current_fix->satellites_tracked != prev_fix.satellites_tracked) {

        ui_dirty = true;
    }

    prev_fix = *current_fix;
    purrgo_gnss_solution_t display_fix;

    // Пересчет UTC времени в локальное с использованием специализированного модуля purrgo_time
    if (!purrgo_time_apply_timezone(current_fix, &display_fix, app_config.tz_offset_minutes)) {
        display_fix = *current_fix; // fallback to UTC if conversion fails (e.g. out of bounds)
        display_fix.valid = false;
    }

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
            purrgo_config_controller_update(current_fix);
            break;
    }
}

// COMPATIBILITY WRAPPERS: These functions are conceptually owned by the sub-controllers
// but their declarations remain in app_fsm.h as per backward compatibility requirements.
// They delegate to the corresponding implementation.

// From config_controller
extern int16_t config_app_get_draft_tz_offset(void);
extern int config_app_get_config_cursor(void);
extern int config_app_get_dir_list(purrgo_fs_dirent_t** list_out);
extern int config_app_get_dir_cursor(void);

int16_t purrgo_app_get_draft_tz_offset(void) {
    return config_app_get_draft_tz_offset();
}
int purrgo_app_get_config_cursor(void) {
    return config_app_get_config_cursor();
}
int purrgo_app_get_dir_list(purrgo_fs_dirent_t** list_out) {
    return config_app_get_dir_list(list_out);
}
int purrgo_app_get_dir_cursor(void) {
    return config_app_get_dir_cursor();
}

// From map_controller
extern void map_app_map_mark_dirty(void);
extern bool map_app_map_is_dirty(void);
extern void map_app_map_clear_dirty(void);
extern int32_t map_app_get_map_center_lat(void);
extern int32_t map_app_get_map_center_lon(void);
extern purrgo_map_scale_t map_app_get_map_zoom_level(void);
extern uint32_t map_app_get_map_scale_width_m(void);
extern const char* map_app_get_map_scale_label(void);
extern bool map_app_is_manual_pan_active(void);

void purrgo_app_map_mark_dirty(void) {
    map_app_map_mark_dirty();
}
bool purrgo_app_map_is_dirty(void) {
    return map_app_map_is_dirty();
}
void purrgo_app_map_clear_dirty(void) {
    map_app_map_clear_dirty();
}
int32_t purrgo_app_get_map_center_lat(void) {
    return map_app_get_map_center_lat();
}
int32_t purrgo_app_get_map_center_lon(void) {
    return map_app_get_map_center_lon();
}
purrgo_map_scale_t purrgo_app_get_map_zoom_level(void) {
    return map_app_get_map_zoom_level();
}
uint32_t purrgo_app_get_map_scale_width_m(void) {
    return map_app_get_map_scale_width_m();
}
const char* purrgo_app_get_map_scale_label(void) {
    return map_app_get_map_scale_label();
}
bool purrgo_app_is_manual_pan_active(void) {
    return map_app_is_manual_pan_active();
}
