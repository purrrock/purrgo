#include "emu_window.h"
#include "display.h"
#include <purrgo/app_fsm.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    SDL_Rect rect;
    const char* label;
    purrgo_btn_t btn_val;
    bool is_active;
} ButtonState;

ButtonState buttons[] = {
    {{10, DISPLAY_HEIGHT * PIXEL_SCALE + 10, 50, 30}, "KEY1", PURRGO_BTN_KEY1_SHORT, true},
    {{70, DISPLAY_HEIGHT * PIXEL_SCALE + 10, 50, 30}, "KEY2", PURRGO_BTN_KEY2_SHORT, true},
    {{130, DISPLAY_HEIGHT * PIXEL_SCALE + 10, 50, 30}, "KEY3", PURRGO_BTN_KEY3_SHORT, true},
    {{190, DISPLAY_HEIGHT * PIXEL_SCALE + 10, 50, 30}, "KEY4", PURRGO_BTN_KEY4_SHORT, true}
};

#define NUM_BUTTONS (sizeof(buttons) / sizeof(buttons[0]))

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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
        stderr,"EMU: button=%d state_before=%d\n",
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
        /* Нет GPU/WebGL (виртуальная машина, headless): программный рендерер. */
        *ren = SDL_CreateRenderer(*win, -1, SDL_RENDERER_SOFTWARE);
    }
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

static uint32_t key_press_times[4] = {0};
static int active_mouse_button = -1;
static uint32_t mouse_press_time = 0;

/*
 * Нажатие/отпускание аппаратной кнопки K1..K4 (index 0..3).
 * Длинное нажатие определяется по времени между down и up, как на устройстве.
 * Используется и клавишами 1-4, и (в web-сборке) HTML-кнопками страницы.
 */
static void emu_key_down(int index) {
    if (index < 0 || index > 3) {
        return;
    }
    key_press_times[index] = SDL_GetTicks();
}

static void emu_key_up(int index) {
    static const purrgo_btn_t short_btn[4] = {
        PURRGO_BTN_KEY1_SHORT, PURRGO_BTN_KEY2_SHORT,
        PURRGO_BTN_KEY3_SHORT, PURRGO_BTN_KEY4_SHORT
    };
    static const purrgo_btn_t long_btn[4] = {
        PURRGO_BTN_KEY1_LONG, PURRGO_BTN_KEY2_LONG,
        PURRGO_BTN_KEY3_LONG, PURRGO_BTN_KEY4_LONG
    };

    if (index < 0 || index > 3 || key_press_times[index] == 0) {
        return;
    }
    if (SDL_GetTicks() - key_press_times[index] >= PURRGO_BTN_LONG_PRESS_MS) {
        handle_button_press(long_btn[index]);
    } else {
        handle_button_press(short_btn[index]);
    }
    key_press_times[index] = 0;
}

#ifdef __EMSCRIPTEN__
/* Вызывается из JS (HTML-кнопки страницы): idx 0..3 = K1..K4, down 1/0. */
EMSCRIPTEN_KEEPALIVE void purrgo_web_key(int idx, int down) {
    if (down) {
        emu_key_down(idx);
    } else {
        emu_key_up(idx);
    }
}

/* Одновременное нажатие K1+K4 (выход из режима трекера). */
EMSCRIPTEN_KEEPALIVE void purrgo_web_combo_k1_k4(void) {
    handle_button_press(PURRGO_BTN_COMBO_K1_K4);
}
#endif

static int sdl_key_to_index(SDL_Keycode sym) {
    switch (sym) {
        case SDLK_1: return 0;
        case SDLK_2: return 1;
        case SDLK_3: return 2;
        case SDLK_4: return 3;
        default:     return -1;
    }
}

void emu_window_process_events(bool* quit) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            *quit = true;
        } else if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
            const int idx = sdl_key_to_index(e.key.keysym.sym);
            if (idx >= 0) {
                emu_key_down(idx);
            } else if (e.key.keysym.sym == SDLK_5) {
                /* Эмуляция одновременного K1+K4 (выход из режима трекера). */
                handle_button_press(PURRGO_BTN_COMBO_K1_K4);
            }
        } else if (e.type == SDL_KEYUP) {
            const int idx = sdl_key_to_index(e.key.keysym.sym);
            if (idx >= 0) {
                emu_key_up(idx);
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
                    if (buttons[i].is_active) {
                        if (buttons[i].btn_val >= PURRGO_BTN_KEY1_SHORT && buttons[i].btn_val <= PURRGO_BTN_KEY4_SHORT) {
                            active_mouse_button = i;
                            mouse_press_time = SDL_GetTicks();
                        } else {
                            handle_button_press(buttons[i].btn_val);
                        }
                    }
                    break;
                }
            }
        } else if (e.type == SDL_MOUSEBUTTONUP) {
            if (active_mouse_button >= 0) {
                uint32_t duration = SDL_GetTicks() - mouse_press_time;
                purrgo_btn_t base_btn = buttons[active_mouse_button].btn_val;

                if (duration >= PURRGO_BTN_LONG_PRESS_MS) {
                    // Offset to long press enum
                    handle_button_press(base_btn + (PURRGO_BTN_KEY1_LONG - PURRGO_BTN_KEY1_SHORT));
                } else {
                    handle_button_press(base_btn);
                }

                active_mouse_button = -1;
                mouse_press_time = 0;
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

#ifndef __EMSCRIPTEN__
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
#endif

    SDL_RenderPresent(renderer);
}
