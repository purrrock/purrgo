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

#define W PURRGO_HW_DISPLAY_WIDTH_PX
#define H PURRGO_HW_DISPLAY_HEIGHT_PX

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
    CHECK(count_px(0, 0, W, 16, WHITE) == W * 16);          /* поле над логотипом */

    /* Логотип: много чёрного, есть тёмно-серый и светло-серый. */
    CHECK(count_px(0, 16, W, 176, BLACK) > 3000);
    CHECK(count_px(0, 16, W, 176, DARK_GRAY) > 300);
    CHECK(count_px(0, 16, W, 176, LIGHT_GRAY) > 50);

    /* Строка версии под логотипом: чёрные пиксели крупного текста. */
    CHECK(count_px(0, 188, W, 204, BLACK) > 30);
    /* Ревизия git: тёмно-серый текст. */
    CHECK(count_px(0, 210, W, 220, DARK_GRAY) > 10);

    /* Плашка Debug: чёрная заливка с белым текстом внутри; в Release - чисто белое поле. */
    const int bar_black = count_px(0, 232, W, 248, BLACK);
    if (purrgo_boot_is_debug()) {
        CHECK(bar_black > W * 16 / 2);
        CHECK(count_px(0, 232, W, 248, WHITE) > 40);        /* буквы */
        CHECK(count_px(0, 232, 4, 248, BLACK) == 4 * 16);   /* плашка во всю ширину */
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
