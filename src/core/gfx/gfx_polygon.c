#include "purrgo/gfx_polygon.h"
#include "purrgo/gfx_line.h"
#include "purrgo/gfx_renderer.h"

#include <stdint.h>
#include <stddef.h>


/*
 * Рисует контур полигона.
 *
 * Последняя точка соединяется с первой, поэтому polygon
 * всегда получается замкнутым.
 */
void gfx_draw_polygon(
    gfx_context_t *ctx,
    const gfx_point_t *points,
    uint16_t count
) {
    if (!ctx || !points || count < 2) {
        return;
    }

    for (uint16_t i = 0; i < count - 1; i++) {
        gfx_draw_line(
            ctx,
            points[i].x,
            points[i].y,
            points[i + 1].x,
            points[i + 1].y
        );
    }

    /* Замыкаем полигон. */
    gfx_draw_line(
        ctx,
        points[count - 1].x,
        points[count - 1].y,
        points[0].x,
        points[0].y
    );
}


/*
 * Заполняет полигон текущим цветом foreground.
 * Поддерживает compound polygons (с holes).
 *
 * Используется scanline / even-odd алгоритм:
 *
 *   1. Для каждой строки framebuffer ищутся пересечения
 *      горизонтальной scanline с рёбрами всех колец полигона.
 *
 *   2. X-координаты пересечений сортируются.
 *
 *   3. Точки берутся попарно:
 *
 *          x0 -------- x1
 *                 x2 -------- x3
 *
 *      и соответствующие интервалы заполняются.
 *
 * Цвет заливки — ctx->color_fg.
 */
void gfx_fill_polygon(
    gfx_context_t *ctx,
    const gfx_point_t *points,
    uint16_t num_points,
    const uint32_t *parts,
    uint16_t num_parts
) {
    if (!ctx || !points || num_points < 3 || !parts || num_parts == 0) {
        return;
    }


    /*
     * Определяем вертикальный диапазон полигона.
     */
    int16_t min_y = points[0].y;
    int16_t max_y = points[0].y;

    for (uint16_t i = 1; i < num_points; i++) {
        if (points[i].y < min_y) {
            min_y = points[i].y;
        }

        if (points[i].y > max_y) {
            max_y = points[i].y;
        }
    }


    /*
     * Ограничиваем scanline диапазоном framebuffer.
     */
    if (min_y < 0) {
        min_y = 0;
    }

    if (max_y >= ctx->height) {
        max_y = ctx->height - 1;
    }


    /*
     * В отличие от старой реализации здесь НЕ происходит:
     *
     *     ctx->color_fg = ctx->color_bg;
     *
     * gfx_draw_hline() должен использовать настоящий текущий
     * foreground color.
     *
     * Поэтому polygon заполняется ctx->color_fg.
     */





    /*
     * Scanline rendering.
     */
    for (int16_t y = min_y; y <= max_y; y++) {
        int16_t nodeX[64];
        uint16_t nodes = 0;


        /*
         * Находим все пересечения текущей горизонтальной
         * scanline с рёбрами всех колец polygon.
         */
        for (uint16_t part_idx = 0; part_idx < num_parts; part_idx++) {
            uint16_t start = (uint16_t)parts[part_idx];
            uint16_t end = (part_idx + 1 < num_parts) ? (uint16_t)parts[part_idx + 1] : num_points;

            if (end - start < 3) {
                continue;
            }

            for (uint16_t i = start; i < end; i++) {
                uint16_t j = (i == end - 1) ? start : i + 1;

                int16_t y_start = points[i].y;
                int16_t y_end   = points[j].y;

                int16_t x_start = points[i].x;
                int16_t x_end   = points[j].x;


            /*
             * Горизонтальное ребро не создаёт отдельного
             * пересечения scanline.
             */
            if (y_start == y_end) {
                continue;
            }


            /*
             * Полуоткрытый интервал:
             *
             *     y_start <= y < y_end
             *
             * или
             *
             *     y_end <= y < y_start
             *
             * Это предотвращает двойной учёт вершины,
             * где сходятся два ребра.
             */
            if (
                (y_start <= y && y_end > y) ||
                (y_end <= y && y_start > y)
            ) {
                /*
                 * Вычисляем X пересечения.
                 *
                 * Используется int32_t для промежуточного
                 * умножения, чтобы не выполнять арифметику
                 * непосредственно в int16_t.
                 */
                int32_t x =
                    (int32_t)x_start +
                    (
                        (int32_t)(x_end - x_start) *
                        (int32_t)(y - y_start)
                    ) /
                    (int32_t)(y_end - y_start);


                /*
                 * Теоретически nodes не может превысить count:
                 * одно ребро даёт максимум одно пересечение
                 * с конкретной scanline.
                 */
                if (nodes < 64) {
                    nodeX[nodes++] = (int16_t)x;
                }
            }
            }
        }


        /*
         * Если пересечений меньше двух, заполнять нечего.
         */
        if (nodes < 2) {
            continue;
        }


        /*
         * Сортируем X-координаты пересечений.
         *
         * Для типичных landuse polygon количество рёбер
         * относительно небольшое, поэтому простая сортировка
         * здесь достаточна.
         */
        for (uint16_t i = 0; i < nodes; i++) {
            for (
                uint16_t j = 0;
                j + 1 < nodes - i;
                j++
            ) {
                if (nodeX[j] > nodeX[j + 1]) {
                    int16_t tmp = nodeX[j];

                    nodeX[j] =
                        nodeX[j + 1];

                    nodeX[j + 1] =
                        tmp;
                }
            }
        }


        /*
         * Заполняем пары пересечений:
         *
         *     [nodeX[0], nodeX[1]]
         *     [nodeX[2], nodeX[3]]
         *     ...
         *
         * gfx_draw_hline() использует ctx->color_fg,
         * то есть цвет, установленный вызывающим кодом.
         */
        for (
            uint16_t i = 0;
            i + 1 < nodes;
            i += 2
        ) {
            gfx_draw_hline(
                ctx,
                nodeX[i],
                nodeX[i + 1],
                y
            );
        }
    }


}