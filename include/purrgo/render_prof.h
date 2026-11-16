#ifndef PURRGO_RENDER_PROF_H
#define PURRGO_RENDER_PROF_H

/*
 * Профилировка кадра карты (диагностика, по умолчанию выключена).
 *
 * Включается заменой 0 на 1 в строке ниже (флаг сборки -D не нужен). В конце каждого
 * purrgo_map_render_viewport() в лог (purrgo_logger_write, уровень логирования не важен)
 * уходит одна строка PROF. Что и как мерить, описано в docs/measurements/README.md (раздел «Профилировка кадра карты»).
 *
 * Статьи (время в мкс, n - число вызовов):
 *   proj  - project_to_screen*() в обходе геометрии и POI;
 *   fill  - заливка полигонов (gfx_fill_compound_polygon*);
 *   lines - рисование линий карты (gfx_draw_line, thick, dashed, dotted, railway);
 *   lbl   - вывод отложенных подписей landuse;
 *   name  - чтение имени объекта из .db (seek + read);
 *   other - остаток кадра: чтение .idx/.mlp с SD, разбор, отсечение, подписи POI.
 * Статьи не вложены друг в друга, поэтому frame = proj + fill + lines + lbl + name + other.
 *
 * Время берётся из счётчика платформы (stm32: DWT->CYCCNT, pc: timespec_get).
 * Накладные расходы одного замера (ovh, такты) измеряются в конце кадра и вычитаются
 * из статей с большим числом вызовов (proj, name).
 */
#ifndef PURRGO_RENDER_PROF
#define PURRGO_RENDER_PROF 0
#endif

#include <stdint.h>

#if PURRGO_RENDER_PROF

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PURRGO_PROF_PROJ = 0,
    PURRGO_PROF_FILL,
    PURRGO_PROF_LINES,
    PURRGO_PROF_LABELS,
    PURRGO_PROF_NAME,
    PURRGO_PROF_COUNT
} purrgo_prof_id_t;

/* Платформа: монотонный 32-битный счётчик и его частота (тактов в микросекунду). */
void purrgo_prof_platform_enable(void);
uint32_t purrgo_prof_ticks(void);
uint32_t purrgo_prof_ticks_per_us(void);

void purrgo_render_prof_begin_frame(void);
void purrgo_render_prof_add(purrgo_prof_id_t id, uint32_t ticks);
void purrgo_render_prof_end_frame(int scale, int ok);

#ifdef __cplusplus
}
#endif

#define PURRGO_PROF_BEGIN(var) const uint32_t var = purrgo_prof_ticks()
#define PURRGO_PROF_END(var, id) \
    purrgo_render_prof_add((id), (uint32_t)(purrgo_prof_ticks() - (var)))

#else /* !PURRGO_RENDER_PROF */

#define PURRGO_PROF_BEGIN(var) ((void)0)
#define PURRGO_PROF_END(var, id) ((void)0)

#endif

#endif /* PURRGO_RENDER_PROF_H */
