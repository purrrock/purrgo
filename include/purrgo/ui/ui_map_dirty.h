#ifndef PURRGO_UI_MAP_DIRTY_H
#define PURRGO_UI_MAP_DIRTY_H

#include <stdbool.h>

bool purrgo_ui_map_is_dirty(void);
void purrgo_ui_map_mark_dirty(void);
void purrgo_ui_map_clear_dirty(void);

#endif /* PURRGO_UI_MAP_DIRTY_H */
