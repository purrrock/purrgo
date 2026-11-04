#ifndef PURRGO_UI_DIRTY_H
#define PURRGO_UI_DIRTY_H

#include <stdbool.h>
#include <stdint.h>

#include "purrgo/hardware_config.h"

/*
 * Флаги «нужна перерисовка» и политика их выставления. Единое место для всех
 * режимов (карта, меню, читалка): отдельных флагов в app_fsm, map_controller,
 * ui_map, ui_book и строке статуса нет.
 *
 * Вызывать только из главного цикла (флаги - обычная маска без атомарности).
 *
 *   UI_DIRTY_SCREEN - общий: состояние экрана изменилось, нужен проход
 *                     purrgo_app_ui_render(). Забирает purrgo_app_ui_render()
 *                     в начале прохода; всё, что изменится во время прохода,
 *                     взведёт флаг заново.
 *   UI_DIRTY_MAP    - полная перерисовка карты. Сбрасывает ui_render_map() после
 *                     успешной отрисовки (или после исчерпания повторов).
 *   UI_DIRTY_STATUS - перерисовать строку статуса на карте. Читалка сравнивает
 *                     строку статуса при отрисовке сама (см. ui_book.c).
 *   UI_DIRTY_TRACK  - дорисовать только последний сегмент трека.
 *
 * В начале работы взведены SCREEN, MAP и STATUS: первый кадр рисуется целиком.
 *
 * Перерисовка по данным (GNSS, заряд батареи) не должна идти чаще
 * PURRGO_UI_MIN_DATA_REDRAW_MS: на e-ink каждый проход - обновление панели.
 * Такие изменения идут через purrgo_ui_dirty_defer(), реакция на пользователя
 * и важные события - через purrgo_ui_dirty_mark()/purrgo_ui_dirty_now().
 */
typedef enum {
    UI_DIRTY_SCREEN = 1u << 0,
    UI_DIRTY_MAP    = 1u << 1,
    UI_DIRTY_STATUS = 1u << 2,
    UI_DIRTY_TRACK  = 1u << 3
} purrgo_ui_dirty_t;

#ifndef PURRGO_UI_MIN_DATA_REDRAW_MS
#if (PURRGO_HW_DISPLAY_BACKEND == PURRGO_DISPLAY_BACKEND_EINK)
#define PURRGO_UI_MIN_DATA_REDRAW_MS 5000U
#else
#define PURRGO_UI_MIN_DATA_REDRAW_MS 1000U
#endif
#endif

void purrgo_ui_dirty_mark(purrgo_ui_dirty_t flags);
void purrgo_ui_dirty_clear(purrgo_ui_dirty_t flags);

/* true, если взведён хотя бы один из перечисленных флагов. */
bool purrgo_ui_dirty_test(purrgo_ui_dirty_t flags);

/* Начало работы (и повторная инициализация): очередь отложенной перерисовки пуста. */
void purrgo_ui_dirty_throttle_reset(uint32_t now_ms);

/* Данные на экране устарели: перерисовать, но не чаще PURRGO_UI_MIN_DATA_REDRAW_MS. */
void purrgo_ui_dirty_defer(void);

/* Отложенная перерисовка больше не нужна (экран сменился, данные не показываются). */
void purrgo_ui_dirty_defer_cancel(void);

/* Важное событие: перерисовать сразу; ожидающая перерисовка по данным отменяется. */
void purrgo_ui_dirty_now(uint32_t now_ms);

/* Каждый тик: если отложенная перерисовка созрела, взводит UI_DIRTY_SCREEN. */
void purrgo_ui_dirty_tick(uint32_t now_ms);

#endif /* PURRGO_UI_DIRTY_H */
