#ifndef DISPLAY_EINK_H
#define DISPLAY_EINK_H

#include <stdint.h>

/*
 * Backend E-Ink Waveshare 2.7" V2 (176 x 264).
 *
 * Внутренний framebuffer PurrGO всегда 2-битный (11616 байт):
 *   0 = Black, 1 = Dark Gray, 2 = Light Gray, 3 = White.
 *
 * Режим передачи на панель выбирается PURRGO_HW_EINK_MODE
 * (purrgo/hardware_config.h):
 *
 *   PURRGO_EINK_MODE_4GRAY - 4 уровня серого, только полное обновление;
 *   PURRGO_EINK_MODE_1BIT  - белый -> белый, любой другой цвет -> чёрный,
 *                            поддерживается частичное обновление.
 */

/*
 * Инициализация аппаратной части E-Ink.
 * SPI1 и GPIO уже должны быть инициализированы CubeMX.
 */
void display_eink_init(void);

/*
 * ПОЛНОЕ обновление всего экрана.
 *
 * В режиме 1-bit после него панель готова к частичным обновлениям
 * (в обе RAM-плоскости контроллера записывается базовое изображение).
 */
void display_eink_refresh(const uint8_t *framebuffer);

/*
 * Обновление прямоугольной области (координаты в пикселях).
 *
 * 1-bit: частичное обновление. По X область расширяется до границ
 *        8 пикселей. Автоматически выполняется ПОЛНОЕ обновление, если
 *        базовое изображение ещё не загружено или накоплено
 *        PURRGO_EINK_PARTIALS_BEFORE_FULL частичных обновлений подряд
 *        (рекомендация производителя против накопления остаточного
 *        изображения).
 * 4-gray: эквивалентно display_eink_refresh().
 */
void display_eink_refresh_region(
    const uint8_t *framebuffer,
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h);

/*
 * Перевод E-Ink дисплея в режим сна.
 */
void display_eink_sleep(void);

#endif /* DISPLAY_EINK_H */
