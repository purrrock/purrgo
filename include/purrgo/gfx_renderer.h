#ifndef GFX_RENDERER_H
#define GFX_RENDERER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Характеристики целевого дисплея должны браться из hardware_config.h.
 * - Layout PurrGo:
 * - На экране APP_STATE_MAP (карта) карта занимает центральную область.
 * - Сверху и снизу находятся служебные status regions (строки состояния).
 * - Размеры и координаты status regions не должны хардкодиться в map.c.
 * - Map renderer НЕ должен владеть всем framebuffer'ом,
 *   projection/rendering карты должны работать относительно отдельного map viewport
 *   (передаваться через размеры ctx или заданные границы).
 *
 * Цветовая абстракция.
 * Для 2-bit E-Ink: 0 (Black), 1 (Dark Gray), 2 (Light Gray), 3 (White).
 */
typedef uint8_t gfx_color_t;

#define BLACK      0
#define DARK_GRAY  1
#define LIGHT_GRAY 2
#define WHITE      3

/*
 * Базовая структура точки.
 *
 * Знаковый int16_t позволяет координатам уходить за границы экрана (clipping),
 * при этом занимая всего 4 байта на структуру.
 */
typedef struct {
    int16_t x;
    int16_t y;
} gfx_point_t;

/*
 * Callback для записи одного пикселя.
 *
 * Платформенно-зависимый код должен реализовать эту функцию,
 * выполняя необходимые битовые операции для framebuffer.
 */
typedef void (*gfx_draw_pixel_fn)(
    void *fb,
    int16_t x,
    int16_t y,
    gfx_color_t color
);

/*
 * Callback для чтения одного пикселя.
 */
typedef gfx_color_t (*gfx_read_pixel_fn)(
    void *fb,
    int16_t x,
    int16_t y
);

/*
 * Callback для быстрого заполнения всего framebuffer.
 *
 * В отличие от draw_pixel() этот callback должен заполнить
 * весь framebuffer непосредственно платформенным способом.
 *
 * Например, для STM32 2-bpp framebuffer это может быть memset()
 * одного байта во весь буфер.
 *
 * Callback является необязательным. Если он не установлен,
 * gfx_clear() использует универсальный пиксельный fallback.
 */
typedef void (*gfx_clear_fn)(
    void *fb,
    gfx_color_t color
);

/*
 * Контекст графического ядра.
 */
typedef struct {
    int16_t width;              /* Ширина экрана в пикселях */
    int16_t height;             /* Высота экрана в пикселях */

    void *framebuffer;          /* Opaque-указатель на framebuffer платформы */

    gfx_draw_pixel_fn draw_pixel; /* Запись одного пикселя */
    gfx_read_pixel_fn read_pixel; /* Чтение одного пикселя */

    /*
     * Необязательный быстрый callback очистки framebuffer.
     *
     * Если NULL, gfx_clear() использует универсальный
     * pixel-by-pixel fallback.
     */
    gfx_clear_fn clear;

    gfx_color_t color_fg;       /* Текущий цвет переднего плана */
    gfx_color_t color_bg;       /* Текущий цвет фона */

    /*
     * Clipping region.
     *
     * x >= clip_x && x < clip_x + clip_w
     * y >= clip_y && y < clip_y + clip_h
     */
    int16_t clip_x;
    int16_t clip_y;
    int16_t clip_w;
    int16_t clip_h;
} gfx_context_t;

/*
 * Инициализация контекста.
 *
 * Возвращает false при передаче нулевых указателей.
 *
 * Callback быстрого clear здесь намеренно не передаётся:
 * это сохраняет существующий API gfx_init() и не требует
 * менять все существующие вызовы в тестах и эмуляторе.
 */
bool gfx_init(
    gfx_context_t *ctx,
    int16_t width,
    int16_t height,
    void *framebuffer,
    gfx_draw_pixel_fn draw_pixel_cb,
    gfx_read_pixel_fn read_pixel_cb
);

/*
 * Установка платформенного callback быстрого заполнения framebuffer.
 *
 * Передача NULL отключает быстрый путь и возвращает gfx_clear()
 * к универсальной реализации через draw_pixel.
 */
void gfx_set_clear_callback(
    gfx_context_t *ctx,
    gfx_clear_fn clear_cb
);

/*
 * Установка области отсечения (clipping).
 */
void gfx_set_clip(
    gfx_context_t *ctx,
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h
);

/*
 * Сброс области отсечения на весь физический экран.
 */
void gfx_reset_clip(gfx_context_t *ctx);

/*
 * Установка текущих цветов контекста.
 */
void gfx_set_color(
    gfx_context_t *ctx,
    gfx_color_t fg,
    gfx_color_t bg
);

/*
 * Получение текущих цветов контекста.
 */
void gfx_get_color(
    const gfx_context_t *ctx,
    gfx_color_t *fg,
    gfx_color_t *bg
);

/*
 * Базовый примитив: отрисовка точки с проверкой clipping.
 */
void gfx_draw_pixel(
    gfx_context_t *ctx,
    int16_t x,
    int16_t y
);

/*
 * Чтение пикселя из framebuffer с проверкой clipping.
 */
gfx_color_t gfx_read_pixel(
    gfx_context_t *ctx,
    int16_t x,
    int16_t y
);

/*
 * Очистка всего framebuffer текущим цветом фона (color_bg).
 *
 * Если установлен платформенный clear callback, используется он.
 * Иначе выполняется универсальная pixel-by-pixel реализация.
 */
void gfx_clear(gfx_context_t *ctx);

/*
 * Отрисовка горизонтальной линии.
 */
void gfx_draw_hline(
    gfx_context_t *ctx,
    int16_t x_start,
    int16_t x_end,
    int16_t y
);

/*
 * Отрисовка вертикальной линии.
 */
void gfx_draw_vline(
    gfx_context_t *ctx,
    int16_t x,
    int16_t y_start,
    int16_t y_end
);

/**
 * @brief Отрисовка точечной линии
 *        (чередование 1 пиксель линии, 1 пиксель пропуска).
 */
void gfx_draw_dotted_line(
    gfx_context_t *ctx,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1
);

/**
 * @brief Отрисовка железнодорожной линии.
 */
void gfx_draw_railway_line(
    gfx_context_t *ctx,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1
);

#endif /* GFX_RENDERER_H */