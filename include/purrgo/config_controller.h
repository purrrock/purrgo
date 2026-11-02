#ifndef PURRGO_CONFIG_CONTROLLER_H
#define PURRGO_CONFIG_CONTROLLER_H

#include "purrgo/gnss_types.h"
#include "purrgo/app_fsm.h"
#include "purrgo/fs_hal.h"
#include "purrgo/config.h"
#include <stdbool.h>
#include <stdint.h>


/*
 * Число пунктов главного меню CONFIG. Порядок: POWER SOURCE, POWER OFF, TZ,
 * DIR, ROUTE, POI, LOG, MAP DETAILS, MAP LAYERS.
 */
#define PURRGO_CONFIG_MENU_ITEMS 9

void purrgo_config_controller_init(void);

void purrgo_config_controller_update(
    const purrgo_gnss_solution_t* current_fix
);

bool purrgo_config_controller_handle_button(
    purrgo_state_t current_state,
    purrgo_action_t button,
    purrgo_state_t* next_state_out
);

void purrgo_config_controller_on_enter(
    purrgo_state_t state
);


/*
 * API совместимости, объявленный также в app_fsm.h.
 */
int16_t config_app_get_draft_tz_offset(void);
int config_app_get_config_cursor(void);
int config_app_get_dir_list(
    purrgo_fs_dirent_t** list_out
);
int config_app_get_dir_cursor(void);

/*
 * В режиме выбора маршрута запись списка с пустым именем означает "(none)"
 * (снять маршрут).
 */


/*
 * Состояние редактирования POI в меню.
 */
purrgo_poi_mode_t config_app_get_draft_poi_mode(void);

/*
 * Состояние редактирования настроек трека в меню.
 */
track_logger_mode_t config_app_get_draft_log_mode(void);

purrgo_map_details_t config_app_get_draft_map_details(void);

/*
 * Черновик источника питания (выбирает кривую разряда).
 */
purrgo_power_source_t config_app_get_draft_power_source(void);

/*
 * Состояние редактирования слоев карты.
 */
int config_app_get_map_layers_cursor(void);
/* Маска видимости слоёв в черновике: purrgo_layer_on(mask, PURRGO_LAYER_*). */
uint16_t config_app_get_draft_layers(void);


#endif /* PURRGO_CONFIG_CONTROLLER_H */