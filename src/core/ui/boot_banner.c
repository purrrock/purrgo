#include "purrgo/boot_banner.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "boot_logo.h"
#include "purrgo/font5x7.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_text.h"
#include "purrgo/hardware_config.h"
#include "purrgo_version.h"   /* генерируется при сборке, см. cmake/PurrgoVersion.cmake */

/* Шрифт 5x7: ячейка символа 6 пикселей по горизонтали (5 + интервал), 8 по вертикали. */
#define FONT_ADVANCE_PX 6
#define FONT_HEIGHT_PX  8

#define SCREEN_W PURRGO_HW_DISPLAY_WIDTH_PX
#define SCREEN_H PURRGO_HW_DISPLAY_HEIGHT_PX

/* Раскладка баннера, пикселей от верха экрана. */
#define LOGO_Y          16
#define VERSION_SCALE   2
#define VERSION_GAP_PX  12      /* от логотипа до строки версии */
#define REVISION_GAP_PX 8       /* от строки версии до строки ревизии */
#define DEBUG_BAR_H_PX  16
#define DEBUG_BAR_Y     (SCREEN_H - DEBUG_BAR_H_PX - 16)
#define DEBUG_TEXT      "DEBUG BUILD"

/* Логотип с запасом помещается в экран по ширине. */
_Static_assert(PURRGO_BOOT_LOGO_W <= SCREEN_W, "boot logo is wider than the display");

const char *purrgo_boot_version(void)
{
    return PURRGO_VERSION_STRING;
}

const char *purrgo_boot_revision(void)
{
    return PURRGO_GIT_REVISION;
}

bool purrgo_boot_is_debug(void)
{
    return PURRGO_BUILD_IS_DEBUG != 0;
}

/* Ширина строки в пикселях при масштабе scale (без хвостового интервала). */
static int16_t text_width(const char *s, int scale)
{
    const size_t n = strlen(s);
    return (n == 0U) ? 0 : (int16_t)((int)n * FONT_ADVANCE_PX * scale - scale);
}

static int16_t centered_x(int16_t width)
{
    return (int16_t)((SCREEN_W - width) / 2);
}

/*
 * Строка шрифтом 5x7, увеличенным в scale раз (блоки scale x scale), прозрачный фон.
 * gfx_draw_string() масштаба не умеет, а для версии 1x слишком мелко.
 */
static void draw_string_scaled(gfx_context_t *gfx, int16_t x, int16_t y,
                               const char *s, int scale, gfx_color_t color)
{
    gfx_set_color(gfx, WHITE, color);   /* gfx_fill_rect() заливает цветом фона (color_bg) */
    for (; *s != '\0'; s++) {
        const unsigned char *glyph = font5x7[(unsigned char)*s];
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 8; row++) {
                if (((glyph[col] >> row) & 1) != 0) {
                    gfx_fill_rect(gfx,
                                  (int16_t)(x + col * scale), (int16_t)(y + row * scale),
                                  (int16_t)scale, (int16_t)scale);
                }
            }
        }
        x = (int16_t)(x + FONT_ADVANCE_PX * scale);
    }
}

static void draw_logo(gfx_context_t *gfx, int16_t x0, int16_t y0)
{
    for (unsigned y = 0; y < PURRGO_BOOT_LOGO_H; y++) {
        const uint8_t *row = &purrgo_boot_logo[y * (PURRGO_BOOT_LOGO_W / 4)];
        for (unsigned x = 0; x < PURRGO_BOOT_LOGO_W; x++) {
            const unsigned packed = row[x >> 2];
            const gfx_color_t c = (gfx_color_t)((packed >> ((3u - (x & 3u)) * 2u)) & 0x03u);
            if (c != WHITE) {   /* фон уже залит белым */
                gfx_set_color(gfx, c, WHITE);
                gfx_draw_pixel(gfx, (int16_t)(x0 + (int)x), (int16_t)(y0 + (int)y));
            }
        }
    }
}

void purrgo_boot_banner_render(gfx_context_t *gfx)
{
    if (gfx == NULL) {
        return;
    }

    gfx_reset_clip(gfx);
    gfx_set_color(gfx, BLACK, WHITE);
    gfx_clear(gfx);

    draw_logo(gfx, (int16_t)((SCREEN_W - PURRGO_BOOT_LOGO_W) / 2), LOGO_Y);

    /* "v0.1.0" крупно. */
    char version[24];
    (void)snprintf(version, sizeof(version), "v%s", PURRGO_VERSION_STRING);
    const int16_t version_y = (int16_t)(LOGO_Y + PURRGO_BOOT_LOGO_H + VERSION_GAP_PX);
    draw_string_scaled(gfx, centered_x(text_width(version, VERSION_SCALE)), version_y,
                       version, VERSION_SCALE, BLACK);

    /* Ревизия git мелко, тёмно-серым. */
    const char *rev = PURRGO_GIT_REVISION;
    const int16_t rev_y = (int16_t)(version_y + FONT_HEIGHT_PX * VERSION_SCALE + REVISION_GAP_PX);
    gfx_set_color(gfx, DARK_GRAY, WHITE);
    gfx_draw_string(gfx, centered_x(text_width(rev, 1)), rev_y, rev);

    /* Debug-сборка: чёрная плашка с белым текстом. */
    if (purrgo_boot_is_debug()) {
        gfx_set_color(gfx, WHITE, BLACK);
        gfx_fill_rect(gfx, 0, DEBUG_BAR_Y, SCREEN_W, DEBUG_BAR_H_PX);
        gfx_draw_string(gfx, centered_x(text_width(DEBUG_TEXT, 1)),
                        (int16_t)(DEBUG_BAR_Y + (DEBUG_BAR_H_PX - 7) / 2), DEBUG_TEXT);
    }

    gfx_set_color(gfx, BLACK, WHITE);
}
