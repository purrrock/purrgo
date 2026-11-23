#ifndef PURRGO_UI_POI_RESULTS_H
#define PURRGO_UI_POI_RESULTS_H

#include "purrgo/app_ui.h"
#include "purrgo/gfx_renderer.h"

/*
 * Экран APP_STATE_MENU_POI_RESULTS (поиск объектов): "идёт поиск", ошибка, "ничего не найдено"
 * или список ближайших объектов (иконка, название, расстояние, румб).
 */
void ui_render_poi_results(gfx_context_t* gfx);
void ui_poi_results_reset(void);

#endif /* PURRGO_UI_POI_RESULTS_H */
