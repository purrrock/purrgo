#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "display.h"
#include <purrgo/gnss_types.h>

#ifdef USE_MOCK_GNSS
#include <purrgo/gnss_mock.h>
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
#include "purrgo/gfx_text.h"
#include "purrgo/gfx_renderer.h"

#include <purrgo/sun.h>
#include <purrgo/map.h>
#include <purrgo/fs_hal.h>

#define PIXEL_SCALE 2
#define WINDOW_WIDTH (DISPLAY_WIDTH * PIXEL_SCALE)
#define UI_AREA_HEIGHT 100
#define WINDOW_HEIGHT (DISPLAY_HEIGHT * PIXEL_SCALE + UI_AREA_HEIGHT)

/*
 * Период обновления E-Ink framebuffer.
 *
 * В emulator это только частота обновления изображения на экране.
 * Сам map rendering ниже выполняется один раз при входе в APP_STATE_MAP,
 * а не при каждом refresh.
 */
#define EINK_REFRESH_PERIOD_MS 333

static gfx_context_t global_gfx_ctx;

/*
 * Указывает, был ли уже выполнен map rendering для текущего входа
 * в APP_STATE_MAP.
 *
 * Это диагностический режим: пока мы исследуем map pipeline, нет смысла
 * повторно открывать IDX/MLP и полностью обходить SQT на каждом кадре.
 *
 * При выходе из APP_STATE_MAP флаг сбрасывается, поэтому при следующем
 * входе карта будет построена заново.
 */
static bool map_rendered = false;

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
 *
 * framebuffer хранит четыре значения цвета:
 *
 *     0 = black
 *     1 = dark gray
 *     2 = light gray
 *     3 = white
 *
 * Emulator переводит их в ARGB8888 для SDL texture.
 */
uint32_t palette[4] = {
    0xFF000000,
    0xFF555555,
    0xFFAAAAAA,
    0xFFFFFFFF
};

/*
 * Копирует framebuffer эмулятора в SDL texture.
 *
 * В framebuffer на один пиксель приходится 2 бита, поэтому четыре
 * пикселя находятся в одном байте.
 */
void render_fb_to_texture(SDL_Texture* texture) {
    const uint8_t* fb = display_get_framebuffer();

    uint32_t pixels[DISPLAY_WIDTH * DISPLAY_HEIGHT];

    for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
        int byte_idx = i / 4;
        int bit_shift = (3 - (i % 4)) * 2;

        uint8_t color_val =
            (fb[byte_idx] >> bit_shift) & 0x03;

        pixels[i] = palette[color_val];
    }

    SDL_UpdateTexture(
        texture,
        NULL,
        pixels,
        DISPLAY_WIDTH * sizeof(uint32_t)
    );
}

/*
 * FS adapter для emulator.
 *
 * map parser работает через абстракцию purrgo_fs_t, поэтому ему не
 * требуется знать, что в emulator файлы открываются через host FS.
 */
static uint32_t emu_fs_read(
    void* handle,
    void* buffer,
    uint32_t size
) {
    return purrgo_fs_read(
        (purrgo_file_t*)handle,
        (uint8_t*)buffer,
        size
    );
}

static bool emu_fs_seek(
    void* handle,
    uint32_t offset
) {
    return purrgo_fs_seek(
        (purrgo_file_t*)handle,
        offset
    );
}

/*
 * Рисование текста непосредственно средствами SDL.
 *
 * Это текст UI самого emulator, а не framebuffer PurrGo.
 */
void sdl_draw_text(
    SDL_Renderer* renderer,
    int x,
    int y,
    const char* text
) {
    extern const unsigned char font5x7[256][5];

    SDL_SetRenderDrawColor(
        renderer,
        0, 0, 0, 255
    );

    int cur_x = x;

    while (*text) {
        unsigned char c = (unsigned char)*text;
        const unsigned char* bitmap = font5x7[c];

        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 8; row++) {
                if ((bitmap[col] >> row) & 1) {
                    SDL_RenderDrawPoint(
                        renderer,
                        cur_x + col,
                        y + row
                    );
                }
            }
        }

        cur_x += 6;
        text++;
    }
}

/*
 * Центральная точка обработки кнопки emulator.
 *
 * Логируем состояние до и после передачи события в application FSM.
 * Это позволяет увидеть, действительно ли нажатие вызывает переход
 * в APP_STATE_MAP и обратный переход из него.
 */
void handle_button_press(purrgo_btn_t btn_val) {
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

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        fprintf(
            stderr,
            "SDL could not initialize! SDL_Error: %s\n",
            SDL_GetError()
        );
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "PurrGo Emulator (E-Ink 3 FPS)",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        fprintf(
            stderr,
            "Window could not be created! SDL_Error: %s\n",
            SDL_GetError()
        );
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer =
        SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_ACCELERATED
        );

    if (!renderer) {
        fprintf(
            stderr,
            "Renderer could not be created! SDL_Error: %s\n",
            SDL_GetError()
        );
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* fb_texture =
        SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            DISPLAY_WIDTH,
            DISPLAY_HEIGHT
        );

    if (!fb_texture) {
        fprintf(
            stderr,
            "Texture could not be created! SDL_Error: %s\n",
            SDL_GetError()
        );
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    display_init();

    purrgo_gnss_solution_t gnss_solution;

#ifdef USE_MOCK_GNSS
    purrgo_gnss_mock_init(&gnss_solution);
#else
    memset(
        &gnss_solution,
        0,
        sizeof(gnss_solution)
    );

    if (argc > 1) {
        if (!serial_hal_open(argv[1], 9600)) {
            fprintf(
                stderr,
                "Failed to open serial port: %s\n",
                argv[1]
            );
            return 1;
        }
    } else {
        fprintf(
            stderr,
            "Usage: %s <COM_PORT>\n",
            argv[0]
        );
        return 1;
    }
#endif

    purrgo_app_init();

    gfx_init(
        &global_gfx_ctx,
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT,
        (void*)1,
        emulator_draw_pixel_cb
    );

    /*
     * Фиксированная camera для текущего теста roads.
     *
     * Центр соответствует map.name:
     *
     *     lon = 28.40848
     *     lat = 53.523514
     *
     * Внутренний масштаб координат = 10^7.
     */
    purrgo_bbox_t fixed_cam = {
        .min_x = 284084800 - 15000,
        .min_y = 535235140 - 20000,
        .max_x = 284084800 + 15000,
        .max_y = 535235140 + 20000
    };

    /*
     * Карта занимает центральную область вертикального дисплея.
     *
     * Верхняя и нижняя области оставлены под служебный UI.
     */
    purrgo_viewport_t map_vp = {
        .width = DISPLAY_WIDTH - 10,
        .height = DISPLAY_HEIGHT - 30,
        .offset_x = 5,
        .offset_y = 15
    };

    fprintf(
        stderr,
        "EMU: map camera min=(%ld,%ld) max=(%ld,%ld), "
        "viewport=(%d,%d,%u,%u)\n",
        (long)fixed_cam.min_x,
        (long)fixed_cam.min_y,
        (long)fixed_cam.max_x,
        (long)fixed_cam.max_y,
        map_vp.offset_x,
        map_vp.offset_y,
        map_vp.width,
        map_vp.height
    );
    fflush(stderr);

    purrgo_sun_info_t sun_info;
    memset(&sun_info, 0, sizeof(sun_info));

    uint32_t last_sun_update = 0;
    bool sun_initialized = false;
    bool first_fix_obtained = false;

    uint32_t last_gnss_time = SDL_GetTicks();
    uint32_t last_eink_refresh = 0;

    bool quit = false;
    SDL_Event e;

    while (!quit) {
        uint32_t current_time = SDL_GetTicks();

        /*
         * 1. Обновление GNSS.
         *
         * Этот участок Stage 1 не изменяется.
         */
#ifndef USE_MOCK_GNSS
        {
            static char line_buffer[128];
            static uint16_t line_pos = 0;

            uint8_t rx_byte;
            int bytes_processed = 0;

            while (
                serial_hal_read_byte(&rx_byte) == 1 &&
                bytes_processed < 256
            ) {
                if (line_pos < sizeof(line_buffer) - 1) {
                    line_buffer[line_pos++] = (char)rx_byte;
                } else {
                    line_pos = 0;
                }

                if (rx_byte == '\n') {
                    line_buffer[line_pos] = '\0';

                    purrgo_gnss_process_nmea(
                        line_buffer,
                        &gnss_solution
                    );

                    line_pos = 0;
                }

                bytes_processed++;
            }
        }
#endif

        if (current_time - last_gnss_time >= 1000) {
            last_gnss_time = current_time;

#ifdef USE_MOCK_GNSS
            purrgo_gnss_mock_update(&gnss_solution);
#endif

            purrgo_app_update(&gnss_solution);

            if (gnss_solution.valid) {
                if (!first_fix_obtained) {
                    first_fix_obtained = true;

                    purrgo_sun_calc(
                        gnss_solution.lat_1e7,
                        gnss_solution.lon_1e7,
                        gnss_solution.year % 100,
                        gnss_solution.month,
                        gnss_solution.day,
                        gnss_solution.hours,
                        gnss_solution.minutes,
                        app_config.tz_offset_minutes,
                        &sun_info
                    );

                    sun_initialized = true;
                    last_sun_update = current_time;
                } else if (
                    current_time - last_sun_update >= 60000
                ) {
                    purrgo_sun_calc(
                        gnss_solution.lat_1e7,
                        gnss_solution.lon_1e7,
                        gnss_solution.year % 100,
                        gnss_solution.month,
                        gnss_solution.day,
                        gnss_solution.hours,
                        gnss_solution.minutes,
                        app_config.tz_offset_minutes,
                        &sun_info
                    );

                    last_sun_update = current_time;
                }
            }
        }

        /*
         * 2. Отрисовка framebuffer с частотой 3 FPS.
         *
         * ВАЖНО:
         *
         * display_clear() выполняется каждый refresh.
         * Поэтому если карта была нарисована только один раз, её нельзя
         * просто оставить во framebuffer: следующий display_clear()
         * сотрёт её.
         *
         * Поэтому map_rendered означает не "карта больше никогда не
         * рисуется", а "не нужно повторно парсить карту на каждом кадре".
         *
         * Для корректного сохранения изображения карту нужно рисовать
         * после очистки framebuffer при каждом кадре. На данном этапе
         * оставляем render-once только как диагностический режим.
         */
        if (
            current_time - last_eink_refresh >=
            EINK_REFRESH_PERIOD_MS
        ) {
            last_eink_refresh = current_time;

            display_clear(COLOR_WHITE);

            char buf[64];

            switch (purrgo_app_get_state()) {
                case APP_STATE_MENU_CONFIG: {
                    gfx_set_color(
                        &global_gfx_ctx,
                        COLOR_BLACK,
                        COLOR_WHITE
                    );

                    int16_t draft_tz =
                        purrgo_app_get_draft_tz_offset();

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        10,
                        "=== CONFIG ==="
                    );

                    char sign =
                        (draft_tz >= 0) ? '+' : '-';

                    int16_t abs_tz =
                        (draft_tz >= 0)
                            ? draft_tz
                            : -draft_tz;

                    int hours = abs_tz / 60;
                    int mins = abs_tz % 60;

                    snprintf(
                        buf,
                        sizeof(buf),
                        "TZ: UTC%c%02d:%02d",
                        sign,
                        hours,
                        mins
                    );

                    gfx_set_color(
                        &global_gfx_ctx,
                        COLOR_WHITE,
                        COLOR_BLACK
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        25,
                        buf
                    );

                    gfx_set_color(
                        &global_gfx_ctx,
                        COLOR_BLACK,
                        COLOR_WHITE
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        45,
                        "+/- : Change"
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        60,
                        "OK  : Save"
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        75,
                        "MENU: Cancel"
                    );

                    break;
                }

                case APP_STATE_SATELLITES: {
                    int y_pos = 10;

                    snprintf(
                        buf,
                        sizeof(buf),
                        "UTC: %02d:%02d:%02d",
                        gnss_solution.hours,
                        gnss_solution.minutes,
                        gnss_solution.seconds
                    );

                    gfx_set_color(
                        &global_gfx_ctx,
                        COLOR_BLACK,
                        COLOR_WHITE
                    );

                    y_pos += 12;

                    int32_t total_mins =
                        (int32_t)gnss_solution.hours * 60 +
                        (int32_t)gnss_solution.minutes +
                        app_config.tz_offset_minutes;

                    while (total_mins < 0)
                        total_mins += 1440;

                    while (total_mins >= 1440)
                        total_mins -= 1440;

                    uint8_t loc_hours =
                        (uint8_t)(total_mins / 60);

                    uint8_t loc_minutes =
                        (uint8_t)(total_mins % 60);

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        y_pos,
                        buf
                    );

                    snprintf(
                        buf,
                        sizeof(buf),
                        "LOC: %02d:%02d:%02d",
                        loc_hours,
                        loc_minutes,
                        gnss_solution.seconds
                    );

                    gfx_set_color(
                        &global_gfx_ctx,
                        COLOR_BLACK,
                        COLOR_WHITE
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        y_pos,
                        buf
                    );

                    y_pos += 12;

                    snprintf(
                        buf,
                        sizeof(buf),
                        "FIX: %s   SAT: %d",
                        gnss_solution.valid
                            ? "3D"
                            : "NO",
                        gnss_solution.satellites_tracked
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        y_pos,
                        buf
                    );

                    y_pos += 12;

                    int lat_deg =
                        gnss_solution.lat_1e7 / 10000000;

                    int lat_frac =
                        (gnss_solution.lat_1e7 > 0
                            ? gnss_solution.lat_1e7
                            : -gnss_solution.lat_1e7)
                        % 10000000;

                    snprintf(
                        buf,
                        sizeof(buf),
                        "LAT: %d.%07d",
                        lat_deg,
                        lat_frac
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        y_pos,
                        buf
                    );

                    y_pos += 12;

                    int lon_deg =
                        gnss_solution.lon_1e7 / 10000000;

                    int lon_frac =
                        (gnss_solution.lon_1e7 > 0
                            ? gnss_solution.lon_1e7
                            : -gnss_solution.lon_1e7)
                        % 10000000;

                    snprintf(
                        buf,
                        sizeof(buf),
                        "LON: %d.%07d",
                        lon_deg,
                        lon_frac
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        y_pos,
                        buf
                    );

                    y_pos += 12;

                    snprintf(
                        buf,
                        sizeof(buf),
                        "ALT: %d m",
                        gnss_solution.alt_m
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        y_pos,
                        buf
                    );

                    y_pos += 12;

                    int speed_kmh =
                        (gnss_solution.speed_knots * 1852)
                        / 100000;

                    snprintf(
                        buf,
                        sizeof(buf),
                        "SPD: %d km/h",
                        speed_kmh
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        y_pos,
                        buf
                    );

                    y_pos += 12;

                    if (sun_initialized) {
                        if (
                            sun_info.status ==
                            SUN_STATUS_NORMAL
                        ) {
                            snprintf(
                                buf,
                                sizeof(buf),
                                "SUN %02d:%02d-%02d:%02d",
                                sun_info.sunrise_hour,
                                sun_info.sunrise_minute,
                                sun_info.sunset_hour,
                                sun_info.sunset_minute
                            );

                            gfx_draw_string(
                                &global_gfx_ctx,
                                10,
                                y_pos,
                                buf
                            );

                            y_pos += 12;

                            int remain_h =
                                sun_info.time_to_event_min / 60;

                            int remain_m =
                                sun_info.time_to_event_min % 60;

                            if (sun_info.is_daytime) {
                                snprintf(
                                    buf,
                                    sizeof(buf),
                                    "TO SUNSET: %02dh %02dm",
                                    remain_h,
                                    remain_m
                                );
                            } else {
                                snprintf(
                                    buf,
                                    sizeof(buf),
                                    "TO SUNRISE: %02dh %02dm",
                                    remain_h,
                                    remain_m
                                );
                            }

                            gfx_draw_string(
                                &global_gfx_ctx,
                                10,
                                y_pos,
                                buf
                            );

                            y_pos += 12;

                            int start_min =
                                sun_info.sunrise_hour * 60 +
                                sun_info.sunrise_minute;

                            int end_min =
                                sun_info.sunset_hour * 60 +
                                sun_info.sunset_minute;

                            int total_day_min =
                                end_min - start_min;

                            if (total_day_min < 0)
                                total_day_min += 1440;

                            snprintf(
                                buf,
                                sizeof(buf),
                                "DAY: %02dh %02dm",
                                total_day_min / 60,
                                total_day_min % 60
                            );

                            gfx_draw_string(
                                &global_gfx_ctx,
                                10,
                                y_pos,
                                buf
                            );

                            y_pos += 12;
                        } else if (
                            sun_info.status ==
                            SUN_STATUS_POLAR_DAY
                        ) {
                            gfx_draw_string(
                                &global_gfx_ctx,
                                10,
                                y_pos,
                                "POLAR DAY"
                            );

                            y_pos += 12;

                            gfx_draw_string(
                                &global_gfx_ctx,
                                10,
                                y_pos,
                                "NO SUNSET DAY: 24h 00m"
                            );

                            y_pos += 12;
                        } else if (
                            sun_info.status ==
                            SUN_STATUS_POLAR_NIGHT
                        ) {
                            gfx_draw_string(
                                &global_gfx_ctx,
                                10,
                                y_pos,
                                "POLAR NIGHT"
                            );

                            y_pos += 12;

                            gfx_draw_string(
                                &global_gfx_ctx,
                                10,
                                y_pos,
                                "NO SUNRISE DAY: 00h 00m"
                            );

                            y_pos += 12;
                        }
                    }

                    break;
                }

                case APP_STATE_MAP: {
                    /*
                     * Этот лог показывает только первый вход/render.
                     * Он не должен печататься на каждом 3 FPS refresh.
                     */
                    static bool map_screen_logged = false;

                    if (!map_screen_logged) {
                        fprintf(
                            stderr,
                            "EMU: APP_STATE_MAP rendering started\n"
                        );
                        fflush(stderr);

                        map_screen_logged = true;
                    }

                    /*
                     * Верхняя служебная строка.
                     */
                    gfx_set_color(
                        &global_gfx_ctx,
                        COLOR_BLACK,
                        COLOR_WHITE
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        5,
                        5,
                        "TOP STATUS AREA"
                    );

                    /*
                     * Пока используется только roads layer.
                     *
                     * Пути вычислены относительно текущего working
                     * directory:
                     *
                     *     build/apps/emulator
                     *
                     * поэтому четыре ".." приводят в корень repository.
                     */
                    const char* idx_path =
                        "../../../tests/data/maps/roads.idx";

                    const char* mlp_path =
                        "../../../tests/data/maps/roads.mlp";

                    const char* name_path =
                        "../../../tests/data/maps/map.name";

                    /*
                     * Парсить карту нужно только один раз после входа
                     * в APP_STATE_MAP.
                     *
                     * ВАЖНО: display_clear() всё равно очищает framebuffer
                     * перед каждым refresh. Поэтому этот флаг сейчас
                     * используется именно для диагностики количества
                     * parser calls. Для постоянного отображения карты
                     * render-once недостаточно — карту потребуется
                     * повторно рисовать после каждого framebuffer clear
                     * либо изменить lifecycle framebuffer.
                     */
                    if (!map_rendered) {
                        fprintf(
                            stderr,
                            "EMU: MAP roads idx=%s mlp=%s\n",
                            idx_path,
                            mlp_path
                        );
                        fflush(stderr);

                        purrgo_file_t* idx_file =
                            purrgo_fs_open(
                                idx_path,
                                FS_READ
                            );

                        purrgo_file_t* mlp_file =
                            purrgo_fs_open(
                                mlp_path,
                                FS_READ
                            );

                        fprintf(
                            stderr,
                            "EMU: roads.idx %s, roads.mlp %s\n",
                            idx_file ? "OPEN" : "FAILED",
                            mlp_file ? "OPEN" : "FAILED"
                        );
                        fflush(stderr);

                        if (idx_file && mlp_file) {
                            purrgo_fs_t idx_fs = {
                                .handle = idx_file,
                                .read = emu_fs_read,
                                .seek = emu_fs_seek
                            };

                            purrgo_fs_t mlp_fs = {
                                .handle = mlp_file,
                                .read = emu_fs_read,
                                .seek = emu_fs_seek
                            };

                            gfx_set_color(
                                &global_gfx_ctx,
                                COLOR_BLACK,
                                COLOR_WHITE
                            );

                            fprintf(
                                stderr,
                                "EMU: calling "
                                "purrgo_map_render_layer()\n"
                            );
                            fflush(stderr);

                            purrgo_map_render_layer(
                                &idx_fs,
                                &mlp_fs,
                                &global_gfx_ctx,
                                &fixed_cam,
                                &map_vp
                            );

                            fprintf(
                                stderr,
                                "EMU: purrgo_map_render_layer() returned\n"
                            );
                            fflush(stderr);

                            /*
                             * Файл больше не нужен после полного
                             * прохождения map parser.
                             */
                            purrgo_fs_close(idx_file);
                            purrgo_fs_close(mlp_file);

                            map_rendered = true;
                        } else {
                            if (idx_file) {
                                purrgo_fs_close(idx_file);
                            }

                            if (mlp_file) {
                                purrgo_fs_close(mlp_file);
                            }
                        }
                    }

                    /*
                     * map.name нужен только для диагностики текущей
                     * тестовой карты.
                     */
                    if (!map_rendered) {
                        purrgo_file_t* name_file =
                            purrgo_fs_open(
                                name_path,
                                FS_READ
                            );

                        if (name_file) {
                            char name_buf[65];

                            uint32_t n =
                                purrgo_fs_read(
                                    name_file,
                                    (uint8_t*)name_buf,
                                    64
                                );

                            if (n > 64)
                                n = 64;

                            name_buf[n] = '\0';

                            fprintf(
                                stderr,
                                "EMU: map.name='%s'\n",
                                name_buf
                            );
                            fflush(stderr);

                            purrgo_fs_close(name_file);
                        } else {
                            fprintf(
                                stderr,
                                "EMU: map.name FAILED\n"
                            );
                            fflush(stderr);
                        }
                    }

                    gfx_set_color(
                        &global_gfx_ctx,
                        COLOR_BLACK,
                        COLOR_WHITE
                    );

                    break;
                }

                case APP_STATE_COMPASS:
                    gfx_set_color(
                        &global_gfx_ctx,
                        COLOR_BLACK,
                        COLOR_WHITE
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        10,
                        "=== COMPASS ==="
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        50,
                        "[ Bearing Pointer ]"
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        65,
                        "[   Placeholder   ]"
                    );

                    break;

                case APP_STATE_TRIP_COMPUTER:
                    gfx_set_color(
                        &global_gfx_ctx,
                        COLOR_BLACK,
                        COLOR_WHITE
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        10,
                        "=== TRIP COMP ==="
                    );

                    gfx_draw_string(
                        &global_gfx_ctx,
                        10,
                        50,
                        "[ Odometer Stats ]"
                    );

                    break;

                default:
                    break;
            }

            render_fb_to_texture(fb_texture);
        }

        /*
         * 3. Обработка SDL events.
         *
         * Все нажатия проходят через handle_button_press(), где
         * дополнительно логируются state_before/state_after.
         */
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
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
                        handle_button_press(
                            buttons[i].btn_val
                        );

                        break;
                    }
                }
            }
        }

        /*
         * 4. Отрисовка самого окна emulator.
         *
         * Здесь SDL только показывает уже готовый PurrGo framebuffer.
         * Карта не рисуется напрямую через SDL.
         */
        SDL_SetRenderDrawColor(
            renderer,
            200,
            200,
            200,
            255
        );

        SDL_RenderClear(renderer);

        SDL_Rect dest_rect = {
            0,
            0,
            WINDOW_WIDTH,
            DISPLAY_HEIGHT * PIXEL_SCALE
        };

        SDL_RenderCopy(
            renderer,
            fb_texture,
            NULL,
            &dest_rect
        );

        /*
         * Рисуем виртуальные кнопки emulator.
         */
        for (size_t i = 0; i < NUM_BUTTONS; ++i) {
            SDL_SetRenderDrawColor(
                renderer,
                150,
                150,
                150,
                255
            );

            SDL_RenderFillRect(
                renderer,
                &buttons[i].rect
            );

            SDL_SetRenderDrawColor(
                renderer,
                50,
                50,
                50,
                255
            );

            SDL_RenderDrawRect(
                renderer,
                &buttons[i].rect
            );

            int text_len =
                (int)strlen(buttons[i].label);

            int text_w = text_len * 6;
            int text_h = 8;

            int text_x =
                buttons[i].rect.x +
                (buttons[i].rect.w - text_w) / 2;

            int text_y =
                buttons[i].rect.y +
                (buttons[i].rect.h - text_h) / 2;

            sdl_draw_text(
                renderer,
                text_x,
                text_y,
                buttons[i].label
            );
        }

        SDL_RenderPresent(renderer);

        /*
         * Небольшая задержка разгружает CPU, но не влияет на
         * E-Ink refresh period.
         */
        SDL_Delay(10);
    }

    SDL_DestroyTexture(fb_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
