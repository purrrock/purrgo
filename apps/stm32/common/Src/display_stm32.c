#include "display_stm32.h"

#include "purrgo/display_hal.h"
#include "purrgo/logger.h"
#include "purrgo/gfx_fb2bpp.h"

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

    display_eink_init();
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
    /*
     * Используем только младшие два бита цвета.
     */
    color &= 0x03;

    /*
     * В одном байте находятся четыре 2-битных пикселя.
     *
     * Поэтому один и тот же цвет записывается
     * во все четыре позиции:
     *
     *   [color][color][color][color]
     */
    uint8_t byte_value =
        (uint8_t)(
            (color << 6) |
            (color << 4) |
            (color << 2) |
            color
        );

    memset(
        framebuffer,
        byte_value,
        DISPLAY_FB_SIZE
    );
}


void display_set_pixel(
    int16_t x,
    int16_t y,
    uint8_t color
)
{
    /*
     * Проверяем координаты до выполнения арифметики
     * индекса framebuffer.
     */
    if (x < 0 ||
        x >= DISPLAY_WIDTH ||
        y < 0 ||
        y >= DISPLAY_HEIGHT)
    {
        return;
    }

    /*
     * Используются только четыре логических цвета.
     */
    color &= 0x03;

    /*
     * Каждый байт содержит четыре пикселя по 2 бита.
     *
     * pixel_index:
     *   линейный номер пикселя.
     *
     * byte_index:
     *   номер байта framebuffer.
     *
     * bit_shift:
     *   положение двух бит нужного пикселя.
     *
     * Для первого пикселя:
     *   bit_shift = 6
     *
     * Для второго:
     *   bit_shift = 4
     *
     * Для третьего:
     *   bit_shift = 2
     *
     * Для четвёртого:
     *   bit_shift = 0
     */
    int pixel_index =
        y * DISPLAY_WIDTH + x;

    int byte_index =
        pixel_index >> 2;

    int bit_shift =
        (3 - (pixel_index & 3)) << 1;

    /*
     * Сначала удаляем старое значение пикселя.
     */
    framebuffer[byte_index] &=
        (uint8_t)~(0x03 << bit_shift);

    /*
     * Затем записываем новое значение.
     */
    framebuffer[byte_index] |=
        (uint8_t)(color << bit_shift);
}


uint8_t display_get_pixel(
    int16_t x,
    int16_t y
)
{
    /*
     * Выход за границы дисплея.
     */
    if (x < 0 ||
        x >= DISPLAY_WIDTH ||
        y < 0 ||
        y >= DISPLAY_HEIGHT)
    {
        return COLOR_BLACK;
    }

    int pixel_index =
        y * DISPLAY_WIDTH + x;

    int byte_index =
        pixel_index >> 2;

    int bit_shift =
        (3 - (pixel_index & 3)) << 1;

    return
        (uint8_t)(
            (framebuffer[byte_index] >> bit_shift) &
            0x03
        );
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
 * Dirty-region накапливается в display_refresh_region() и выводится в
 * display_flush().
 *
 * Для E-Ink 4-gray backend каждый refresh является полным:
 *
 *   EPD_2IN7_V2_Init_4GRAY()
 *   EPD_2IN7_V2_4GrayDisplay()
 *   EPD_2IN7_V2_Sleep()
 *
 * Поэтому координаты dirty-region для E-Ink не передаются
 * непосредственно в драйвер.
 */

static int16_t pending_x1 = -1;
static int16_t pending_y1 = -1;
static int16_t pending_x2 = -1;
static int16_t pending_y2 = -1;

static int pending_has_region = 0;


static void do_refresh_region(
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h
)
{
    /*
     * 1-bit: частичное обновление указанной области
     *        (display_eink сам переходит на полное, когда нужно).
     * 4-gray: частичного обновления нет, физически обновляется весь экран.
     */
    display_eink_refresh_region(framebuffer, x, y, w, h);
}


/*
 * Полное обновление дисплея.
 */
void display_refresh(void)
{
    pending_has_region = 0;

    /* Явное ПОЛНОЕ обновление (сброс остаточного изображения). */
    display_eink_refresh(framebuffer);
}


/*
 * Помечает область как изменённую.
 *
 * Реального обновления дисплея здесь не происходит.
 * Оно выполняется display_flush().
 */
void display_refresh_region(
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h
)
{
    /*
     * Clip области к границам дисплея.
     */
    if (x < 0)
    {
        w += x;
        x = 0;
    }

    if (y < 0)
    {
        h += y;
        y = 0;
    }

    if (x + w > DISPLAY_WIDTH)
    {
        w = DISPLAY_WIDTH - x;
    }

    if (y + h > DISPLAY_HEIGHT)
    {
        h = DISPLAY_HEIGHT - y;
    }

    if (w <= 0 || h <= 0)
    {
        return;
    }

    /*
     * Если это первая dirty-region,
     * просто сохраняем её.
     */
    if (!pending_has_region)
    {
        pending_x1 = x;
        pending_y1 = y;
        pending_x2 = (int16_t)(x + w - 1);
        pending_y2 = (int16_t)(y + h - 1);

        pending_has_region = 1;

        return;
    }

    /*
     * Если область уже существует,
     * расширяем её так, чтобы она включала
     * новую область.
     */
    if (x < pending_x1)
    {
        pending_x1 = x;
    }

    if (y < pending_y1)
    {
        pending_y1 = y;
    }

    if (x + w - 1 > pending_x2)
    {
        pending_x2 = (int16_t)(x + w - 1);
    }

    if (y + h - 1 > pending_y2)
    {
        pending_y2 = (int16_t)(y + h - 1);
    }
}


/*
 * Выполняет накопленное обновление дисплея.
 */
void display_flush(void)
{
    if (!pending_has_region)
    {
        return;
    }

    /*
     * E-Ink 1-bit: обновляем только накопленную область. Переход на
     * полное обновление (первое обновление, лимит частичных подряд)
     * выполняет display_eink_refresh_region().
     *
     * E-Ink 4-gray: display_eink_refresh_region() обновляет весь экран.
     */
    do_refresh_region(
        pending_x1,
        pending_y1,
        (int16_t)(pending_x2 - pending_x1 + 1),
        (int16_t)(pending_y2 - pending_y1 + 1)
    );

    pending_has_region = 0;
}