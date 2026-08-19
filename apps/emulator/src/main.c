#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include "display.h"
#include <purrgo/gnss_types.h>
#include <purrgo/gnss_mock.h>
#include <purrgo/app_fsm.h>

#define PIXEL_SCALE 2
#define WINDOW_WIDTH (DISPLAY_WIDTH * PIXEL_SCALE)
#define UI_AREA_HEIGHT 100
#define WINDOW_HEIGHT (DISPLAY_HEIGHT * PIXEL_SCALE + UI_AREA_HEIGHT)

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
#define NUM_BUTTONS (sizeof(buttons)/sizeof(buttons[0]))

uint32_t palette[4] = {
    0xFF000000, // Black
    0xFF555555, // Dark Gray
    0xFFAAAAAA, // Light Gray
    0xFFFFFFFF  // White
};

void render_fb_to_texture(SDL_Texture* texture) {
    const uint8_t* fb = display_get_framebuffer();
    uint32_t pixels[DISPLAY_WIDTH * DISPLAY_HEIGHT];

    for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
        int byte_idx = i / 4;
        int bit_shift = (3 - (i % 4)) * 2;
        uint8_t color_val = (fb[byte_idx] >> bit_shift) & 0x03;
        pixels[i] = palette[color_val];
    }

    SDL_UpdateTexture(texture, NULL, pixels, DISPLAY_WIDTH * sizeof(uint32_t));
}

void draw_text(SDL_Renderer* renderer, int x, int y, const char* text) {
    // Since SDL doesn't have built-in text rendering without SDL_ttf,
    // we use our 5x7 font just by drawing points on renderer directly!
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

void handle_button_press(purrgo_btn_t btn_val) {
    purrgo_app_handle_button(btn_val);
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("PurrGo Emulator",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          WINDOW_WIDTH, WINDOW_HEIGHT,
                                          SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* fb_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                                SDL_TEXTUREACCESS_STREAMING,
                                                DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (!fb_texture) {
        fprintf(stderr, "Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    display_init();
    display_draw_string(0, 0, "PurrGo Emulator\nReady.", COLOR_BLACK, COLOR_WHITE);

    purrgo_gnss_solution_t mock_gnss;
    purrgo_gnss_mock_init(&mock_gnss);

    purrgo_app_init();

    uint32_t last_update_time = SDL_GetTicks();

    bool quit = false;
    SDL_Event e;

    while (!quit) {
        uint32_t current_time = SDL_GetTicks();
        if (current_time - last_update_time >= 1000) {
            last_update_time = current_time;
            purrgo_gnss_mock_update(&mock_gnss);
            purrgo_app_update(&mock_gnss);
        }

        display_clear(COLOR_WHITE);

        char buf[32];

        if (purrgo_app_get_state() == APP_STATE_MENU_CONFIG) {
            display_draw_string(10, 10, "=== CONFIG ===", COLOR_BLACK, COLOR_WHITE);

            int16_t draft_tz = purrgo_app_get_draft_tz_offset();
            char sign = (draft_tz >= 0) ? '+' : '-';
            int16_t abs_tz = (draft_tz >= 0) ? draft_tz : -draft_tz;
            int hours = abs_tz / 60;
            int mins = abs_tz % 60;

            snprintf(buf, sizeof(buf), "TZ: UTC%c%02d:%02d", sign, hours, mins);
            display_draw_string(10, 40, buf, COLOR_WHITE, COLOR_BLACK);

            display_draw_string(10, 60, "+/- : Change", COLOR_BLACK, COLOR_WHITE);
            display_draw_string(10, 80, "OK : Save", COLOR_BLACK, COLOR_WHITE);
            display_draw_string(10, 100, "MENU: Cancel", COLOR_BLACK, COLOR_WHITE);
        } else {
            int y_pos = 10;

            // TIME
        snprintf(buf, sizeof(buf), "TIME: %02d:%02d:%02d", mock_gnss.hours, mock_gnss.minutes, mock_gnss.seconds);
        display_draw_string(10, y_pos, buf, COLOR_BLACK, COLOR_WHITE);
        y_pos += 20;

        // FIX & SAT
        snprintf(buf, sizeof(buf), "FIX: %s   SAT: %d", mock_gnss.valid ? "3D" : "NO", mock_gnss.satellites);
        display_draw_string(10, y_pos, buf, COLOR_BLACK, COLOR_WHITE);
        y_pos += 20;

        // LAT
        int lat_deg = mock_gnss.lat_1e7 / 10000000;
        int lat_frac = (mock_gnss.lat_1e7 > 0 ? mock_gnss.lat_1e7 : -mock_gnss.lat_1e7) % 10000000;
        snprintf(buf, sizeof(buf), "LAT: %d.%07d", lat_deg, lat_frac);
        display_draw_string(10, y_pos, buf, COLOR_BLACK, COLOR_WHITE);
        y_pos += 20;

        // LON
        int lon_deg = mock_gnss.lon_1e7 / 10000000;
        int lon_frac = (mock_gnss.lon_1e7 > 0 ? mock_gnss.lon_1e7 : -mock_gnss.lon_1e7) % 10000000;
        snprintf(buf, sizeof(buf), "LON: %d.%07d", lon_deg, lon_frac);
        display_draw_string(10, y_pos, buf, COLOR_BLACK, COLOR_WHITE);
        y_pos += 20;

        // ALT
        snprintf(buf, sizeof(buf), "ALT: %d m", mock_gnss.alt_m);
        display_draw_string(10, y_pos, buf, COLOR_BLACK, COLOR_WHITE);
        y_pos += 20;

        // SPD
        int speed_kmh = (mock_gnss.speed_knots * 1852) / 100000; // knots * 1.852 km/h
        snprintf(buf, sizeof(buf), "SPD: %d km/h", speed_kmh);
        display_draw_string(10, y_pos, buf, COLOR_BLACK, COLOR_WHITE);
        }

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_UP: handle_button_press(PURRGO_BTN_UP); break;
                    case SDLK_DOWN: handle_button_press(PURRGO_BTN_DOWN); break;
                    case SDLK_LEFT: handle_button_press(PURRGO_BTN_LEFT); break;
                    case SDLK_RIGHT: handle_button_press(PURRGO_BTN_RIGHT); break;
                    case SDLK_KP_PLUS:
                    case SDLK_PLUS: handle_button_press(PURRGO_BTN_PLUS); break;
                    case SDLK_KP_MINUS:
                    case SDLK_MINUS: handle_button_press(PURRGO_BTN_MINUS); break;
                    case SDLK_m: handle_button_press(PURRGO_BTN_MENU); break;
                    case SDLK_RETURN:
                    case SDLK_RETURN2:
                    case SDLK_KP_ENTER: handle_button_press(PURRGO_BTN_OK); break;
                }
            } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int x = e.button.x;
                int y = e.button.y;
                for (size_t i = 0; i < NUM_BUTTONS; ++i) {
                    if (x >= buttons[i].rect.x && x <= buttons[i].rect.x + buttons[i].rect.w &&
                        y >= buttons[i].rect.y && y <= buttons[i].rect.y + buttons[i].rect.h) {
                        handle_button_press(buttons[i].btn_val);
                        break;
                    }
                }
            }
        }

        render_fb_to_texture(fb_texture);

        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderClear(renderer);

        SDL_Rect dest_rect = {0, 0, WINDOW_WIDTH, DISPLAY_HEIGHT * PIXEL_SCALE};
        SDL_RenderCopy(renderer, fb_texture, NULL, &dest_rect);

        // Draw buttons UI
        for (size_t i = 0; i < NUM_BUTTONS; ++i) {
            SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
            SDL_RenderFillRect(renderer, &buttons[i].rect);
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderDrawRect(renderer, &buttons[i].rect);

            int text_len = strlen(buttons[i].label);
            int text_w = text_len * 6;
            int text_h = 8;
            int text_x = buttons[i].rect.x + (buttons[i].rect.w - text_w) / 2;
            int text_y = buttons[i].rect.y + (buttons[i].rect.h - text_h) / 2;
            draw_text(renderer, text_x, text_y, buttons[i].label);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    SDL_DestroyTexture(fb_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
