#ifndef PURRGO_UI_ROWS_H
#define PURRGO_UI_ROWS_H

/*
 * Слежение за строками списка меню для частичного обновления дисплея.
 *
 * Меню перерисовывается целиком во framebuffer, но на E-Ink нужно обновить
 * только изменившиеся строки. Строка считается изменившейся, если у неё
 * другой текст или другое выделение (курсор). Так обновляются и перемещение
 * курсора, и смена значения пункта (SEL), и смена страницы списка.
 *
 * Использование за один проход отрисовки:
 *
 *     ui_rows_begin(&rows, y0, step);
 *     for (...) ui_rows_update(&rows, i, selected, text);
 *     ui_rows_end(&rows, count);
 *
 * Первый проход после ui_rows_reset() ничего не регистрирует: экран при входе
 * в меню обновляется целиком (смена режима/экрана).
 */

#include <stdbool.h>
#include <stdint.h>

#include "purrgo/display_hal.h"
#include "purrgo/hardware_config.h"

#define UI_ROWS_MAX 16

typedef struct {
    uint32_t sig[UI_ROWS_MAX];  /* подпись строки: текст + выделение */
    uint8_t  count;             /* строк на экране после прошлого прохода */
    bool     valid;             /* был хотя бы один проход */
    int16_t  y0;                /* y первой строки, px */
    int16_t  step;              /* высота строки, px */
} ui_rows_t;

static inline void ui_rows_reset(ui_rows_t *r)
{
    r->count = 0;
    r->valid = false;
    r->y0 = 0;
    r->step = 0;
}

static inline void ui_rows_begin(ui_rows_t *r, int16_t y0, int16_t step)
{
    r->y0 = y0;
    r->step = step;
}

static inline void ui_rows_mark(const ui_rows_t *r, int row)
{
    display_refresh_region(0, (int16_t)(r->y0 + row * r->step),
                           PURRGO_HW_DISPLAY_WIDTH_PX, r->step);
}

/* FNV-1a по тексту, выделение подмешивается в конец. */
static inline uint32_t ui_rows_signature(bool selected, const char *text)
{
    uint32_t h = 2166136261u;

    for (const char *p = text; *p != '\0'; p++) {
        h = (h ^ (uint8_t)*p) * 16777619u;
    }
    h = (h ^ (selected ? 0xA5u : 0x5Au)) * 16777619u;
    return h;
}

static inline void ui_rows_update(ui_rows_t *r, int row, bool selected, const char *text)
{
    if (row < 0 || row >= UI_ROWS_MAX) {
        return;
    }

    const uint32_t sig = ui_rows_signature(selected, text);

    if (r->valid && (row >= r->count || r->sig[row] != sig)) {
        ui_rows_mark(r, row);
    }
    r->sig[row] = sig;
}

/* Конец прохода: строки, которые были на экране, а теперь исчезли, тоже обновляются. */
static inline void ui_rows_end(ui_rows_t *r, int count)
{
    if (count > UI_ROWS_MAX) {
        count = UI_ROWS_MAX;
    }
    if (r->valid) {
        for (int row = count; row < r->count; row++) {
            ui_rows_mark(r, row);
        }
    }
    r->count = (uint8_t)count;
    r->valid = true;
}

#endif /* PURRGO_UI_ROWS_H */
