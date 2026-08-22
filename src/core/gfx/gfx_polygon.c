#include "purrgo/gfx_polygon.h"
#include "purrgo/gfx_line.h"
#include "purrgo/gfx_renderer.h"

#include <stdint.h>
#include <stddef.h>

/*
 * Максимальное количество пересечений одной scanline
 * с рёбрами polygon.
 *
 * nodeX[] хранит int16_t, поэтому буфер занимает:
 *
 *     64 * 2 = 128 байт
 *
 * Это приемлемо для STM32 и не требует динамического выделения памяти.
 */
#define GFX_MAX_POLYGON_NODES 64


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
 * Заполняет обычный polygon.
 *
 * Эта функция сохраняет старый API и является wrapper'ом
 * над compound polygon renderer.
 *
 * Для обычного polygon имеется только одно кольцо,
 * начинающееся с точки 0.
 */
void gfx_fill_polygon(
    gfx_context_t *ctx,
    const gfx_point_t *points,
    uint16_t count
) {
    if (!ctx || !points || count < 3) {
        return;
    }

    uint32_t parts[1] = {0};

    gfx_fill_compound_polygon(
        ctx,
        points,
        count,
        parts,
        1
    );
}


/*
 * Заполняет compound polygon текущим цветом foreground.
 *
 * Поддерживает несколько колец:
 *
 *     outer ring
 *     hole
 *     hole
 *     ...
 *
 * Используется scanline / even-odd алгоритм:
 *
 *   1. Для каждой строки framebuffer ищутся пересечения
 *      горизонтальной scanline с рёбрами всех колец.
 *
 *   2. X-координаты пересечений сортируются.
 *
 *   3. Пересечения берутся попарно:
 *
 *          x0 -------- x1
 *                 x2 -------- x3
 *
 *      и соответствующие интервалы заполняются.
 *
 * Благодаря even-odd правилу направление обхода колец
 * (CW / CCW) не влияет на наличие hole.
 *
 * ВАЖНО:
 * nodeX имеет фиксированный размер.
 * Если одна scanline требует более
 * GFX_MAX_POLYGON_NODES пересечений, функция прекращает
 * дальнейшую отрисовку polygon, чтобы гарантированно
 * не допустить выхода за границы массива.
 */
void gfx_fill_compound_polygon(
    gfx_context_t *ctx,
    const gfx_point_t *points,
    uint16_t num_points,
    const uint32_t *parts,
    uint16_t num_parts
) {
    if (
        !ctx ||
        !points ||
        num_points < 3 ||
        !parts ||
        num_parts == 0
    ) {
        return;
    }


    /*
     * Проверяем корректность массива parts[].
     *
     * parts[] содержит начальные индексы колец:
     *
     *     parts = {0, 100, 150}
     *
     * означает:
     *
     *     ring 0 = [0,   100)
     *     ring 1 = [100, 150)
     *     ring 2 = [150, num_points)
     */
    if (parts[0] != 0) {
        return;
    }

    for (uint16_t part_idx = 0; part_idx < num_parts; part_idx++) {
        uint32_t start =
            parts[part_idx];

        uint32_t end =
            (
                part_idx + 1 < num_parts
            )
                ? parts[part_idx + 1]
                : (uint32_t)num_points;


        /*
         * Кольцо должно содержать хотя бы одну точку,
         * а его конец не должен выходить за массив points[].
         */
        if (
            start >= end ||
            end > (uint32_t)num_points
        ) {
            return;
        }


        /*
         * parts[] должен быть строго возрастающим.
         */
        if (
            part_idx > 0 &&
            start <= parts[part_idx - 1]
        ) {
            return;
        }
    }


    /*
     * Определяем вертикальный диапазон всей compound geometry.
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
     * Если geometry полностью находится выше или ниже
     * framebuffer, рисовать нечего.
     *
     * Это также защищает от случая, когда после clipping
     * диапазон scanline становится пустым.
     */
    if (max_y < 0 || min_y >= ctx->height) {
        return;
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
     * После clipping диапазон может оказаться пустым.
     */
    if (min_y > max_y) {
        return;
    }


    /*
     * Scanline rendering.
     */
    for (int16_t y = min_y; y <= max_y; y++) {

        /*
         * Массив пересечений находится на стеке.
         *
         * Размер фиксирован и известен во время компиляции.
         * malloc() в graphics/render path отсутствует.
         */
        int16_t nodeX[GFX_MAX_POLYGON_NODES];

        uint16_t nodes = 0;


        /*
         * Обрабатываем каждое кольцо отдельно.
         */
        for (
            uint16_t part_idx = 0;
            part_idx < num_parts;
            part_idx++
        ) {
            /*
             * Эти значения уже проверены выше.
             */
            uint16_t start =
                (uint16_t)parts[part_idx];

            uint16_t end =
                (
                    part_idx + 1 < num_parts
                )
                    ? (uint16_t)parts[part_idx + 1]
                    : num_points;


            /*
             * Теоретически эта проверка уже обеспечена
             * валидацией parts[].
             *
             * Оставляем её как локальную защиту перед
             * вычислением i + 1 и замыканием кольца.
             */
            if (end - start < 3) {
                continue;
            }


            /*
             * Обрабатываем рёбра текущего кольца.
             */
            for (uint16_t i = start; i < end; i++) {

                uint16_t j =
                    (i == end - 1)
                        ? start
                        : (uint16_t)(i + 1);


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
                 * Используем полуоткрытый интервал:
                 *
                 *     y_start <= y < y_end
                 *
                 * или
                 *
                 *     y_end <= y < y_start
                 *
                 * Это предотвращает двойной учёт вершины,
                 * в которой сходятся два ребра.
                 */
                if (
                    !(
                        (y_start <= y && y_end > y) ||
                        (y_end <= y && y_start > y)
                    )
                ) {
                    continue;
                }


                /*
                 * Вычисляем X пересечения.
                 *
                 * Все промежуточные операции выполняются
                 * в int32_t.
                 *
                 * Это важно, поскольку координаты точек имеют
                 * меньший тип, а произведение:
                 *
                 *     (x_end - x_start) * (y - y_start)
                 *
                 * не должно вычисляться в int16_t.
                 */
                int32_t x =
                    (int32_t)x_start +
                    (
                        (int32_t)(x_end - x_start) *
                        (int32_t)(y - y_start)
                    ) /
                    (int32_t)(y_end - y_start);


                /*
                 * Проверяем границу массива ДО записи.
                 *
                 * При превышении лимита немедленно прекращаем
                 * обработку polygon.
                 *
                 * Это fail-fast поведение:
                 *
                 *   - нет buffer overflow;
                 *   - нет повреждения stack;
                 *   - нет тихого удаления пересечений;
                 *
                 * но polygon может быть отрисован частично,
                 * поскольку предыдущие scanline уже могли быть
                 * переданы в renderer.
                 *
                 * Это ограничение будет отдельно устранено,
                 * если реальная map geometry потребует >64
                 * пересечений на одной scanline.
                 */
                if (nodes >= GFX_MAX_POLYGON_NODES) {
                    return;
                }

                nodeX[nodes++] = (int16_t)x;
            }
        }


        /*
         * Меньше двух пересечений означает, что внутри
         * polygon на данной scanline нет заполняемого
         * интервала.
         */
        if (nodes < 2) {
            continue;
        }


        /*
         * Сортируем X-координаты пересечений.
         *
         * Для типичных map polygons количество пересечений
         * небольшое, поэтому простая bubble sort здесь
         * достаточно предсказуема и не требует дополнительной
         * памяти.
         */
        for (uint16_t i = 0; i < nodes; i++) {
            for (
                uint16_t j = 0;
                j + 1 < nodes - i;
                j++
            ) {
                if (nodeX[j] > nodeX[j + 1]) {
                    int16_t tmp =
                        nodeX[j];

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
         * gfx_draw_hline() использует ctx->color_fg.
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