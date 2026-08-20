#ifndef PURRGO_APP_UI_H
#define PURRGO_APP_UI_H

#include <purrgo/gfx_renderer.h>
#include <purrgo/gnss_types.h>
#include <purrgo/sun.h>

void purrgo_app_ui_render(
    gfx_context_t* gfx,
    const purrgo_gnss_solution_t* gnss,
    const purrgo_sun_info_t* sun
);

#endif /* PURRGO_APP_UI_H */
