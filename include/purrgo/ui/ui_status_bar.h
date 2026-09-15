#ifndef PURRGO_UI_STATUS_BAR_H
#define PURRGO_UI_STATUS_BAR_H

#include <stdbool.h>

bool purrgo_ui_status_bar_is_dirty(void);
void purrgo_ui_status_bar_mark_dirty(void);
void purrgo_ui_status_bar_clear_dirty(void);

#endif // PURRGO_UI_STATUS_BAR_H
