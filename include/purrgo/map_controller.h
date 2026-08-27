#ifndef PURRGO_MAP_CONTROLLER_H
#define PURRGO_MAP_CONTROLLER_H

#include "purrgo/gnss_types.h"
#include "purrgo/app_fsm.h"
#include <stdbool.h>
#include <stdint.h>

void purrgo_map_controller_init(void);
void purrgo_map_controller_update(const purrgo_gnss_solution_t* current_fix);
bool purrgo_map_controller_handle_button(purrgo_btn_t button);

// COMPATIBILITY API DECLARED IN APP_FSM.H
void map_app_map_mark_dirty(void);
bool map_app_map_is_dirty(void);
void map_app_map_clear_dirty(void);
int32_t map_app_get_map_center_lat(void);
int32_t map_app_get_map_center_lon(void);
purrgo_map_scale_t map_app_get_map_zoom_level(void);
uint32_t map_app_get_map_scale_width_m(void);
const char* map_app_get_map_scale_label(void);
bool map_app_is_manual_pan_active(void);

#endif // PURRGO_MAP_CONTROLLER_H
