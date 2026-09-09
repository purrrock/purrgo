#include "display_st7789.h"
#include "spi.h"
#include "gpio.h"

/*
 * Note: Uses existing extern SPI_HandleTypeDef hspi1 from spi.h
 */

/* ST7789 Commands */
#define ST7789_SWRESET 0x01
#define ST7789_SLPOUT  0x11
#define ST7789_NORON   0x13
#define ST7789_INVON   0x21
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C
#define ST7789_MADCTL  0x36
#define ST7789_COLMOD  0x3A

/* Helper for writing command */
static void ST7789_WriteCommand(uint8_t cmd) {
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY) != HAL_OK) {
        Error_Handler();
    }
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

/* Helper for writing 8-bit data */
static void ST7789_WriteData(uint8_t data) {
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_Transmit(&hspi1, &data, 1, HAL_MAX_DELAY) != HAL_OK) {
        Error_Handler();
    }
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

/* Helper for writing block data */
void ST7789_WriteDataBlock(uint8_t *data, uint16_t size) {
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_Transmit(&hspi1, data, size, HAL_MAX_DELAY) != HAL_OK) {
        Error_Handler();
    }
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

void ST7789_Init(void) {
    /* Hardware Reset */
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(150);

    /* Software Reset */
    ST7789_WriteCommand(ST7789_SWRESET);
    HAL_Delay(150);

    /* Sleep Out */
    ST7789_WriteCommand(ST7789_SLPOUT);
    HAL_Delay(120);

    /* Pixel Format (RGB565) */
    ST7789_WriteCommand(ST7789_COLMOD);
    ST7789_WriteData(0x55); /* 16 bits/pixel */

    /* Memory Access Control */
    /* Note: Using generic ST7789 default controller values.
     * MADCTL orientation, RGB color order, and any X/Y offsets
     * are unverified for the specific GMT024-08-SPI8P panel. */
    ST7789_WriteCommand(ST7789_MADCTL);
    ST7789_WriteData(0x00);

    /* Inversion On - common for ST7789 */
    ST7789_WriteCommand(ST7789_INVON);

    /* Normal Display On */
    ST7789_WriteCommand(ST7789_NORON);
    HAL_Delay(10);

    /* Display On */
    ST7789_WriteCommand(ST7789_DISPON);
    HAL_Delay(120);
}

void ST7789_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];

    /* Set column address */
    ST7789_WriteCommand(ST7789_CASET);
    data[0] = x0 >> 8;
    data[1] = x0 & 0xFF;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xFF;
    ST7789_WriteDataBlock(data, 4);

    /* Set row address */
    ST7789_WriteCommand(ST7789_RASET);
    data[0] = y0 >> 8;
    data[1] = y0 & 0xFF;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xFF;
    ST7789_WriteDataBlock(data, 4);

    /* Write to RAM */
    ST7789_WriteCommand(ST7789_RAMWR);
}

void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if ((x >= ST7789_WIDTH) || (y >= ST7789_HEIGHT)) {
        return;
    }

    ST7789_SetWindow(x, y, x, y);

    uint8_t data[2];
    data[0] = color >> 8;
    data[1] = color & 0xFF;

    ST7789_WriteDataBlock(data, 2);
}

void ST7789_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    if ((width == 0) || (height == 0) || (x >= ST7789_WIDTH) || (y >= ST7789_HEIGHT)) {
        return;
    }

    /* Clip width and height */
    if (x + width > ST7789_WIDTH) {
        width = ST7789_WIDTH - x;
    }
    if (y + height > ST7789_HEIGHT) {
        height = ST7789_HEIGHT - y;
    }

    ST7789_SetWindow(x, y, x + width - 1, y + height - 1);

    uint8_t data[2] = {color >> 8, color & 0xFF};

    /* Using a small buffer to avoid massive overhead of single pixel transmission. */
    #define FILL_BUF_SIZE 128
    uint8_t buf[FILL_BUF_SIZE * 2];
    for (int i = 0; i < FILL_BUF_SIZE; i++) {
        buf[i * 2] = data[0];
        buf[i * 2 + 1] = data[1];
    }

    uint32_t pixels_to_write = width * height;

    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

    while (pixels_to_write > 0) {
        uint16_t chunk_pixels = (pixels_to_write > FILL_BUF_SIZE) ? FILL_BUF_SIZE : pixels_to_write;
        if (HAL_SPI_Transmit(&hspi1, buf, chunk_pixels * 2, HAL_MAX_DELAY) != HAL_OK) {
            Error_Handler();
        }
        pixels_to_write -= chunk_pixels;
    }

    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

void ST7789_FillScreen(uint16_t color) {
    ST7789_FillRect(0, 0, ST7789_WIDTH, ST7789_HEIGHT, color);
}

void ST7789_StartPixels(void) {
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
}

void ST7789_WritePixels(uint8_t *data, uint16_t size) {
    if (HAL_SPI_Transmit(&hspi1, data, size, HAL_MAX_DELAY) != HAL_OK) {
        Error_Handler();
    }
}

void ST7789_EndPixels(void) {
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}
