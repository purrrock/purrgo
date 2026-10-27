/* Заглушка spi.h CubeMX для host-теста display_eink.c. */
#ifndef STM32_SHIM_SPI_H
#define STM32_SHIM_SPI_H
#include <stdint.h>
#define SPI_BAUDRATEPRESCALER_8 0x20000000U
typedef struct { struct { uint32_t BaudRatePrescaler; } Init; } SPI_HandleTypeDef;
extern SPI_HandleTypeDef hspi1;
int HAL_SPI_DeInit(SPI_HandleTypeDef *h);
int HAL_SPI_Init(SPI_HandleTypeDef *h);
#endif
