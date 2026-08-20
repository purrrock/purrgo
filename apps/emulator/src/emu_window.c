#include "emu_window.h"
#include "display.h"
#include <purrgo/app_fsm.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    SDL_Rect rect;
    const char* label;
    purrgo_btn_t btn_val;
} ButtonState;

ButtonState buttons[] = {
    {{50, DISPLAY_HEIGHT * PIXEL_SCALE + 5, 40, 30}, "UP", PURRGO_BTN_UP},
    {{50, DISPLAY_HEIGHT * PIXEL_SCALE + 65, 40, 30}, "DOWN", PURRGO_BTN_DOWN},
    {{5, DISPLAY_HEIGHT * PIXEL_SCALE + 35, 40, 30}, "LEFT", PURRGO_BTN_LEFT},
    {{95, DISPLAY_HEIGHT * PIXEL_SCALE + 35, 40, 30}, "RIGHT", PURRGO_BTN_RIGHT},
    {{160, DISPLAY_HEIGHT * PIXEL_SCALE + 15, 40, 30}, "PLUS", PURRGO_BTN_PLUS},
    {{160, DISPLAY_HEIGHT * PIXEL_SCALE + 55, 40, 30}, "MINUS", PURRGO_BTN_MINUS},
    {{210, DISPLAY_HEIGHT * PIXEL_SCALE + 15, 40, 30}, "MENU", PURRGO_BTN_MENU},
    {{210, DISPLAY_HEIGHT * PIXEL_SCALE + 55, 40, 30}, "OK", PURRGO_BTN_OK}
};

#define NUM_BUTTONS (sizeof(buttons) / sizeof(buttons[0]))

/*
 * 2-bit framebuffer palette.
 */
uint32_t palette[4] = {
    0xFF000000,
    0xFF555555,
    0xFFAAAAAA,
    0xFFFFFFFF
};

static void render_fb_to_texture(SDL_Texture* texture) {
    const uint8_t* fb = display_get_framebuffer();
    uint32_t pixels[DISPLAY_WIDTH * DISPLAY_HEIGHT];

    for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
        int byte_idx = i / 4;
        int bit_shift = (3 - (i % 4)) * 2;
        uint8_t color_val = (fb[byte_idx] >> bit_shift) & 0x03;
        pixels[i] = palette[color_val];
    }

    SDL_UpdateTexture(
        texture,
        NULL,
        pixels,
        DISPLAY_WIDTH * sizeof(uint32_t)
    );
}

void sdl_draw_text(
    SDL_Renderer* renderer,
    int x,
    int y,
    const char* text
) {
    extern const unsigned char font5x7[256][5];

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    int cur_x = x;

    while (*text) {
        unsigned char c = (unsigned char)*text;
        const unsigned char* bitmap = font5x7[c];

        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 8; row++) {
                if ((bitmap[col] >> row) & 1) {
                    SDL_RenderDrawPoint(renderer, cur_x + col, y + row);
                }
            }
        }
        cur_x += 6;
        text++;
    }
}

static void handle_button_press(purrgo_btn_t btn_val) {
    fprintf(
        stderr,
        "EMU: button=%d state_before=%d\n",
        (int)btn_val,
        (int)purrgo_app_get_state()
    );
    fflush(stderr);

    purrgo_app_handle_button(btn_val);

    fprintf(
        stderr,
        "EMU: state_after=%d\n",
        (int)purrgo_app_get_state()
    );
    fflush(stderr);
}

bool emu_window_init(SDL_Window** win, SDL_Renderer** ren, SDL_Texture** tex) {
    *win = SDL_CreateWindow(
        "PurrGo Emulator (E-Ink 3 FPS)",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!*win) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    *ren = SDL_CreateRenderer(*win, -1, SDL_RENDERER_ACCELERATED);
    if (!*ren) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    *tex = SDL_CreateTexture(
        *ren,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT
    );

    if (!*tex) {
        fprintf(stderr, "Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

void emu_window_process_events(bool* quit) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            *quit = true;
        } else if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_UP:
                    handle_button_press(PURRGO_BTN_UP);
                    break;
                case SDLK_DOWN:
                    handle_button_press(PURRGO_BTN_DOWN);
                    break;
                case SDLK_LEFT:
                    handle_button_press(PURRGO_BTN_LEFT);
                    break;
                case SDLK_RIGHT:
                    handle_button_press(PURRGO_BTN_RIGHT);
                    break;
                case SDLK_KP_PLUS:
                case SDLK_PLUS:
                    handle_button_press(PURRGO_BTN_PLUS);
                    break;
                case SDLK_KP_MINUS:
                case SDLK_MINUS:
                    handle_button_press(PURRGO_BTN_MINUS);
                    break;
                case SDLK_m:
                    handle_button_press(PURRGO_BTN_MENU);
                    break;
                case SDLK_RETURN:
                case SDLK_RETURN2:
                case SDLK_KP_ENTER:
                    handle_button_press(PURRGO_BTN_OK);
                    break;
            }
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            int x = e.button.x;
            int y = e.button.y;

            for (size_t i = 0; i < NUM_BUTTONS; ++i) {
                if (
                    x >= buttons[i].rect.x &&
                    x <= buttons[i].rect.x + buttons[i].rect.w &&
                    y >= buttons[i].rect.y &&
                    y <= buttons[i].rect.y + buttons[i].rect.h
                ) {
                    handle_button_press(buttons[i].btn_val);
                    break;
                }
            }
        }
    }
}

void emu_window_render(SDL_Renderer* renderer, SDL_Texture* fb_texture) {
    render_fb_to_texture(fb_texture);

    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderClear(renderer);

    SDL_Rect dest_rect = {
        0,
        0,
        WINDOW_WIDTH,
        DISPLAY_HEIGHT * PIXEL_SCALE
    };

    SDL_RenderCopy(renderer, fb_texture, NULL, &dest_rect);

    for (size_t i = 0; i < NUM_BUTTONS; ++i) {
        SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
        SDL_RenderFillRect(renderer, &buttons[i].rect);

        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderDrawRect(renderer, &buttons[i].rect);

        int text_len = (int)strlen(buttons[i].label);
        int text_w = text_len * 6;
        int text_h = 8;

        int text_x = buttons[i].rect.x + (buttons[i].rect.w - text_w) / 2;
        int text_y = buttons[i].rect.y + (buttons[i].rect.h - text_h) / 2;

        sdl_draw_text(renderer, text_x, text_y, buttons[i].label);
    }

    SDL_RenderPresent(renderer);
}
