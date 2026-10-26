#include "DEV_Config.h"
#include "spi.h"

/*
 * SPI1 создаётся CubeMX.
 *
 * На момент вызова eink_test() функция MX_SPI1_Init()
 * уже должна быть выполнена в main().
 */
extern SPI_HandleTypeDef hspi1;


/*
 * Передать один байт через SPI1.
 *
 * CS здесь НЕ управляется.
 *
 * Это важно: оригинальный EPD_2in7_V2.c самостоятельно
 * устанавливает DC и CS перед вызовом этой функции.
 */
void DEV_SPI_WriteByte(UBYTE value)
{
    /*
     * Передача одного байта.
     *
     * Таймаут 1000 мс значительно больше нормального времени
     * передачи одного байта даже при низкой частоте SPI.
     */
    (void)HAL_SPI_Transmit(&hspi1, &value, 1, 1000);
}


/*
 * Передать блок байт через SPI1.
 *
 * CS также не управляется этой функцией.
 */
void DEV_SPI_Write_nByte(UBYTE *value, UDOUBLE len)
{
    if (value == NULL || len == 0)
    {
        return;
    }

    (void)HAL_SPI_Transmit(&hspi1, value, (uint16_t)len, 1000);
}


/*
 * Инициализация аппаратного слоя Waveshare.
 *
 * В нашем проекте GPIO и SPI уже инициализируются CubeMX,
 * поэтому повторно ничего не настраиваем.
 *
 * Здесь только приводим управляющие линии E-Ink
 * в безопасное исходное состояние.
 */
int DEV_Module_Init(void)
{
    /*
     * E-Ink не выбран.
     */
    HAL_GPIO_WritePin(EINK_CS_GPIO_Port,
                      EINK_CS_Pin,
                      GPIO_PIN_SET);

    /*
     * Перед началом обмена считаем DC = command.
     */
    HAL_GPIO_WritePin(EINK_DC_GPIO_Port,
                      EINK_DC_Pin,
                      GPIO_PIN_RESET);

    /*
     * Reset в неактивном состоянии.
     */
    HAL_GPIO_WritePin(EINK_RST_GPIO_Port,
                      EINK_RST_Pin,
                      GPIO_PIN_SET);

    return 0;
}


/*
 * Завершение работы аппаратного слоя.
 *
 * Питание дисплея физически не отключается:
 * E-Ink подключён непосредственно к 3.3 V.
 *
 * Сам дисплей переводится в Sleep функцией
 * EPD_2IN7_V2_Sleep().
 */
void DEV_Module_Exit(void)
{
    HAL_GPIO_WritePin(EINK_CS_GPIO_Port,
                      EINK_CS_Pin,
                      GPIO_PIN_SET);

    HAL_GPIO_WritePin(EINK_DC_GPIO_Port,
                      EINK_DC_Pin,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(EINK_RST_GPIO_Port,
                      EINK_RST_Pin,
                      GPIO_PIN_SET);
}


/*
 * Совместимость с оригинальным API.
 *
 * GPIO уже настроены CubeMX.
 */
void DEV_GPIO_Init(void)
{
}


/*
 * Совместимость с оригинальным API.
 *
 * SPI1 уже настроен и запущен CubeMX.
 */
void DEV_SPI_Init(void)
{
}

