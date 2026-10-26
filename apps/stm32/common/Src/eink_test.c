#include "eink_test.h"

#include "DEV_Config.h"
#include "EPD_2in7_V2.h"
#include "display_eink.h"

#include <stddef.h>

/*
 * Размер framebuffer.
 *
 * Драйвер Waveshare определяет:
 *
 *   WIDTH  = 176
 *   HEIGHT = 264
 *
 * Один пиксель занимает один бит.
 *
 * Поэтому:
 *
 *   176 / 8 = 22 байта на строку
 *   22 * 264 = 5808 байт
 */
#define EINK_TEST_WIDTH_BYTES   (EPD_2IN7_V2_WIDTH / 8)
#define EINK_TEST_IMAGE_SIZE    (EINK_TEST_WIDTH_BYTES * EPD_2IN7_V2_HEIGHT)


/*
 * Буфер изображения.
 *
 * 5808 байт для STM32F411 — небольшая область памяти,
 * поэтому для аппаратного теста проще использовать статический
 * буфер, а не malloc().
 */
static UBYTE eink_test_image[EINK_TEST_IMAGE_SIZE];


/*
 * Создать тестовый рисунок.
 *
 * В буфере Waveshare:
 *
 *   0xFF -> белые пиксели
 *   0x00 -> чёрные пиксели
 *
 * Делаем чередующиеся горизонтальные полосы.
 *
 * Это позволяет визуально проверить:
 *
 *   - передачу framebuffer;
 *   - полный размер изображения;
 *   - отсутствие полностью "зависшего" дисплея;
 *   - правильную работу DC/CS/SPI.
 */
static void EinkTest_CreatePattern(void)
{
    uint32_t y;
    uint32_t x;

    for (y = 0; y < EPD_2IN7_V2_HEIGHT; y++)
    {
        /*
         * Каждые 32 строки меняем цвет полосы.
         */
        UBYTE value = ((y / 32U) & 1U) ? 0x00U : 0xFFU;

        for (x = 0; x < EINK_TEST_WIDTH_BYTES; x++)
        {
            eink_test_image[y * EINK_TEST_WIDTH_BYTES + x] = value;
        }
    }
}


/*
 * Выполнить тест E-Ink.
 */
void EinkTest_Run(void)
{
    /*
     * Подготовить управляющие линии E-Ink.
     *
     * SPI1 уже должен быть инициализирован CubeMX.
     */
    if (DEV_Module_Init() != 0)
    {
        /*
         * В текущем аппаратном слое эта функция всегда
         * возвращает 0.
         *
         * Оставляем проверку потому, что она соответствует
         * API Waveshare и позволяет расширить обработку
         * ошибки позднее.
         */
        return;
    }

    /*
     * Инициализация контроллера E-Ink V2.
     *
     * Здесь используется оригинальная последовательность
     * из Waveshare EPD_2in7_V2.c:
     *
     *   hardware reset
     *   ожидание BUSY
     *   software reset
     *   настройка RAM
     *   настройка data entry mode
     */
    EPD_2IN7_V2_Init();

    /*
     * Сначала очищаем экран.
     *
     * Это отдельное полное обновление и может занять
     * несколько секунд.
     */
    EPD_2IN7_V2_Clear();

    /*
     * Создаём диагностический рисунок.
     */
    EinkTest_CreatePattern();

    /*
     * Передаём полный framebuffer дисплею
     * и запускаем обновление.
     *
     * Оригинальный Waveshare-драйвер сам ожидает
     * окончания BUSY.
     */
    EPD_2IN7_V2_Display(eink_test_image);

    /*
     * После завершения обновления переводим E-Ink
     * в Sleep.
     *
     * Для дальнейшего обновления дисплей потребуется
     * снова инициализировать.
     */
    EPD_2IN7_V2_Sleep();

    /*
     * Оставляем CS в неактивном состоянии.
     */
    DEV_Module_Exit();
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
    /* 176 * 264 * 2 bit / 8 = 11616 байт. */
    static uint8_t fb[(EPD_2IN7_V2_WIDTH * EPD_2IN7_V2_HEIGHT * 2U) / 8U];

    EinkTest_FillPattern4Gray(fb, EPD_2IN7_V2_WIDTH, EPD_2IN7_V2_HEIGHT);

    /* Ровно тот же вызов, который делает display_stm32.c. */
    display_eink_init();
    display_eink_refresh(fb);
}


void EinkTest_RunPartial(void)
{
    static uint8_t fb[(EPD_2IN7_V2_WIDTH * EPD_2IN7_V2_HEIGHT * 2U) / 8U];
    const int w = EPD_2IN7_V2_WIDTH;
    const int h = EPD_2IN7_V2_HEIGHT;

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

    display_eink_init();
    display_eink_refresh(fb);            /* полное обновление */
    DEV_Delay_ms(1000);

    for (int i = 0; i < 4; i++)
    {
        const int bx = 12 + i * 36;      /* намеренно не кратно 8 */
        const int by = 100;

        for (int y = by; y < by + 24; y++)
        {
            for (int x = bx; x < bx + 24; x++)
            {
                eink_set_px2(fb, w, x, y, 0);
            }
        }

        display_eink_refresh_region(fb, (int16_t)bx, (int16_t)by, 24, 24);
        DEV_Delay_ms(1000);
    }
}
