#include "purrgo/ui/ui_dirty.h"

static uint8_t s_dirty = UI_DIRTY_SCREEN | UI_DIRTY_MAP | UI_DIRTY_STATUS;

/* Перерисовка по данным: накопилась, но ещё не выведена (см. purrgo_ui_dirty_defer). */
static bool     s_deferred = false;
static uint32_t s_last_redraw_ms = 0;

void purrgo_ui_dirty_mark(purrgo_ui_dirty_t flags) {
    s_dirty = (uint8_t)(s_dirty | (uint8_t)flags);
}

void purrgo_ui_dirty_clear(purrgo_ui_dirty_t flags) {
    s_dirty = (uint8_t)(s_dirty & (uint8_t)~(uint8_t)flags);
}

bool purrgo_ui_dirty_test(purrgo_ui_dirty_t flags) {
    return (s_dirty & (uint8_t)flags) != 0u;
}

void purrgo_ui_dirty_throttle_reset(uint32_t now_ms) {
    s_deferred = false;
    s_last_redraw_ms = now_ms;
}

void purrgo_ui_dirty_defer(void) {
    s_deferred = true;
}

void purrgo_ui_dirty_defer_cancel(void) {
    s_deferred = false;
}

void purrgo_ui_dirty_now(uint32_t now_ms) {
    purrgo_ui_dirty_mark(UI_DIRTY_SCREEN);
    s_deferred = false;
    s_last_redraw_ms = now_ms;
}

void purrgo_ui_dirty_tick(uint32_t now_ms) {
    if (s_deferred &&
        (uint32_t)(now_ms - s_last_redraw_ms) >= PURRGO_UI_MIN_DATA_REDRAW_MS) {
        purrgo_ui_dirty_now(now_ms);
    }
}
