#include "purrgo/ui/ui_status_bar.h"

static bool status_bar_dirty = true;

bool purrgo_ui_status_bar_is_dirty(void) {
    return status_bar_dirty;
}

void purrgo_ui_status_bar_mark_dirty(void) {
    status_bar_dirty = true;
}

void purrgo_ui_status_bar_clear_dirty(void) {
    status_bar_dirty = false;
}
