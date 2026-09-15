#include "purrgo/ui/ui_map_dirty.h"

static bool map_dirty = true;

bool purrgo_ui_map_is_dirty(void) {
    return map_dirty;
}

void purrgo_ui_map_mark_dirty(void) {
    map_dirty = true;
}

void purrgo_ui_map_clear_dirty(void) {
    map_dirty = false;
}
