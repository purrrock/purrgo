#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include "display.h"
#include <purrgo/gnss_types.h>

#define PIXEL_SCALE 2
#define WINDOW_WIDTH (DISPLAY_WIDTH * PIXEL_SCALE)
#define UI_AREA_HEIGHT 200
#define WINDOW_HEIGHT (DISPLAY_HEIGHT * PIXEL_SCALE + UI_AREA_HEIGHT)

typedef struct {
    SDL_Rect rect;
    const char* label;
    int id;
} ButtonState;

ButtonState buttons[] = {
    {{10, DISPLAY_HEIGHT * PIXEL_SCALE + 10, 80, 40}, "UP", 0},
    {{10, DISPLAY_HEIGHT * PIXEL_SCALE + 60, 80, 40}, "DOWN", 1},
    {{100, DISPLAY_HEIGHT * PIXEL_SCALE + 10, 80, 40}, "LEFT", 2},
    {{100, DISPLAY_HEIGHT * PIXEL_SCALE + 60, 80, 40}, "RIGHT", 3},
    {{190, DISPLAY_HEIGHT * PIXEL_SCALE + 10, 80, 40}, "PLUS", 4},
    {{190, DISPLAY_HEIGHT * PIXEL_SCALE + 60, 80, 40}, "MINUS", 5},
    {{280, DISPLAY_HEIGHT * PIXEL_SCALE + 10, 80, 40}, "MENU", 6},
    {{280, DISPLAY_HEIGHT * PIXEL_SCALE + 60, 80, 40}, "OK", 7}
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
    // A simple text renderer for button labels could be implemented here
    // but for now, we'll just draw rectangles to indicate buttons
    // Since SDL doesn't have built-in text rendering without SDL_ttf,
    // we use our 8x8 font just by drawing points on renderer directly!
    extern char font8x8_basic[128][8];
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    int cur_x = x;
    while (*text) {
        char c = *text;
        if (c >= 0 && c < 128) {
            const char* bitmap = font8x8_basic[(int)c];
            for (int row = 0; row < 8; row++) {
                for (int col = 0; col < 8; col++) {
                    if ((bitmap[row] >> col) & 1) {
                        SDL_RenderDrawPoint(renderer, cur_x + col * 2, y + row * 2);
                        SDL_RenderDrawPoint(renderer, cur_x + col * 2 + 1, y + row * 2);
                        SDL_RenderDrawPoint(renderer, cur_x + col * 2, y + row * 2 + 1);
                        SDL_RenderDrawPoint(renderer, cur_x + col * 2 + 1, y + row * 2 + 1);
                    }
                }
            }
        }
        cur_x += 16;
        text++;
    }
}

void handle_button_press(const char* btn_name) {
    printf("Button pressed: %s\n", btn_name);
    // Print to NMEA log as requested
    char msg[32];
    snprintf(msg, sizeof(msg), "BTN: %s\n", btn_name);
    static int log_y = 0;
    display_draw_string(0, log_y, msg, COLOR_BLACK, COLOR_WHITE);
    log_y += 8;
    if (log_y >= DISPLAY_HEIGHT) {
        log_y = 0;
        display_clear(COLOR_WHITE);
    }
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

    purrgo_gnss_solution_t mock_gnss = {
        .valid = true,
        .lat_1e7 = 557558310,
        .lon_1e7 = 376173000,
        .speed_knots = 269, // 5km/h ~ 2.69 knots -> 269
        .alt_m = 150,
        .satellites = 9,
        .hours = 12,
        .minutes = 34,
        .seconds = 56,
        .day = 1,
        .month = 1,
        .year = 24
    };

    uint32_t last_update_time = SDL_GetTicks();

    bool quit = false;
    SDL_Event e;

    while (!quit) {
        uint32_t current_time = SDL_GetTicks();
        if (current_time - last_update_time >= 1000) {
            last_update_time = current_time;

            mock_gnss.seconds++;
            if (mock_gnss.seconds >= 60) {
                mock_gnss.seconds = 0;
                mock_gnss.minutes++;
                if (mock_gnss.minutes >= 60) {
                    mock_gnss.minutes = 0;
                    mock_gnss.hours = (mock_gnss.hours + 1) % 24;
                }
            }

            // Slightly increment coordinates to simulate movement
            mock_gnss.lat_1e7 += 10;
            mock_gnss.lon_1e7 += 15;
        }

        display_clear(COLOR_WHITE);

        char buf[32];
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

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_UP: handle_button_press("UP"); break;
                    case SDLK_DOWN: handle_button_press("DOWN"); break;
                    case SDLK_LEFT: handle_button_press("LEFT"); break;
                    case SDLK_RIGHT: handle_button_press("RIGHT"); break;
                    case SDLK_KP_PLUS:
                    case SDLK_PLUS: handle_button_press("PLUS"); break;
                    case SDLK_KP_MINUS:
                    case SDLK_MINUS: handle_button_press("MINUS"); break;
                    case SDLK_m: handle_button_press("MENU"); break;
                    case SDLK_RETURN:
                    case SDLK_RETURN2:
                    case SDLK_KP_ENTER: handle_button_press("OK"); break;
                }
            } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int x = e.button.x;
                int y = e.button.y;
                for (size_t i = 0; i < NUM_BUTTONS; ++i) {
                    if (x >= buttons[i].rect.x && x <= buttons[i].rect.x + buttons[i].rect.w &&
                        y >= buttons[i].rect.y && y <= buttons[i].rect.y + buttons[i].rect.h) {
                        handle_button_press(buttons[i].label);
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

            draw_text(renderer, buttons[i].rect.x + 10, buttons[i].rect.y + 10, buttons[i].label);
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
