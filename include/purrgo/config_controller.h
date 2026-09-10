#ifndef PURRGO_CONFIG_CONTROLLER_H
#define PURRGO_CONFIG_CONTROLLER_H

#include "purrgo/gnss_types.h"
#include "purrgo/app_fsm.h"
#include "purrgo/fs_hal.h"
#include "purrgo/config.h"
#include <stdbool.h>
#include <stdint.h>


void purrgo_config_controller_init(void);

void purrgo_config_controller_update(
    const purrgo_gnss_solution_t* current_fix
);

bool purrgo_config_controller_handle_button(
    purrgo_state_t current_state,
    purrgo_btn_t button,
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
 * Состояние редактирования POI в меню.
 */
purrgo_poi_mode_t config_app_get_draft_poi_mode(void);

purrgo_poi_label_mode_t
config_app_get_draft_poi_label_mode(void);


/*
 * Состояние редактирования настроек трека в меню.
 */
track_logger_mode_t config_app_get_draft_log_mode(void);

bool config_app_get_draft_track_display_enabled(void);

/*
 * Состояние редактирования слоев карты.
 */
int config_app_get_map_layers_cursor(void);
bool config_app_get_draft_layer_landuse(void);
bool config_app_get_draft_layer_water(void);
bool config_app_get_draft_layer_landuse_labels(void);
bool config_app_get_draft_layer_water_labels(void);
bool config_app_get_draft_layer_roads(void);
bool config_app_get_draft_layer_poi(void);
bool config_app_get_draft_layer_poi_labels(void);
bool config_app_get_draft_layer_route(void);
bool config_app_get_draft_layer_track(void);


#endif /* PURRGO_CONFIG_CONTROLLER_H */