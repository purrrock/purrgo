#ifndef EMULATOR_DISPLAY_H
#define EMULATOR_DISPLAY_H

#include <stdint.h>
#include "purrgo/hardware_config.h"

/*
 * Геометрия дисплея и глубина цвета транслируются напрямую
 * из активного профиля аппаратной конфигурации.
 */
#define DISPLAY_WIDTH       PURRGO_HW_DISPLAY_WIDTH_PX
#define DISPLAY_HEIGHT      PURRGO_HW_DISPLAY_HEIGHT_PX
#define DISPLAY_BPP         PURRGO_HW_DISPLAY_BPP

/*
 * Размер кадрового буфера в байтах:
 * (Ширина * Высота * Бит_на_пиксель) / 8 бит в байте
 */
#define DISPLAY_FB_SIZE     ((DISPLAY_WIDTH * DISPLAY_HEIGHT * DISPLAY_BPP) / 8)

/* 
 * 2-битная палитра градаций серого (логические уровни):
 * 00b = Black, 01b = Dark Gray, 10b = Light Gray, 11b = White
 */
#define COLOR_BLACK         0x00
#define COLOR_DARK_GRAY     0x01
#define COLOR_LIGHT_GRAY    0x02
#define COLOR_WHITE         0x03

void display_init(void);
void display_clear(uint8_t color);

/*
 * Установка пикселя в кадровый буфер.
 * Используются явные типы int16_t для координат с целью предотвращения
 * неявных приведений типов и корректной работы знакового отсечения (clipping).
 */
void display_set_pixel(int16_t x, int16_t y, uint8_t color);

const uint8_t* display_get_framebuffer(void);

#endif // EMULATOR_DISPLAY_H