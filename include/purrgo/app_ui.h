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

/*
 * Запросить ОДНО полное обновление дисплея в конце текущего прохода
 * purrgo_app_ui_render(). Экранные модули не должны вызывать
 * display_refresh() сами: на E-Ink каждый вызов - это отдельное
 * полное обновление (секунды, мерцание, износ панели), а элементы,
 * которые app_ui дорисовывает после экрана (подсказки кнопок),
 * иначе вызывают второй полный refresh.
 */
void purrgo_app_ui_request_full_refresh(void);

#endif /* PURRGO_APP_UI_H */
