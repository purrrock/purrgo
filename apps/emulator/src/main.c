#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "display.h"
#include <purrgo/gnss_types.h>
#include "purrgo/track_logger.h"
#include <purrgo/gnss_io.h>
#include <purrgo/gnss.h>
#include <purrgo/gnss_adapter.h>

#ifdef USE_MOCK_GNSS
#include <purrgo/pc_gnss_mock.h>
#else
#include "purrgo/gnss_adapter.h"
#include "serial_hal.h"
#endif

#include <purrgo/app_fsm.h>
#include <purrgo/gfx_line.h>
#include <purrgo/gfx_rect.h>
#include <purrgo/gfx_circle.h>
#include <purrgo/gfx_text.h>
#include <purrgo/config.h>
#include "purrgo/gfx_renderer.h"

#include <purrgo/sun.h>
#include <purrgo/map.h>
#include <purrgo/fs_hal.h>

#include "emu_window.h"
#include <purrgo/app_ui.h>

/*
 * Период обновления E-Ink framebuffer.
 *
 * В emulator это только частота обновления изображения на экране.
 */
#define EINK_REFRESH_PERIOD_MS 333

static gfx_context_t global_gfx_ctx;

extern int dbg_map_render_calls;

/*
 * Callback GFX -> framebuffer emulator.
 *
 * GFX не знает об SDL. Он вызывает этот callback для изменения пикселя,
 * а display_set_pixel() записывает значение в framebuffer дисплея.
 */
static void emulator_draw_pixel_cb(
    void *fb,
    int16_t x,
    int16_t y,
    gfx_color_t color
) {
    (void)fb;
    display_set_pixel(x, y, color);
}

static gfx_color_t emulator_read_pixel_cb(
    void *fb,
    int16_t x,
    int16_t y
) {
    (void)fb;
    return display_get_pixel(x, y);
}

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* fb_texture = NULL;

static bool emulator_sys_init(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        fprintf(
            stderr,
            "SDL could not initialize! SDL_Error: %s\n",
            SDL_GetError()
        );
        return false;
    }

    if (!emu_window_init(&window, &renderer, &fb_texture)) {
        if (fb_texture) SDL_DestroyTexture(fb_texture);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    display_init();

#ifdef USE_MOCK_GNSS
    pc_gnss_mock_init();
#else
    if (argc > 1) {
        if (!serial_hal_open(argv[1], 9600)) {
            fprintf(
                stderr,
                "Failed to open serial port: %s\n",
                argv[1]
            );
            return false;
        }
    } else {
        fprintf(
            stderr,
            "Usage: %s <COM_PORT>\n",
            argv[0]
        );
        return false;
    }
#endif

    purrgo_app_init();

    gfx_init(
        &global_gfx_ctx,
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT,
        (void*)1,
        emulator_draw_pixel_cb,
        emulator_read_pixel_cb
    );

    return true;
}

static void emulator_run_loop(void) {
    uint32_t last_gnss_time = SDL_GetTicks();
    uint32_t last_eink_refresh = 0;

    bool quit = false;

    while (!quit) {
        uint32_t current_time = SDL_GetTicks();

        if (current_time - last_gnss_time >= 1000) {
            last_gnss_time = current_time;
#ifdef USE_MOCK_GNSS
            pc_gnss_mock_update();
#endif
        }

        /*
         * 1. Обновление FSM/GNSS.
         */
        purrgo_app_tick(current_time);

        /*
         * 2. Отрисовка framebuffer с частотой 3 FPS.
         *
         * Парсинг и отрисовка карты должны выполняться только если
         * карта помечена как dirty в APP_STATE_MAP.
         */
        if (current_time - last_eink_refresh >= EINK_REFRESH_PERIOD_MS) {
            last_eink_refresh = current_time;

            if (purrgo_app_ui_is_dirty() || purrgo_app_map_is_dirty()) {
                int last_calls = dbg_map_render_calls;

                purrgo_app_ui_render(&global_gfx_ctx, purrgo_app_get_gnss_solution(), purrgo_app_get_sun_info());
                purrgo_app_ui_clear_dirty();

                if (last_calls != dbg_map_render_calls) {
                    printf("MAP RENDER EXECUTED. Total renders: %d\n", dbg_map_render_calls);
                }

                emu_window_render(renderer, fb_texture);
            }
        }

        /*
         * 3. Обработка SDL events.
         */
        emu_window_process_events(&quit);

        /*
         * Небольшая задержка разгружает CPU, но не влияет на
         * E-Ink refresh period.
         */
        SDL_Delay(10);
    }
}

static void emulator_cleanup(void) {
    // Корректно закрываем GPX-файл, сбрасываем буфер и пишем закрывающие теги
    purrgo_logger_stop();
    // ============================
    if (fb_texture) SDL_DestroyTexture(fb_texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    if (!emulator_sys_init(argc, argv)) {
        return 1;
    }

    emulator_run_loop();
    emulator_cleanup();

    return 0;
}
