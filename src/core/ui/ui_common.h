#ifndef PURRGO_UI_COMMON_H
#define PURRGO_UI_COMMON_H

#include "purrgo/app_ui.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_text.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/geo.h"
#include "purrgo/map.h"
#include "purrgo/fs_hal.h"
#include "purrgo/config.h"
#include "purrgo/logger.h"
#include <stdio.h>
#include <string.h>

void ui_render_map(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, const purrgo_sun_info_t* sun);
void ui_render_trip_computer(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, const purrgo_sun_info_t* sun);
void ui_render_menu_config(gfx_context_t* gfx);
void ui_render_menu_dir_select(gfx_context_t* gfx);

#endif // PURRGO_UI_COMMON_H
