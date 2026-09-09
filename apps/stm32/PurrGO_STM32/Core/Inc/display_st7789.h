#ifndef DISPLAY_ST7789_H
#define DISPLAY_ST7789_H

#include "main.h"

/*
 * ST7789 Display Driver for STM32
 *
 * Used as a temporary debugging display until the e-Paper display is available.
 * Intentionally kept simple, utilizing blocking HAL SPI transmissions.
 */

#define ST7789_WIDTH  240
#define ST7789_HEIGHT 320

/*
 * Note on Coordinates:
 * Coordinates are inclusive. x: 0..239, y: 0..319
 */

/* Standard RGB565 Colors */
#define ST7789_COLOR_BLACK   0x0000
#define ST7789_COLOR_WHITE   0xFFFF
#define ST7789_COLOR_RED     0xF800
#define ST7789_COLOR_GREEN   0x07E0
#define ST7789_COLOR_BLUE    0x001F
#define ST7789_COLOR_CYAN    0x07FF
#define ST7789_COLOR_MAGENTA 0xF81F
#define ST7789_COLOR_YELLOW  0xFFE0

void ST7789_Init(void);

void ST7789_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

void ST7789_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

void ST7789_FillScreen(uint16_t color);

#endif /* DISPLAY_ST7789_H */
