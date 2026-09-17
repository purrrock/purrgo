#ifndef DISPLAY_EINK_H
#define DISPLAY_EINK_H

#include <stdint.h>

/*
 * Инициализация аппаратной части E-Ink.
 *
 * SPI1 и GPIO уже должны быть инициализированы CubeMX
 * до вызова этой функции.
 */
void display_eink_init(void);

/*
 * Полное обновление E-Ink дисплея.
 *
 * framebuffer:
 *   Буфер PurrGO размером 11616 байт.
 *   Формат буфера должен соответствовать формату,
 *   который принимает EPD_2IN7_V2_4GrayDisplay().
 *
 * Для 2.7" E-Paper V2:
 *   WIDTH  = 176 пикселей
 *   HEIGHT = 264 пикселей
 *   2 бита на пиксель
 *   11616 байт.
 *
 * В режиме 4-gray Waveshare не предоставляет
 * соответствующего partial-refresh API, поэтому
 * каждый вызов выполняет полное обновление дисплея.
 */
void display_eink_refresh(const uint8_t *framebuffer);

#endif /* DISPLAY_EINK_H */