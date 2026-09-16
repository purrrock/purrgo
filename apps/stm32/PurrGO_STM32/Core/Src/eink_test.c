#include "eink_test.h"

#include "DEV_Config.h"
#include "EPD_2in7_V2.h"

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

