#include "purrgo/ui/ui_dirty.h"

static uint8_t s_dirty = UI_DIRTY_SCREEN | UI_DIRTY_MAP | UI_DIRTY_STATUS;

void purrgo_ui_dirty_mark(purrgo_ui_dirty_t flags) {
    s_dirty = (uint8_t)(s_dirty | (uint8_t)flags);
}

void purrgo_ui_dirty_clear(purrgo_ui_dirty_t flags) {
    s_dirty = (uint8_t)(s_dirty & (uint8_t)~(uint8_t)flags);
}

bool purrgo_ui_dirty_test(purrgo_ui_dirty_t flags) {
    return (s_dirty & (uint8_t)flags) != 0u;
}
