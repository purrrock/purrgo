#ifndef GFX_POLYGON_H
#define GFX_POLYGON_H

#include "purrgo/gfx_renderer.h"
#include <stdint.h>

/**
 * @brief Отрисовка контура полигона.
 *
 * @param ctx Контекст графического ядра.
 * @param points Массив вершин полигона.
 * @param count Количество вершин в массиве.
 */
void gfx_draw_polygon(gfx_context_t *ctx, const gfx_point_t *points, uint16_t count);

/**
 * @brief Заливка полигона без holes (обратная совместимость).
 *
 * @param ctx Контекст графического ядра.
 * @param points Массив вершин полигона.
 * @param count Количество вершин в массиве.
 */
void gfx_fill_polygon(gfx_context_t *ctx, const gfx_point_t *points, uint16_t count);

/**
 * @brief Выпуклый полигон с заливкой строго внутри контура (маркёры).
 *
 * Обычные gfx_fill_polygon() + gfx_draw_polygon() растеризуют края по-разному
 * (заливка берёт пересечения с усечением, контур - Bresenham), поэтому на
 * мелких фигурах заливка местами выходит за контур на 1 px. Здесь на каждой
 * строке заливаются только пиксели строго между самым левым и самым правым
 * пикселем контура, а затем поверх рисуется сам контур (те же пиксели, что у
 * gfx_draw_polygon()): заливка не может оказаться снаружи рамки.
 *
 * Только для выпуклых полигонов высотой до GFX_BORDERED_MAX_H строк (иначе
 * рисуется обычная заливка и контур). Цвета foreground/background контекста
 * не меняются.
 *
 * @param fill   Цвет заливки.
 * @param border Цвет контура.
 */
#define GFX_BORDERED_MAX_H 32

void gfx_draw_polygon_bordered(
    gfx_context_t *ctx,
    const gfx_point_t *points,
    uint16_t count,
    gfx_color_t fill,
    gfx_color_t border
);

/**
 * @brief Заливка compound полигона (с holes).
 *
 * @param ctx Контекст графического ядра.
 * @param points Массив вершин полигона.
 * @param num_points Общее количество вершин во всех частях.
 * @param parts Массив начальных индексов частей полигона (колец).
 * @param num_parts Количество частей (колец) полигона.
 */
void gfx_fill_compound_polygon(
    gfx_context_t *ctx,
    const gfx_point_t *points,
    uint16_t num_points,
    const uint32_t *parts,
    uint16_t num_parts
);

/**
 * @brief Шаблон штриховки для gfx_fill_compound_polygon_hatched().
 *
 * Шаблон привязан к координатам экрана. Рисуются только строки, где y кратно
 * row_period; в такой строке штрих длиной dash_len повторяется с шагом
 * dash_period, а начало штрихов сдвигается на (row * row_shift) % dash_period,
 * где row = y / row_period.
 *
 * Примеры:
 *   горизонтальные штрихи в шахматном порядке:
 *       {row_period 4, dash_len 4, dash_period 8, row_shift 4}
 *   диагональные линии под 45 градусов:
 *       {row_period 1, dash_len 1, dash_period 6, row_shift 1}
 */
typedef struct {
    uint8_t row_period;   /* Расстояние между строками штриховки, px (>= 1) */
    uint8_t dash_len;     /* Длина штриха, px (>= 1) */
    uint8_t dash_period;  /* Шаг штрихов в строке, px (>= dash_len) */
    uint8_t row_shift;    /* Сдвиг начала штрихов на каждую следующую строку */
} gfx_hatch_t;

/**
 * @brief Штриховка compound полигона (с holes) текущим цветом foreground.
 *
 * Геометрия, holes и отсечение те же, что у gfx_fill_compound_polygon();
 * вместо сплошной заливки рисуются только штрихи шаблона. Контур полигона не
 * рисуется. hatch == NULL ничего не рисует.
 */
void gfx_fill_compound_polygon_hatched(
    gfx_context_t *ctx,
    const gfx_point_t *points,
    uint16_t num_points,
    const uint32_t *parts,
    uint16_t num_parts,
    const gfx_hatch_t *hatch
);
/**
 * @brief Растровая плитка для gfx_fill_compound_polygon_tiled().
 *
 * Плитка w x h (w <= 8, h <= 16) повторяется по всей плоскости и привязана
 * к координатам экрана. rows[y] - строка плитки, бит x (младший бит - левый
 * пиксель) равен 1, если пиксель закрашивается.
 *
 * Пример - снежинки 5 x 10: в верхней половине снежинка (X из 5 пикселей
 * с полем в 1 px), в нижней такая же, сдвинутая на 2 px вправо (по кругу):
 *   rows = {0x00, 0x0A, 0x04, 0x0A, 0x00,
 *           0x00, 0x09, 0x10, 0x09, 0x00}
 */
typedef struct {
    uint8_t w;
    uint8_t h;
    uint8_t rows[16];
} gfx_tile_t;

/**
 * @brief Заливка compound полигона (с holes) растровой плиткой текущим
 *        цветом foreground; контур полигона не рисуется.
 *
 * Геометрия, holes и отсечение те же, что у gfx_fill_compound_polygon().
 * Плитки на границе полигона обрезаются. tile == NULL ничего не рисует.
 */
void gfx_fill_compound_polygon_tiled(
    gfx_context_t *ctx,
    const gfx_point_t *points,
    uint16_t num_points,
    const uint32_t *parts,
    uint16_t num_parts,
    const gfx_tile_t *tile
);

#endif /* GFX_POLYGON_H */
