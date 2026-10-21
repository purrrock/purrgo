#ifndef PURRGO_APP_UI_H
#define PURRGO_APP_UI_H

#include <stdbool.h>
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

/*
 * Финальный экран перед выключением (из меню или по разряду батареи):
 * последняя карта с треком и маркером; верхняя и нижняя строки статуса -
 * инверсный текст "POWER OFF" (в верхней остаётся масштаб карты).
 *
 * Рисует только во framebuffer и ничего не обновляет на дисплее: после
 * вызова платформа сама делает display_refresh() и display_sleep().
 * Вызывать ДО purrgo_logger_stop() (трек берётся из RAM-буфера логгера) и
 * ДО размонтирования SD (с неё читаются файлы карты). Использует
 * gfx-контекст и данные ГНСС последнего purrgo_app_ui_render(); если
 * рендера ещё не было - ничего не делает и возвращает false.
 */
bool purrgo_app_ui_render_power_off(void);

/* true, пока выбран экран выключения (его проверяют модули, рисующие строки статуса). */
bool purrgo_app_ui_is_power_off(void);

#endif /* PURRGO_APP_UI_H */
