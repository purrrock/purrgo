#ifndef EMU_WINDOW_H
#define EMU_WINDOW_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#define PIXEL_SCALE 2
#define WINDOW_WIDTH (DISPLAY_WIDTH * PIXEL_SCALE)
#define UI_AREA_HEIGHT 100
#define WINDOW_HEIGHT (DISPLAY_HEIGHT * PIXEL_SCALE + UI_AREA_HEIGHT)

bool emu_window_init(SDL_Window** win, SDL_Renderer** ren, SDL_Texture** tex);
void emu_window_render(SDL_Renderer* renderer, SDL_Texture* fb_texture);
void emu_window_process_events(bool* quit);

#endif /* EMU_WINDOW_H */
