#ifndef DEV_CONFIG_H
#define DEV_CONFIG_H

#include "main.h"
#include <stdint.h>

/*
 * Типы, которые использует оригинальный Waveshare EPD_2in7_V2.c.
 */
#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

/*
 * E-Ink подключён к STM32F411 следующим образом:
 *
 * PA7 -> DIN  (SPI1 MOSI)
 * PA5 -> CLK  (SPI1 SCK)
 * PB7 -> CS
 * PB8 -> DC
 * PB9 -> RST
 * PB2 -> BUSY
 *
 * PA7/PA5 управляются аппаратным SPI1, поэтому в этом слое
 * они отдельно не переключаются как GPIO.
 *
 * E-Ink питается постоянно от 3.3 V, поэтому EPD_PWR_PIN
 * из оригинального Waveshare DEV_Config здесь отсутствует.
 */

/*
 * Эти макросы имеют формат, ожидаемый оригинальным
 * EPD_2in7_V2.c:
 *
 *     DEV_Digital_Write(EPD_CS_PIN, 0);
 *
 * CubeMX создаёт EINK_*_GPIO_Port и EINK_*_Pin
 * согласно label в .ioc.
 */
#define EPD_CS_PIN      EINK_CS_GPIO_Port,   EINK_CS_Pin
#define EPD_DC_PIN      EINK_DC_GPIO_Port,   EINK_DC_Pin
#define EPD_RST_PIN     EINK_RST_GPIO_Port,  EINK_RST_Pin
#define EPD_BUSY_PIN    EINK_BUSY_GPIO_Port, EINK_BUSY_Pin

/*
 * GPIO-обёртки.
 *
 * В оригинальном Waveshare драйвере 0 означает LOW,
 * любое ненулевое значение — HIGH.
 */
#define DEV_Digital_Write(_pin, _value) \
    HAL_GPIO_WritePin(_pin, ((_value) == 0) ? GPIO_PIN_RESET : GPIO_PIN_SET)

#define DEV_Digital_Read(_pin) \
    HAL_GPIO_ReadPin(_pin)

/*
 * Задержка используется драйвером для reset и ожидания BUSY.
 */
#define DEV_Delay_ms(_xms) HAL_Delay(_xms)

/*
 * Передача одного байта через уже инициализированный SPI1.
 */
void DEV_SPI_WriteByte(UBYTE value);

/*
 * Передача блока байт.
 *
 * В текущей версии EPD_2in7_V2.c основной путь использует
 * DEV_SPI_WriteByte(), но функция оставлена для совместимости
 * с Waveshare DEV_Config API.
 */
void DEV_SPI_Write_nByte(UBYTE *value, UDOUBLE len);

/*
 * Совместимость с API Waveshare.
 *
 * SPI1 и GPIO уже инициализированы CubeMX до запуска теста,
 * поэтому эти функции не должны повторно инициализировать
 * периферию.
 */
int  DEV_Module_Init(void);
void DEV_Module_Exit(void);
void DEV_GPIO_Init(void);
void DEV_SPI_Init(void);

#endif /* DEV_CONFIG_H */
