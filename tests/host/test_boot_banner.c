/*
 * Загрузочный баннер (purrgo/boot_banner.h):
 *   - рисуется на весь экран с белым фоном, логотип занимает верхнюю часть;
 *   - строка версии (крупная, "v" + версия) и ревизия присутствуют;
 *   - в Debug-сборке внизу чёрная плашка "DEBUG BUILD", в Release её нет;
 *   - NULL-контекст не приводит к падению; версия и ревизия не пустые.
 */
#include <string.h>
#include "purrgo/boot_banner.h"
#include "purrgo/hardware_config.h"
#include "test_common.h"
#include "../../src/core/ui/boot_logo.h"   /* размеры логотипа (генерируются) */

#define W PURRGO_HW_DISPLAY_WIDTH_PX
#define H PURRGO_HW_DISPLAY_HEIGHT_PX

/* Раскладка баннера: должна совпадать с boot_banner.c. */
#define LOGO_X0     ((W - PURRGO_BOOT_LOGO_W) / 2)
#define LOGO_Y0     16
#define LOGO_Y1     (LOGO_Y0 + PURRGO_BOOT_LOGO_H)
#define VERSION_Y0  (LOGO_Y1 + 12)
#define VERSION_Y1  (VERSION_Y0 + 16)       /* шрифт 5x7, масштаб 2 */
#define REV_Y0      (VERSION_Y1 + 8)
#define REV_Y1      (REV_Y0 + 8)
#define BAR_Y0      (H - 16 - 16)
#define BAR_Y1      (BAR_Y0 + 16)

static uint8_t g_fb[W * H];   /* значение: цвет + 1, 0 - пиксель не рисовался */

static void cb_draw(void *fb, int16_t x, int16_t y, gfx_color_t c)
{
    (void)fb;
    if (x >= 0 && x < W && y >= 0 && y < H) { g_fb[y * W + x] = (uint8_t)(c + 1); }
}
static gfx_color_t cb_read(void *fb, int16_t x, int16_t y)
{
    (void)fb;
    return (x >= 0 && x < W && y >= 0 && y < H) ? (gfx_color_t)(g_fb[y * W + x] - 1) : 0;
}

static int count_px(int x0, int y0, int x1, int y1, gfx_color_t color)
{
    int n = 0;
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            if (g_fb[y * W + x] == color + 1) { n++; }
        }
    }
    return n;
}

int main(void)
{
    gfx_context_t gfx;
    CHECK(gfx_init(&gfx, W, H, g_fb, cb_draw, cb_read));

    purrgo_boot_banner_render(NULL);   /* не падает */

    CHECK(strlen(purrgo_boot_version()) > 0);
    CHECK(strlen(purrgo_boot_revision()) > 0);
    CHECK(PURRGO_BOOT_BANNER_MS == 500u);

    /* Неполный clip не должен мешать: баннер сбрасывает его сам. */
    gfx_set_clip(&gfx, 10, 10, 20, 20);
    purrgo_boot_banner_render(&gfx);

    /* Весь экран нарисован (ни одного нетронутого пикселя) и фон белый. */
    int untouched = 0;
    for (int i = 0; i < W * H; i++) { if (g_fb[i] == 0) { untouched++; } }
    CHECK(untouched == 0);
    CHECK(count_px(0, 0, W, LOGO_Y0, WHITE) == W * LOGO_Y0);   /* поле над логотипом */
    CHECK(count_px(0, LOGO_Y0, LOGO_X0, LOGO_Y1, WHITE) == LOGO_X0 * PURRGO_BOOT_LOGO_H); /* и по бокам */

    /* Логотип: много чёрного, есть тёмно-серый и светло-серый. */
    CHECK(count_px(0, LOGO_Y0, W, LOGO_Y1, BLACK) > 2500);
    CHECK(count_px(0, LOGO_Y0, W, LOGO_Y1, DARK_GRAY) > 300);
    CHECK(count_px(0, LOGO_Y0, W, LOGO_Y1, LIGHT_GRAY) > 10);   /* пуговица на шапке */

    /* Строка версии под логотипом: чёрные пиксели крупного текста. */
    CHECK(count_px(0, VERSION_Y0, W, VERSION_Y1, BLACK) > 30);
    /* Ревизия git: тёмно-серый текст. */
    CHECK(count_px(0, REV_Y0, W, REV_Y1 + 2, DARK_GRAY) > 10);

    /* Плашка Debug: чёрная заливка с белым текстом внутри; в Release - чисто белое поле. */
    const int bar_black = count_px(0, BAR_Y0, W, BAR_Y1, BLACK);
    if (purrgo_boot_is_debug()) {
        CHECK(bar_black > W * 16 / 2);
        CHECK(count_px(0, BAR_Y0, W, BAR_Y1, WHITE) > 40);        /* буквы */
        CHECK(count_px(0, BAR_Y0, 4, BAR_Y1, BLACK) == 4 * 16);   /* плашка во всю ширину */
    } else {
        CHECK(bar_black == 0);
    }

    /* Повторный вызов даёт тот же кадр. */
    uint8_t copy[W * H];
    memcpy(copy, g_fb, sizeof copy);
    purrgo_boot_banner_render(&gfx);
    CHECK(memcmp(copy, g_fb, sizeof copy) == 0);

    return test_finish("test_boot_banner");
}
