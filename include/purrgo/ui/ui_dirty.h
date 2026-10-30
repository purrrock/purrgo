#ifndef PURRGO_UI_DIRTY_H
#define PURRGO_UI_DIRTY_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Флаги «нужна перерисовка». Единое место вместо отдельных флагов в app_fsm,
 * map_controller, ui_map и строке статуса.
 *
 * Вызывать только из главного цикла (флаги - обычная маска без атомарности).
 *
 *   UI_DIRTY_SCREEN - общий: состояние экрана изменилось, нужен проход
 *                     purrgo_app_ui_render(). Забирает purrgo_app_ui_render()
 *                     в начале прохода; всё, что изменится во время прохода,
 *                     взведёт флаг заново.
 *   UI_DIRTY_MAP    - полная перерисовка карты. Сбрасывает ui_render_map() после
 *                     успешной отрисовки (или после исчерпания повторов).
 *   UI_DIRTY_STATUS - перерисовать строку статуса на карте.
 *   UI_DIRTY_TRACK  - дорисовать только последний сегмент трека.
 *
 * В начале работы взведены SCREEN, MAP и STATUS: первый кадр рисуется целиком.
 */
typedef enum {
    UI_DIRTY_SCREEN = 1u << 0,
    UI_DIRTY_MAP    = 1u << 1,
    UI_DIRTY_STATUS = 1u << 2,
    UI_DIRTY_TRACK  = 1u << 3
} purrgo_ui_dirty_t;

void purrgo_ui_dirty_mark(purrgo_ui_dirty_t flags);
void purrgo_ui_dirty_clear(purrgo_ui_dirty_t flags);

/* true, если взведён хотя бы один из перечисленных флагов. */
bool purrgo_ui_dirty_test(purrgo_ui_dirty_t flags);

#endif /* PURRGO_UI_DIRTY_H */
