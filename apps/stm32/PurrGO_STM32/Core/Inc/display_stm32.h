#ifndef DISPLAY_STM32_H
#define DISPLAY_STM32_H

#include <stdint.h>

#include "purrgo/hardware_config.h"
#include <purrgo/gfx_renderer.h>

/*
 * Геометрия дисплея и глубина цвета берутся из активной
 * аппаратной конфигурации.
 *
 * Для Waveshare 2.7" E-Paper V2:
 *   WIDTH  = 176
 *   HEIGHT = 264
 *   BPP    = 2
 */
#define DISPLAY_WIDTH       PURRGO_HW_DISPLAY_WIDTH_PX
#define DISPLAY_HEIGHT      PURRGO_HW_DISPLAY_HEIGHT_PX
#define DISPLAY_BPP         PURRGO_HW_DISPLAY_BPP

/*
 * 2-битная палитра PurrGO:
 *
 *   0 = Black
 *   1 = Dark Gray
 *   2 = Light Gray
 *   3 = White
 *
 * Эти значения являются логическими цветами PurrGO.
 * Конкретный физический формат передачи определяется
 * выбранным display backend.
 */
#define COLOR_BLACK         0x00
#define COLOR_DARK_GRAY     0x01
#define COLOR_LIGHT_GRAY    0x02
#define COLOR_WHITE         0x03

void display_init(void);

void display_clear(uint8_t color);

/*
 * Установка пикселя в кадровый буфер.
 *
 * Координаты могут быть отрицательными: функция сама
 * выполняет проверку выхода за границы дисплея.
 */
void display_set_pixel(
    int16_t x,
    int16_t y,
    uint8_t color
);

/*
 * Чтение пикселя из кадрового буфера.
 *
 * При выходе координат за границы возвращается COLOR_BLACK.
 */
uint8_t display_get_pixel(
    int16_t x,
    int16_t y
);

/*
 * Возвращает указатель на внутренний framebuffer.
 */
const uint8_t *display_get_framebuffer(void);

/*
 * GFX -> display callbacks.
 */
void stm32_draw_pixel_cb(
    void *fb,
    int16_t x,
    int16_t y,
    gfx_color_t color
);

gfx_color_t stm32_read_pixel_cb(
    void *fb,
    int16_t x,
    int16_t y
);

void stm32_clear_cb(
    void *fb,
    gfx_color_t color
);

#endif /* DISPLAY_STM32_H */