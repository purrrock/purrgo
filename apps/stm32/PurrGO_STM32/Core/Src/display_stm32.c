#include "display_stm32.h"

#include "purrgo/display_hal.h"
#include "purrgo/logger.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Выбор аппаратного display backend выполняется во время компиляции.
 *
 * ST7789:
 *   используется для текущей отладки на LCD.
 *
 * E-Ink:
 *   используется для конечного Waveshare 2.7" E-Paper V2.
 */
#if PURRGO_HW_DISPLAY_BACKEND == PURRGO_DISPLAY_BACKEND_EINK

#include "display_eink.h"

#elif PURRGO_HW_DISPLAY_BACKEND == PURRGO_DISPLAY_BACKEND_ST7789

#include "display_st7789.h"

#else

#error "Unsupported PurrGO display backend"

#endif


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

#if PURRGO_HW_DISPLAY_BACKEND == PURRGO_DISPLAY_BACKEND_EINK

    display_eink_init();

#elif PURRGO_HW_DISPLAY_BACKEND == PURRGO_DISPLAY_BACKEND_ST7789

    ST7789_Init();

#endif
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
 * --------------------------------------------------------------------------
 * Refresh management
 * --------------------------------------------------------------------------
 *
 * Для ST7789 поддерживается накопление dirty-region и partial refresh.
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

static int partial_refresh_count = 0;

static int16_t pending_x1 = -1;
static int16_t pending_y1 = -1;
static int16_t pending_x2 = -1;
static int16_t pending_y2 = -1;

static int pending_has_region = 0;


#if PURRGO_HW_DISPLAY_BACKEND == PURRGO_DISPLAY_BACKEND_ST7789

/*
 * Преобразование логических цветов PurrGO в RGB565
 * для отладочного ST7789.
 */
static const uint16_t color_lut[4] =
{
    ST7789_COLOR_BLACK,
    0x52AA,
    0xAD55,
    ST7789_COLOR_WHITE
};


static void do_refresh_region(
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

    ST7789_SetWindow(
        x,
        y,
        x + w - 1,
        y + h - 1
    );

    ST7789_StartPixels();

    int buffer_index = 0;

    for (int row = y; row < y + h; row++)
    {
        int pixel_index =
            row * DISPLAY_WIDTH + x;

        for (int col = x; col < x + w; col++)
        {
            /*
             * Извлекаем 2-битный цвет PurrGO
             * из framebuffer.
             */
            int byte_index =
                pixel_index >> 2;

            int bit_shift =
                (3 - (pixel_index & 3)) << 1;

            uint8_t color2bpp =
                (uint8_t)(
                    (framebuffer[byte_index] >> bit_shift) &
                    0x03
                );

            /*
             * Преобразуем логический цвет
             * в RGB565 ST7789.
             */
            uint16_t rgb565 =
                color_lut[color2bpp];

            display_tx_buffer[buffer_index << 1] =
                (uint8_t)(rgb565 >> 8);

            display_tx_buffer[(buffer_index << 1) | 1] =
                (uint8_t)(rgb565 & 0xFF);

            buffer_index++;
            pixel_index++;

            /*
             * Передаём заполненную часть
             * промежуточного SPI-буфера.
             */
            if (buffer_index >= DISPLAY_TX_BUF_PIXELS)
            {
                ST7789_WritePixels(
                    display_tx_buffer,
                    buffer_index << 1
                );

                buffer_index = 0;
            }
        }
    }

    /*
     * Передаём остаток данных.
     */
    if (buffer_index > 0)
    {
        ST7789_WritePixels(
            display_tx_buffer,
            buffer_index << 1
        );
    }

    ST7789_EndPixels();
}


#elif PURRGO_HW_DISPLAY_BACKEND == PURRGO_DISPLAY_BACKEND_EINK

static void do_refresh_region(
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h
)
{
    /*
     * Параметры области здесь намеренно не используются.
     *
     * EPD_2IN7_V2_4GrayDisplay() выполняет полное
     * обновление всего дисплея.
     *
     * Поэтому даже если GFX сообщает небольшую
     * dirty-region, физически обновляется весь
     * E-Ink экран.
     */
    (void)x;
    (void)y;
    (void)w;
    (void)h;

    display_eink_refresh(framebuffer);
}

#endif


/*
 * Полное обновление дисплея.
 */
void display_refresh(void)
{
    partial_refresh_count = 0;
    pending_has_region = 0;

    do_refresh_region(
        0,
        0,
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT
    );
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
        pending_x2 = x + w - 1;
        pending_y2 = y + h - 1;

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
        pending_x2 = x + w - 1;
    }

    if (y + h - 1 > pending_y2)
    {
        pending_y2 = y + h - 1;
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


#if PURRGO_HW_DISPLAY_BACKEND == PURRGO_DISPLAY_BACKEND_EINK

    /*
     * Для E-Ink 4-gray нет соответствующего
     * partial-refresh пути в используемом
     * EPD_2IN7_V2 driver.
     *
     * Поэтому любая dirty-region приводит
     * к полному обновлению экрана.
     */
    do_refresh_region(
        0,
        0,
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT
    );

    pending_has_region = 0;
    partial_refresh_count = 0;


#elif PURRGO_HW_DISPLAY_BACKEND == PURRGO_DISPLAY_BACKEND_ST7789

    int16_t w =
        pending_x2 - pending_x1 + 1;

    int16_t h =
        pending_y2 - pending_y1 + 1;

    /*
     * После определённого количества partial refresh
     * выполняем полный refresh.
     */
    if (partial_refresh_count >= MAX_PARTIAL_REFRESHES)
    {
        display_refresh();
    }
    else
    {
        partial_refresh_count++;

        do_refresh_region(
            pending_x1,
            pending_y1,
            w,
            h
        );

        pending_has_region = 0;
    }

#endif
}