/*
 * Эталон для test_gfx_line_equiv: прежняя реализация линий (gfx_line.c до ускорения),
 * функции переименованы в ref_*. Файл gfx_line_orig.inc - дословная копия старого gfx_line.c.
 */
#define gfx_draw_line ref_gfx_draw_line
#define gfx_draw_dashed_line ref_gfx_draw_dashed_line
#define gfx_draw_thick_line ref_gfx_draw_thick_line
#define gfx_draw_dotted_line ref_gfx_draw_dotted_line
#define gfx_draw_railway_line ref_gfx_draw_railway_line
#include "gfx_line_orig.inc"
