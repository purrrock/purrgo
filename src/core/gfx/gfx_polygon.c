#include "purrgo/gfx_polygon.h"
#include "purrgo/gfx_line.h"
#include "purrgo/gfx_renderer.h"

#include <stdint.h>
#include <stddef.h>

/*
 * Размер буфера пересечений одной scanline (быстрый путь).
 *
 * nodeX[] хранит int16_t: 128 * 2 = 256 байт стека, без динамической памяти.
 * Если пересечений больше, scanline заливается запасным путём
 * (fill_scanline_streaming), который не ограничен этим размером.
 */
#define GFX_MAX_POLYGON_NODES 128

/*
 * Размер порции запасного пути: 64 * 4 = 256 байт стека.
 */
#define GFX_STREAM_BATCH 64

/*
 * Порог "широких" координат. Пока все вершины внутри +-GFX_NARROW_LIMIT,
 * (x_end - x_start) * (y - y_start) помещается в int32 (не больше 2^15 * 2^15 * 2),
 * иначе произведение считается в int64.
 */
#define GFX_NARROW_LIMIT 16384


static int16_t clamp_i16(int32_t v)
{
    if (v < INT16_MIN) {
        return INT16_MIN;
    }
    if (v > INT16_MAX) {
        return INT16_MAX;
    }
    return (int16_t)v;
}


/*
 * Рисует контур полигона.
 *
 * Последняя точка соединяется с первой, поэтому polygon
 * всегда получается замкнутым.
 *
 * Контур рассчитан на вершины в пределах int16 (маркер, рамки): gfx_draw_line()
 * принимает int16, поэтому большие координаты насыщаются.
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
            clamp_i16(points[i].x),
            clamp_i16(points[i].y),
            clamp_i16(points[i + 1].x),
            clamp_i16(points[i + 1].y)
        );
    }

    /* Замыкаем полигон. */
    gfx_draw_line(
        ctx,
        clamp_i16(points[count - 1].x),
        clamp_i16(points[count - 1].y),
        clamp_i16(points[0].x),
        clamp_i16(points[0].y)
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
 * Пересечение ребра (i -> j) со scanline y.
 *
 * Используется полуоткрытый интервал:
 *
 *     y_start <= y < y_end   или   y_end <= y < y_start
 *
 * Это предотвращает двойной учёт вершины, в которой сходятся два ребра.
 * Горизонтальное ребро пересечения не создаёт.
 *
 * X пересечения усекается делением к нулю и насыщается в int16: порядок
 * пересечений при этом сохраняется, а заливаемый интервал всё равно отсекается
 * по clip.
 */
static bool edge_crossing(
    const gfx_point_t *points,
    uint16_t i,
    uint16_t j,
    int32_t y,
    bool wide,
    int16_t *out_x
) {
    int32_t y_start = points[i].y;
    int32_t y_end   = points[j].y;

    if (y_start == y_end) {
        return false;
    }

    if (
        !(
            (y_start <= y && y_end > y) ||
            (y_end <= y && y_start > y)
        )
    ) {
        return false;
    }

    int32_t x_start = points[i].x;
    int32_t x_end   = points[j].x;
    int32_t x;

    if (wide) {
        x = x_start + (int32_t)(
            ((int64_t)(x_end - x_start) * (int64_t)(y - y_start)) /
            (int64_t)(y_end - y_start)
        );
    } else {
        x = x_start +
            ((x_end - x_start) * (y - y_start)) /
            (y_end - y_start);
    }

    *out_x = clamp_i16(x);
    return true;
}


/*
 * Границы кольца ring_idx в массиве points[]. false - кольцо вырожденное
 * (меньше 3 точек), его рёбра пропускаются.
 */
static bool ring_bounds(
    const uint32_t *parts,
    uint16_t num_parts,
    uint16_t num_points,
    uint16_t ring_idx,
    uint16_t *start,
    uint16_t *end
) {
    *start = (uint16_t)parts[ring_idx];
    *end = (ring_idx + 1 < num_parts)
        ? (uint16_t)parts[ring_idx + 1]
        : num_points;

    return (uint16_t)(*end - *start) >= 3;
}


/*
 * Быстрый путь: собирает все пересечения scanline в nodeX[].
 * Возвращает false, если пересечений больше capacity (nodeX содержит
 * неполный набор и использовать его нельзя).
 */
static bool collect_nodes(
    const gfx_point_t *points,
    uint16_t num_points,
    const uint32_t *parts,
    uint16_t num_parts,
    int32_t y,
    bool wide,
    int16_t *nodeX,
    uint16_t capacity,
    uint16_t *out_nodes
) {
    uint16_t nodes = 0;

    for (uint16_t part_idx = 0; part_idx < num_parts; part_idx++) {
        uint16_t start;
        uint16_t end;

        if (!ring_bounds(parts, num_parts, num_points, part_idx, &start, &end)) {
            continue;
        }

        for (uint16_t i = start; i < end; i++) {
            uint16_t j = (i == end - 1) ? start : (uint16_t)(i + 1);
            int16_t x;

            if (!edge_crossing(points, i, j, y, wide, &x)) {
                continue;
            }

            if (nodes >= capacity) {
                return false;
            }

            nodeX[nodes++] = x;
        }
    }

    *out_nodes = nodes;
    return true;
}


/*
 * Запасной путь для scanline с очень большим числом пересечений.
 *
 * Пересечения упорядочиваются без массива на все пересечения: каждый проход по
 * рёбрам выбирает GFX_STREAM_BATCH наименьших ключей среди ещё не обработанных.
 * Ключ = (x + 32768) << 16 | индекс ребра, поэтому ключи уникальны и порядок
 * полностью определён даже при совпадающих x. Заливка идёт по чётно-нечётному
 * правилу с сохранением состояния между порциями. Число проходов не больше
 * ceil(рёбер / GFX_STREAM_BATCH).
 */
static void fill_scanline_streaming(
    gfx_context_t *ctx,
    const gfx_point_t *points,
    uint16_t num_points,
    const uint32_t *parts,
    uint16_t num_parts,
    int32_t y,
    bool wide
) {
    uint32_t batch[GFX_STREAM_BATCH];
    uint32_t after = 0;
    bool have_after = false;
    bool inside = false;
    int16_t open_x = 0;

    for (;;) {
        uint16_t n = 0;

        for (uint16_t part_idx = 0; part_idx < num_parts; part_idx++) {
            uint16_t start;
            uint16_t end;

            if (!ring_bounds(parts, num_parts, num_points, part_idx, &start, &end)) {
                continue;
            }

            for (uint16_t i = start; i < end; i++) {
                uint16_t j = (i == end - 1) ? start : (uint16_t)(i + 1);
                int16_t x;

                if (!edge_crossing(points, i, j, y, wide, &x)) {
                    continue;
                }

                uint32_t key =
                    ((uint32_t)((int32_t)x + 32768) << 16) | (uint32_t)i;

                if (have_after && key <= after) {
                    continue;
                }

                if (n == GFX_STREAM_BATCH) {
                    if (key >= batch[n - 1]) {
                        continue;
                    }
                    n--;    /* вытесняем наибольший ключ порции */
                }

                uint16_t pos = n;

                while (pos > 0 && batch[pos - 1] > key) {
                    batch[pos] = batch[pos - 1];
                    pos--;
                }
                batch[pos] = key;
                n++;
            }
        }

        if (n == 0) {
            return;
        }

        for (uint16_t k = 0; k < n; k++) {
            int16_t x = (int16_t)((int32_t)(batch[k] >> 16) - 32768);

            if (!inside) {
                open_x = x;
                inside = true;
            } else {
                gfx_draw_hline(ctx, open_x, x, (int16_t)y);
                inside = false;
            }
        }

        after = batch[n - 1];
        have_after = true;

        if (n < GFX_STREAM_BATCH) {
            return;
        }
    }
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
 * Вершины - int32 (до +-2^29): координаты далёких вершин не насыщаются, наклон
 * рёбер сохраняется. Если на одной scanline больше GFX_MAX_POLYGON_NODES
 * пересечений, строка заливается запасным путём - полигон никогда не
 * обрывается на середине.
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
     * Вертикальный диапазон всей compound geometry и признак "широких"
     * координат (см. GFX_NARROW_LIMIT).
     */
    int32_t min_y = points[0].y;
    int32_t max_y = min_y;
    bool wide = false;

    for (uint16_t i = 0; i < num_points; i++) {
        if (points[i].y < min_y) {
            min_y = points[i].y;
        }

        if (points[i].y > max_y) {
            max_y = points[i].y;
        }

        if (
            points[i].x < -GFX_NARROW_LIMIT || points[i].x > GFX_NARROW_LIMIT ||
            points[i].y < -GFX_NARROW_LIMIT || points[i].y > GFX_NARROW_LIMIT
        ) {
            wide = true;
        }
    }


    /*
     * Если geometry полностью находится выше или ниже
     * framebuffer, рисовать нечего.
     */
    int32_t clip_top = ctx->clip_y;
    int32_t clip_bottom = (int32_t)ctx->clip_y + (int32_t)ctx->clip_h - 1;

    if (max_y < clip_top || min_y > clip_bottom) {
        return;
    }


    /*
     * Ограничиваем scanline диапазоном framebuffer.
     */
    if (min_y < clip_top) {
        min_y = clip_top;
    }

    if (max_y > clip_bottom) {
        max_y = clip_bottom;
    }


    /*
     * Scanline rendering.
     */
    for (int32_t y = min_y; y <= max_y; y++) {

        /*
         * Массив пересечений находится на стеке.
         * malloc() в graphics/render path отсутствует.
         */
        int16_t nodeX[GFX_MAX_POLYGON_NODES];

        uint16_t nodes = 0;

        if (!collect_nodes(
                points,
                num_points,
                parts,
                num_parts,
                y,
                wide,
                nodeX,
                GFX_MAX_POLYGON_NODES,
                &nodes)) {

            fill_scanline_streaming(
                ctx,
                points,
                num_points,
                parts,
                num_parts,
                y,
                wide
            );
            continue;
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
         * Сортируем X-координаты пересечений (insertion sort, in-place).
         */
        for (uint16_t i = 1; i < nodes; i++) {
            int16_t key = nodeX[i];
            int16_t j = (int16_t)(i - 1);

            while (j >= 0 && nodeX[j] > key) {
                nodeX[j + 1] = nodeX[j];
                j = (int16_t)(j - 1);
            }
            nodeX[j + 1] = key;
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
                (int16_t)y
            );
        }
    }
}
