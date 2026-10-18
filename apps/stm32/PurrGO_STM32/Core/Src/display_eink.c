#include "display_eink.h"

#include "DEV_Config.h"
#include "EPD_2in7_V2.h"

#include "main.h"
#include "spi.h"

#include "purrgo/hardware_config.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Делитель SPI1 для E-Ink.
 *
 * CubeMX настраивает SPI1 (APB2 = 84 МГц) с делителем 2, то есть ~42 МГц.
 * Это общая шина с ST7789, который такую частоту выдерживает, но
 * контроллер панели рассчитан на существенно меньшую (в референсных
 * проектах Waveshare - единицы МГц). Простые команды и константные
 * данные проходят и на 42 МГц, а данные с плотными переходами (LUT
 * 4-gray, реальное изображение) искажаются: на экране остаётся мусор.
 *
 * Делитель 8 -> 10.5 МГц. Можно переопределить в compile definitions.
 */
#ifndef PURRGO_EINK_SPI_PRESCALER
#define PURRGO_EINK_SPI_PRESCALER SPI_BAUDRATEPRESCALER_8
#endif

/*
 * Сколько частичных обновлений подряд допускается до принудительного
 * полного обновления.
 *
 * Спецификация панели рекомендует полное обновление после 5 подряд
 * идущих fast/partial обновлений, чтобы не накапливать остаточное
 * изображение (ghosting). Значение можно увеличить, если ghosting
 * на вашем экземпляре приемлем.
 */
#ifndef PURRGO_EINK_PARTIALS_BEFORE_FULL
#define PURRGO_EINK_PARTIALS_BEFORE_FULL 5U
#endif

#define EINK_WIDTH          EPD_2IN7_V2_WIDTH
#define EINK_HEIGHT         EPD_2IN7_V2_HEIGHT

/* Строка 2-битного framebuffer PurrGO: 4 пикселя в байте. */
#define EINK_FB2_ROW_BYTES  (EINK_WIDTH / 4U)

/* Размер 2-битного framebuffer PurrGO. */
#define EINK_FRAMEBUFFER_SIZE ((EINK_WIDTH * EINK_HEIGHT * 2U) / 8U)

#if (EINK_FRAMEBUFFER_SIZE != 11616U)
#error "Unexpected E-Ink framebuffer size"
#endif

#if (EINK_WIDTH % 8U) != 0U
#error "E-Ink width must be a multiple of 8"
#endif


void display_eink_init(void)
{
    /*
     * Переключаем SPI1 на тактовую частоту, допустимую для панели.
     * HAL_SPI_DeInit/Init вызывает MspDeInit/MspInit, GPIO PA5/PA7
     * настраиваются заново тем же кодом CubeMX.
     */
    (void)HAL_SPI_DeInit(&hspi1);
    hspi1.Init.BaudRatePrescaler = PURRGO_EINK_SPI_PRESCALER;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * DEV_Module_Init() устанавливает начальные уровни управляющих
     * линий E-Ink: CS = HIGH, DC = LOW, RST = HIGH.
     */
    (void)DEV_Module_Init();
}


void display_eink_sleep(void)
{
    EPD_2IN7_V2_Sleep();
}


#if PURRGO_HW_EINK_MODE == PURRGO_EINK_MODE_1BIT

/*
 * ===========================================================================
 * Режим 1-bit (чёрно-белый) с частичным обновлением
 * ===========================================================================
 *
 * Формат буфера панели Waveshare в 1-bit режиме:
 *   1 бит на пиксель, старший бит = левый пиксель,
 *   1 = белый, 0 = чёрный, 22 байта на строку.
 *
 * Преобразование из 2-битного буфера PurrGO:
 *   пиксель белый (значение 3) -> 1, любое другое значение -> 0.
 */

#define EINK_MONO_ROW_BYTES (EINK_WIDTH / 8U)
#define EINK_MONO_SIZE      (EINK_MONO_ROW_BYTES * EINK_HEIGHT)

/*
 * Рабочий буфер преобразования. Источником истины остаётся 2-битный
 * framebuffer PurrGO, поэтому буфер хранит только то, что сейчас
 * передаётся на панель.
 */
static uint8_t s_mono[EINK_MONO_SIZE];

/* В RAM панели загружено базовое изображение (требуется для partial). */
static bool s_base_valid = false;

/* Частичных обновлений с момента последнего полного. */
static uint8_t s_partial_count = 0U;


/*
 * Один 2-битный байт (4 пикселя) -> 4 бита (1 = белый).
 * Пиксель k лежит в битах [2*(3-k)+1 : 2*(3-k)].
 */
static inline uint8_t eink_white_nibble(uint8_t v)
{
    uint8_t t = (uint8_t)((v >> 1) & v & 0x55U);

    return (uint8_t)(((t >> 3) & 0x8U) |
                     ((t >> 2) & 0x4U) |
                     ((t >> 1) & 0x2U) |
                     ( t       & 0x1U));
}

/*
 * Преобразует прямоугольник из 2-битного буфера PurrGO в плотно
 * упакованный 1-bit буфер (ширина области = (xb1 - xb0) байт).
 *
 *   xb0, xb1 - границы по X в байтах 1-bit буфера (xb1 не включается);
 *   y0,  y1  - границы по Y в строках (y1 не включается).
 */
static void eink_convert_rect(
    const uint8_t *fb2,
    uint16_t xb0,
    uint16_t xb1,
    uint16_t y0,
    uint16_t y1,
    uint8_t *out)
{
    for (uint16_t y = y0; y < y1; y++)
    {
        const uint8_t *src = &fb2[(uint32_t)y * EINK_FB2_ROW_BYTES + (uint32_t)xb0 * 2U];

        for (uint16_t xb = xb0; xb < xb1; xb++)
        {
            *out++ = (uint8_t)((eink_white_nibble(src[0]) << 4) |
                                eink_white_nibble(src[1]));
            src += 2;
        }
    }
}


void display_eink_refresh(const uint8_t *framebuffer)
{
    if (framebuffer == NULL)
    {
        return;
    }

    eink_convert_rect(framebuffer, 0U, EINK_MONO_ROW_BYTES, 0U, EINK_HEIGHT, s_mono);

    /*
     * Полное обновление по референсу Waveshare для partial-режима:
     * Init() + Display_Base(). Display_Base() записывает изображение в
     * ОБЕ RAM-плоскости контроллера (0x24 и 0x26), после чего
     * Display_Partial() корректно сравнивает новое и старое состояние.
     */
    EPD_2IN7_V2_Init();
    EPD_2IN7_V2_Display_Base(s_mono);
    EPD_2IN7_V2_Sleep();

    s_base_valid = true;
    s_partial_count = 0U;
}


void display_eink_refresh_region(
    const uint8_t *framebuffer,
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h)
{
    if (framebuffer == NULL)
    {
        return;
    }

    /* Clip к границам панели. */
    if (x < 0) { w = (int16_t)(w + x); x = 0; }
    if (y < 0) { h = (int16_t)(h + y); y = 0; }
    if (x + w > (int16_t)EINK_WIDTH)  { w = (int16_t)(EINK_WIDTH - x); }
    if (y + h > (int16_t)EINK_HEIGHT) { h = (int16_t)(EINK_HEIGHT - y); }

    if (w <= 0 || h <= 0)
    {
        return;
    }

    /*
     * Нет базового изображения или накопилось много частичных
     * обновлений - делаем полное.
     */
    if (!s_base_valid || s_partial_count >= PURRGO_EINK_PARTIALS_BEFORE_FULL)
    {
        display_eink_refresh(framebuffer);
        return;
    }

    /*
     * Область по X выравнивается до границ 8 пикселей: панель
     * адресуется байтами по 8 пикселей.
     */
    uint16_t xs = (uint16_t)x & ~7U;
    uint16_t xe = ((uint16_t)(x + w) + 7U) & ~7U;
    uint16_t ys = (uint16_t)y;
    uint16_t ye = (uint16_t)(y + h);

    if (xe > EINK_WIDTH)
    {
        xe = EINK_WIDTH;
    }

    eink_convert_rect(framebuffer, xs / 8U, xe / 8U, ys, ye, s_mono);

    /*
     * Display_Partial принимает плотно упакованную область, конец
     * диапазона не включается. При выровненных xs/xe драйвер
     * использует ветку (Xend - Xstart) % 8 == 0.
     */
    EPD_2IN7_V2_Display_Partial(s_mono, xs, ys, xe, ye);
    EPD_2IN7_V2_Sleep();

    s_partial_count++;
}


#elif PURRGO_HW_EINK_MODE == PURRGO_EINK_MODE_4GRAY

/*
 * ===========================================================================
 * Режим 4-gray (только полное обновление)
 * ===========================================================================
 *
 * Двухбитный framebuffer PurrGO напрямую соответствует формату
 * EPD_2IN7_V2_4GrayDisplay(), преобразование не требуется.
 */

void display_eink_refresh(const uint8_t *framebuffer)
{
    if (framebuffer == NULL)
    {
        return;
    }

    /*
     * После EPD_2IN7_V2_Sleep() дисплей необходимо повторно
     * инициализировать, поэтому Init_4GRAY() выполняется перед каждым
     * физическим обновлением.
     */
    EPD_2IN7_V2_Init_4GRAY();
    EPD_2IN7_V2_4GrayDisplay((UBYTE *)framebuffer);
    EPD_2IN7_V2_Sleep();
}

void display_eink_refresh_region(
    const uint8_t *framebuffer,
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h)
{
    /* В 4-gray частичного обновления нет: всегда весь экран. */
    (void)x;
    (void)y;
    (void)w;
    (void)h;

    display_eink_refresh(framebuffer);
}

#endif /* PURRGO_HW_EINK_MODE */
