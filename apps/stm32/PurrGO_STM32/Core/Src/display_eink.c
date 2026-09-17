#include "display_eink.h"

#include "DEV_Config.h"
#include "EPD_2in7_V2.h"

#include <stddef.h>

/*
 * Размер framebuffer PurrGO для E-Ink.
 *
 * Дисплей:
 *   176 x 264 пикселя
 *
 * Формат:
 *   2 бита на пиксель
 *
 * Поэтому:
 *
 *   176 * 264 * 2 / 8 = 11616 байт
 *
 * EPD_2IN7_V2_4GrayDisplay() непосредственно работает
 * с этим форматом буфера, поэтому дополнительное
 * преобразование изображения здесь не требуется.
 */
#define EINK_FRAMEBUFFER_SIZE \
    ((EPD_2IN7_V2_WIDTH * EPD_2IN7_V2_HEIGHT * 2U) / 8U)

/*
 * Защита от незаметного изменения формата дисплея
 * в используемой версии Waveshare driver.
 */
#if (EINK_FRAMEBUFFER_SIZE != 11616U)
#error "Unexpected E-Ink framebuffer size"
#endif


void display_eink_init(void)
{
    /*
     * GPIO и SPI1 инициализируются CubeMX.
     *
     * DEV_Module_Init() здесь выполняет только начальную
     * установку управляющих линий E-Ink:
     *
     *   CS  = HIGH
     *   DC  = LOW
     *   RST = HIGH
     *
     * SPI1 повторно не инициализируется.
     */
    (void)DEV_Module_Init();
}


void display_eink_refresh(const uint8_t *framebuffer)
{
    /*
     * Не передаём NULL в Waveshare driver.
     */
    if (framebuffer == NULL)
    {
        return;
    }

    /*
     * После EPD_2IN7_V2_Sleep() дисплей необходимо
     * повторно инициализировать перед следующим обновлением.
     *
     * Поэтому Init_4GRAY() выполняется перед каждым
     * физическим обновлением.
     */
    EPD_2IN7_V2_Init_4GRAY();

    /*
     * Waveshare 4-gray driver непосредственно принимает
     * двухбитный framebuffer размером 11616 байт.
     *
     * Преобразование framebuffer не требуется.
     *
     * EPD_2IN7_V2_4GrayDisplay() выполняет полное
     * обновление обоих RAM-планов E-Ink и ожидает
     * завершения обновления.
     */
    EPD_2IN7_V2_4GrayDisplay((UBYTE *)framebuffer);

    /*
     * После обновления переводим дисплей в sleep.
     *
     * Для E-Ink это существенно снижает потребление,
     * что соответствует назначению PurrGO.
     */
    EPD_2IN7_V2_Sleep();

    /*
     * Оставляем интерфейс модуля в безопасном состоянии.
     * При следующем refresh display_eink_refresh()
     * снова вызовет DEV_Module_Init() только если
     * display_eink_init() будет вызван повторно.
     *
     * В текущей схеме DEV_Module_Exit() здесь не вызываем:
     * display_eink_init() выполняется один раз при
     * инициализации backend, а GPIO/SPI принадлежат
     * CubeMX и приложению.
     */
}