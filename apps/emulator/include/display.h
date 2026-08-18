#ifndef EMULATOR_DISPLAY_H
#define EMULATOR_DISPLAY_H

#include <stdint.h>

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 296
#define DISPLAY_BPP    2 // 2 bits per pixel
#define DISPLAY_FB_SIZE ((DISPLAY_WIDTH * DISPLAY_HEIGHT * DISPLAY_BPP) / 8)

// Color values (2-bit)
#define COLOR_BLACK     0x00
#define COLOR_DARK_GRAY 0x01
#define COLOR_LIGHT_GRAY 0x02
#define COLOR_WHITE     0x03

void display_init(void);
void display_clear(uint8_t color);
void display_set_pixel(int x, int y, uint8_t color);
void display_draw_char(int x, int y, char c, uint8_t color, uint8_t bg_color);
void display_draw_string(int x, int y, const char* str, uint8_t color, uint8_t bg_color);
const uint8_t* display_get_framebuffer(void);

#endif // EMULATOR_DISPLAY_H
