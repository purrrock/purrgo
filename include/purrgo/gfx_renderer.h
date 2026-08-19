#ifndef GFX_RENDERER_H
#define GFX_RENDERER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Характеристики целевого дисплея и Layout PurrGo:
 * - Портретная ориентация (Portrait orientation).
 * - Физическое разрешение: 296x128 (В x Ш).
 * - На экране APP_STATE_MAP (карта) карта занимает центральную область.
 * - Сверху и снизу находятся служебные status regions (строки состояния).
 * - Размеры и координаты status regions не должны хардкодиться в map.c.
 * - Map renderer НЕ должен владеть всем framebuffer'ом,
 *   projection/rendering карты должны работать относительно отдельного map viewport
 *   (передаваться через размеры ctx или заданные границы).
 *
 * Цветовая абстракция.
 * Для 1-bit: 0 (Black), 1 (White).
 * Для 2-bit E-Ink: 0 (Black), 1 (Dark Gray), 2 (Light Gray), 3 (White).
 */
typedef uint8_t gfx_color_t;

/* * Базовая структура точки. 
 * Знаковый int16_t позволяет координатам уходить за границы экрана (clipping),
 * при этом занимая всего 4 байта на структуру.
 */
typedef struct {
    int16_t x;
    int16_t y;
} gfx_point_t;

/*
 * Сигнатура callback-функции для отрисовки пикселя.
 * Платформенно-зависимый код должен реализовать эту функцию,
 * выполняя необходимые битовые операции (read-modify-write) для Framebuffer.
 */
typedef void (*gfx_draw_pixel_fn)(void *fb, int16_t x, int16_t y, gfx_color_t color);

/*
 * Контекст графического ядра.
 * Передается по указателю во все функции отрисовки.
 */
typedef struct {
    int16_t width;              /* Ширина экрана в пикселях */
    int16_t height;             /* Высота экрана в пикселях */
    
    void *framebuffer;          /* Opaque-указатель на массив пикселей платформы */
    gfx_draw_pixel_fn draw_pixel; /* Указатель на платформенно-зависимую функцию вывода */
    
    gfx_color_t color_fg;       /* Текущий цвет переднего плана (линии, текст, контуры) */
    gfx_color_t color_bg;       /* Текущий цвет фона (заливка, очистка экрана) */
} gfx_context_t;

/*
 * Инициализация контекста.
 * Возвращает false при передаче нулевых указателей.
 */
bool gfx_init(gfx_context_t *ctx, 
              int16_t width, 
              int16_t height, 
              void *framebuffer, 
              gfx_draw_pixel_fn draw_pixel_cb);

/*
 * Установка текущих цветов контекста.
 */
void gfx_set_color(gfx_context_t *ctx, gfx_color_t fg, gfx_color_t bg);

/*
 * Базовый примитив: отрисовка точки с проверкой границ (clipping).
 * Inline-функция может быть перенесена в .c, если требуется строгая инкапсуляция,
 * но здесь оставлена как прототип для вызова внутри gfx_line.c / gfx_polygon.c
 */
void gfx_draw_pixel(gfx_context_t *ctx, int16_t x, int16_t y);

/*
 * Очистка всего Framebuffer текущим цветом фона (color_bg).
 * Зависит от аппаратной реализации, может потребовать платформенного callback'а
 * для быстрой очистки через memset (если абстракция draw_pixel слишком медленная).
 */
void gfx_clear(gfx_context_t *ctx);

#endif /* GFX_RENDERER_H */