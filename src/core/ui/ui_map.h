#ifndef PURRGO_UI_MAP_H
#define PURRGO_UI_MAP_H

#include "purrgo/app_ui.h"
#include "purrgo/gfx_renderer.h"

void ui_render_map(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, const purrgo_sun_info_t* sun);

/* Сброс состояния экрана карты (кэш маркера и строки статуса, счётчик повторов) - для тестов. */
void ui_map_reset(void);

#endif // PURRGO_UI_MAP_H
