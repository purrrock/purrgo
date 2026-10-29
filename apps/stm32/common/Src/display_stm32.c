#include "display_stm32.h"

#include "purrgo/display_hal.h"
#include "purrgo/logger.h"
#include "purrgo/gfx_fb2bpp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Аппаратный display backend: E-Ink Waveshare 2.7" e-Paper V2.
 *
 * Отладочный TFT ST7789 удалён из проекта.
 */
#if PURRGO_HW_DISPLAY_BACKEND != PURRGO_DISPLAY_BACKEND_EINK
#error "Unsupported PurrGO display backend"
#endif

#include "display_eink.h"


/*
 * Размер framebuffer:
 *
 *   WIDTH * HEIGHT * BPP / 8
 *
 * Для текущего дисплея:
 *
 *   176 * 264 * 2 / 8 = 11616 байт
 */
#define DISPLAY_FB_SIZE \
    ((DISPLAY_WIDTH * DISPLAY_HEIGHT * DISPLAY_BPP) / 8)

static uint8_t framebuffer[DISPLAY_FB_SIZE];


/*
 * --------------------------------------------------------------------------
 * Framebuffer operations
 * --------------------------------------------------------------------------
 */

void display_init(void)
{
    /*
     * Сначала создаём корректное начальное содержимое framebuffer.
     *
     * Аппаратная инициализация backend выполняется ниже.
     */
    display_clear(COLOR_WHITE);

    display_eink_init(framebuffer);
}

void display_set_mode(display_mode_t mode)
{
    display_eink_set_mode(mode);
}

void display_stm32_poll(void)
{
    display_eink_poll();
}

bool display_stm32_ready_for_render(void)
{
    return !display_eink_data_busy();
}

/*
 * Перевод дисплея в режим пониженного энергопотребления.
 */
void display_sleep(void)
{
    display_eink_sleep();
}


void display_clear(uint8_t color)
{
    gfx_fb2bpp_clear(framebuffer, DISPLAY_WIDTH, DISPLAY_HEIGHT, color);
}


/*
 * Раскладка пикселей (4 пикселя на байт, первый - в старших битах) описана
 * в purrgo/gfx_fb2bpp.h; координаты вне экрана игнорируются.
 */
void display_set_pixel(
    int16_t x,
    int16_t y,
    uint8_t color
)
{
    gfx_fb2bpp_set_pixel(framebuffer, DISPLAY_WIDTH, DISPLAY_HEIGHT, x, y, color);
}


uint8_t display_get_pixel(
    int16_t x,
    int16_t y
)
{
    /* За границами дисплея - COLOR_BLACK (0). */
    return gfx_fb2bpp_get_pixel(framebuffer, DISPLAY_WIDTH, DISPLAY_HEIGHT, x, y);
}


const uint8_t *display_get_framebuffer(void)
{
    return framebuffer;
}


/*
 * --------------------------------------------------------------------------
 * GFX -> display callbacks
 * --------------------------------------------------------------------------
 */

void stm32_draw_pixel_cb(
    void *fb,
    int16_t x,
    int16_t y,
    gfx_color_t color
)
{
    /*
     * framebuffer передаётся через общий GFX API,
     * но фактически framebuffer принадлежит
     * display_stm32.c.
     */
    (void)fb;

    display_set_pixel(
        x,
        y,
        (uint8_t)color
    );
}


gfx_color_t stm32_read_pixel_cb(
    void *fb,
    int16_t x,
    int16_t y
)
{
    (void)fb;

    return (gfx_color_t)display_get_pixel(x, y);
}


void stm32_clear_cb(
    void *fb,
    gfx_color_t color
)
{
    (void)fb;

    display_clear((uint8_t)color);
}


/*
 * Быстрая заливка горизонтального отрезка: целые байты - одним memset(),
 * раскладка пикселей та же, что у display_set_pixel() (см. gfx_fb2bpp.h).
 */
void stm32_fill_span_cb(
    void *fb,
    int16_t x0,
    int16_t x1,
    int16_t y,
    gfx_color_t color
)
{
    (void)fb;

    gfx_fb2bpp_fill_span(
        framebuffer,
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT,
        x0,
        x1,
        y,
        (uint8_t)color
    );
}


/*
 * --------------------------------------------------------------------------
 * Refresh management
 * --------------------------------------------------------------------------
 *
 * Все обновления неблокирующие и только ставят запрос в очередь
 * display_eink.c; физическое обновление идёт в фоне (display_stm32_poll()
 * вызывается из главного цикла). Области объединяет сам display_eink.c, поэтому
 * здесь они не накапливаются: между display_refresh_region() и display_flush()
 * poll() не вызывается (отрисовка кадра идёт одним проходом главного цикла),
 * значит запрос уходит в панель не раньше конца отрисовки.
 *
 * В режиме 4 gray обновляются пиксели, изменившиеся относительно предыдущего
 * кадра, в режиме 1 bit - накопленная область.
 */

/*
 * Полное обновление дисплея (мигает весь экран, сбрасывает остаточное
 * изображение). Не блокирует.
 */
void display_refresh(void)
{
    display_eink_request_full();
}


/*
 * Помечает область как изменённую (с обрезкой по границам дисплея).
 * Реального обновления дисплея здесь не происходит.
 */
void display_refresh_region(
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h
)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISPLAY_WIDTH)  { w = DISPLAY_WIDTH - x; }
    if (y + h > DISPLAY_HEIGHT) { h = DISPLAY_HEIGHT - y; }

    if (w <= 0 || h <= 0)
    {
        return;
    }

    /*
     * Переход на полное обновление (первое обновление, смена режима, лимит
     * частичных подряд) выполняет display_eink.c.
     */
    display_eink_request_region(x, y, w, h);
}


/*
 * Контракт display_hal.h сохранён, но запрос уже поставлен в
 * display_refresh_region(): здесь делать нечего.
 */
void display_flush(void)
{
}
