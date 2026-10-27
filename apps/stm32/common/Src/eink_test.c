#include "eink_test.h"

#include "main.h"
#include "display_eink.h"
#include "epd4g.h"
#include "epd4g_bw.h"

#include <stddef.h>

/* Геометрия панели (epd4g): 176 x 264, 2 бита на пиксель = 11616 байт. */
#define EINK_TEST_WIDTH   EPD4G_WIDTH
#define EINK_TEST_HEIGHT   EPD4G_HEIGHT
#define EINK_TEST_FB  EPD4G_FB_BYTES

/*
 * Тест 1-bit режима библиотеки напрямую (без слоя display_eink):
 * чередующиеся горизонтальные полосы, 1 бит на пиксель (1 = белый).
 * Проверяет только передачу данных и DC/CS/SPI, SPI-скорость выставляет
 * display_eink_init().
 */
void EinkTest_Run(void)
{
    static uint8_t image[EPD4G_BW_FB_BYTES];

    display_eink_init(NULL);

    for (uint32_t y = 0; y < EINK_TEST_HEIGHT; y++)
    {
        const uint8_t value = ((y / 32U) & 1U) ? 0x00U : 0xFFU;

        for (uint32_t x = 0; x < EPD4G_BW_STRIDE; x++)
        {
            image[y * EPD4G_BW_STRIDE + x] = value;
        }
    }

    if (epd4g_bw_init(EPD4G_BW_NORMAL) == EPD4G_OK)
    {
        (void)epd4g_bw_full(image, EPD4G_SRC_1BPP, EPD4G_BW_NORMAL);
    }
    (void)epd4g_sleep();
}


/*
 * Логические цвета PurrGO: 0 = чёрный, 1 = тёмно-серый,
 * 2 = светло-серый, 3 = белый.
 */
static void eink_set_px2(uint8_t *fb, int width, int x, int y, uint8_t c)
{
    int idx   = y * width + x;
    int shift = (3 - (idx & 3)) << 1;

    fb[idx >> 2] = (uint8_t)((fb[idx >> 2] & ~(0x03 << shift)) | ((c & 0x03) << shift));
}

void EinkTest_FillPattern4Gray(uint8_t *fb2bpp, int width, int height)
{
    const int band_h = 24;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            uint8_t c;

            if (y < band_h * 4)
            {
                /* 4 сплошные полосы: проверка всех уровней серого. */
                c = (uint8_t)(y / band_h);
            }
            else if (y < band_h * 5)
            {
                /* Вертикальные полосы 1 px: максимальная плотность переходов. */
                c = (x & 1) ? 3U : 0U;
            }
            else if (y < band_h * 6)
            {
                /* Шахматка 2x2. */
                c = (((x >> 1) + (y >> 1)) & 1) ? 3U : 0U;
            }
            else if (y < band_h * 7)
            {
                /* Горизонтальный градиент из 4 уровней. */
                c = (uint8_t)((x * 4) / width);
            }
            else
            {
                /* Рамка по краю экрана, остальное белое. */
                c = (x < 2 || y < 2 || x >= width - 2 || y >= height - 2) ? 0U : 3U;
            }

            eink_set_px2(fb2bpp, width, x, y, c);
        }
    }
}

void EinkTest_Run4Gray(void)
{
    static uint8_t fb[EINK_TEST_FB];

    EinkTest_FillPattern4Gray(fb, EINK_TEST_WIDTH, EINK_TEST_HEIGHT);

    /* Тот же путь, что у приложения: очередь display_eink -> epd4g. */
    display_eink_init(fb);
    display_eink_set_mode(DISPLAY_MODE_GRAY4);
    display_eink_request_full();
    display_eink_sleep();                /* дожидается обновления и усыпляет панель */
}


static void eink_test_square(uint8_t *fb, int x0, int y0, uint8_t c)
{
    for (int y = y0; y < y0 + 24; y++)
    {
        for (int x = x0; x < x0 + 24; x++)
        {
            eink_set_px2(fb, EINK_TEST_WIDTH, x, y, c);
        }
    }
}

void EinkTest_RunPartial(void)
{
    static uint8_t fb[EINK_TEST_FB];
    const int w = EINK_TEST_WIDTH;
    const int h = EINK_TEST_HEIGHT;

    /* Белый фон (цвет 3 во всех 4 позициях байта) и рамка. */
    for (unsigned i = 0; i < sizeof(fb); i++)
    {
        fb[i] = 0xFFU;
    }
    for (int x = 0; x < w; x++)
    {
        eink_set_px2(fb, w, x, 0, 0);
        eink_set_px2(fb, w, x, h - 1, 0);
    }
    for (int y = 0; y < h; y++)
    {
        eink_set_px2(fb, w, 0, y, 0);
        eink_set_px2(fb, w, w - 1, y, 0);
    }

    display_eink_init(fb);

    /* 1. Режим 1 bit: полное обновление, затем 4 квадрата частичным (по окну). */
    display_eink_set_mode(DISPLAY_MODE_MONO);
    display_eink_request_full();
    display_eink_wait();
    HAL_Delay(1000);

    for (int i = 0; i < 4; i++)
    {
        const int bx = 12 + i * 36;      /* намеренно не кратно 8 */
        const int by = 60;

        eink_test_square(fb, bx, by, 0);
        display_eink_request_region((int16_t)bx, (int16_t)by, 24, 24);
        display_eink_wait();
        HAL_Delay(1000);
    }

    /* 2. Режим 4 gray: полное (смена режима), затем 4 квадрата разных серых по diff. */
    display_eink_set_mode(DISPLAY_MODE_GRAY4);
    display_eink_request_full();
    display_eink_wait();
    HAL_Delay(1000);

    for (int i = 0; i < 4; i++)
    {
        const int bx = 12 + i * 36;
        const int by = 160;

        eink_test_square(fb, bx, by, (uint8_t)i);
        display_eink_request_region((int16_t)bx, (int16_t)by, 24, 24);
        display_eink_wait();
        HAL_Delay(1000);
    }

    display_eink_sleep();
}
