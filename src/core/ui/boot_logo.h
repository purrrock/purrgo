#ifndef PURRGO_BOOT_LOGO_DATA_H
#define PURRGO_BOOT_LOGO_DATA_H

#include <stdint.h>

/*
 * Логотип загрузочного баннера (см. purrgo/boot_banner.h). Данные генерирует
 * tools/make_boot_logo.py из docs/img/purrgo-logoBW.jpeg.
 *
 * 2 bpp в раскладке framebuffer (purrgo/gfx_fb2bpp.h): значения 0..3 - те же
 * логические цвета, что BLACK..WHITE; по четыре пикселя в байте, первый - в
 * старших битах; каждая строка занимает PURRGO_BOOT_LOGO_W / 4 байт.
 */
#define PURRGO_BOOT_LOGO_W      160
#define PURRGO_BOOT_LOGO_H      160
#define PURRGO_BOOT_LOGO_BYTES  ((PURRGO_BOOT_LOGO_W / 4) * PURRGO_BOOT_LOGO_H)

extern const uint8_t purrgo_boot_logo[PURRGO_BOOT_LOGO_BYTES];

#endif /* PURRGO_BOOT_LOGO_DATA_H */
