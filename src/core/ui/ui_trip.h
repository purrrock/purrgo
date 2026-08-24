#ifndef PURRGO_UI_TRIP_H
#define PURRGO_UI_TRIP_H

#include "purrgo/app_ui.h"
#include "purrgo/gfx_renderer.h"

void ui_render_trip_computer(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, const purrgo_sun_info_t* sun);

#endif // PURRGO_UI_TRIP_H
