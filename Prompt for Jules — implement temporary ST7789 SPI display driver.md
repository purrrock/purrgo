Implement a STM32 HAL driver for the 240×320 TFT display with an ST7789 controller.

## Project context
The TFT is being used only as a temporary development/debugging display until the final Waveshare 2.7-inch e-Paper display arrives.

The intended display architecture is:

    PurrGO graphics
           │
           ▼
    display_hal / gfx
           │
           ▼
    display_st7789.c
           │
           ▼
    SPI1 + GPIO

The future e-Paper driver will use the same higher-level graphics layer:

    PurrGO graphics
           │
           ▼
    display_hal / gfx
           │
           ▼
    display_epaper.c
           │
           ▼
    SPI + GPIO

Therefore, keep the ST7789 implementation isolated from the graphics/application code as much as practical.

## Existing hardware connection

Use exactly this pin assignment:

| TFT pin | STM32F411CEU6 | Function |
|---|---|---|
| GND | GND | Ground |
| VCC | 3V3 | Power |
| SCL | PA5 | SPI1_SCK |
| SDA | PA7 | SPI1_MOSI |
| RST | PB0 | Hardware reset, GPIO output `TFT_RST` |
| DC | PB1 | Data/Command, GPIO output `TFT_DC` |
| CS | PA4 | Chip Select, GPIO output `TFT_CS` |
| BL | 3V3 | Backlight permanently enabled |

SPI1 is already configured in CubeMX.

Use the CubeMX-generated SPI handle:

    extern SPI_HandleTypeDef hspi1;

Do not create another SPI peripheral or another SPI handle.

## Required files

Create:

    display_st7789.h
    display_st7789.c

Place them in the project's existing appropriate `apps/stm32/PurrGO_STM32/Core/Inc` and `apps/stm32/PurrGO_STM32/Core/Src` locations.
Do not introduce unnecessary new modules.

## Required public API

Implement at least these functions:

    void ST7789_Init(void);

    void ST7789_SetWindow(uint16_t x0,
                          uint16_t y0,
                          uint16_t x1,
                          uint16_t y1);

    void ST7789_DrawPixel(uint16_t x,
                          uint16_t y,
                          uint16_t color);

    void ST7789_FillRect(uint16_t x,
                         uint16_t y,
                         uint16_t width,
                         uint16_t height,
                         uint16_t color);

    void ST7789_FillScreen(uint16_t color);

Use `uint16_t` RGB565 colors.

Define useful constants in the header, for example:

    ST7789_WIDTH   240
    ST7789_HEIGHT  320

and common RGB565 colors if appropriate.

## GPIO handling

Use the CubeMX-generated GPIO symbols:

    TFT_RST_Pin
    TFT_RST_GPIO_Port

    TFT_DC_Pin
    TFT_DC_GPIO_Port

    TFT_CS_Pin
    TFT_CS_GPIO_Port

Do not hard-code GPIO register addresses or pin numbers in the driver.

Use HAL GPIO functions:

    HAL_GPIO_WritePin()

The driver must control:

### CS

CS is active low.

Before SPI transmission:

    TFT_CS = LOW

After the transmission:

    TFT_CS = HIGH

### DC

DC selects command/data:

    DC = LOW  -> command
    DC = HIGH -> data

Create small internal helper functions if useful, for example:

    ST7789_WriteCommand()
    ST7789_WriteData()

These should remain private to `display_st7789.c`.

### RST

Perform a real hardware reset during `ST7789_Init()` using PB0 / `TFT_RST`.

The reset sequence must explicitly drive the reset pin low, wait, drive it high, and wait for the controller to become ready.

Use `HAL_Delay()` for the reset timing.

Do not replace the hardware reset with a software-only reset.

## SPI transmission

Use blocking HAL SPI transmission only:

    HAL_SPI_Transmit(&hspi1, ...)

Do NOT implement:

- DMA
- interrupts
- circular DMA
- callbacks
- RTOS synchronization
- double buffering
- asynchronous transfers

This display is temporary, so the simplest reliable implementation is preferred.

Do not use `HAL_SPI_Transmit_DMA()`.

## ST7789 initialization

Implement an actual ST7789 initialization sequence suitable for a 240×320 TFT.

The initialization must at minimum cover the controller reset and the basic commands required to bring the display out of sleep and into normal display operation, including the necessary:

- software reset / controller reset handling if required by the selected sequence
- pixel format configuration for RGB565
- memory access control/orientation
- display inversion setting if required by the selected ST7789 configuration
- sleep-out
- normal display-on

Do not invent register addresses or initialization values.

If the exact initialization sequence for this specific ST7789 240×320 module cannot be established from the available project information, state this explicitly and use only values that can be verified from a reliable ST7789 reference or an established 240×320 ST7789 implementation.

Do not silently guess about panel-specific offsets, MADCTL orientation, or color order.

## Pixel format

Use RGB565, 16 bits per pixel.

For each pixel sent over SPI, transmit:

    high byte first
    low byte second

For example:

    color >> 8
    color & 0xFF

Do not use 18-bit color.

## ST7789_SetWindow()

Implement the standard ST7789 address-window mechanism:

1. Set column address.
2. Set page/row address.
3. Issue memory-write command.

The function should configure the rectangular region so subsequent pixel data is written into that region.

Use the appropriate ST7789 commands rather than writing directly to controller registers.

Be careful about inclusive coordinates:

    x0 ... x1
    y0 ... y1

Document the coordinate convention in the header/source.

For this 240×320 display, valid logical coordinates should be:

    x = 0 .. 239
    y = 0 .. 319

If the panel requires a physical X/Y offset, do not guess one. Keep the offset explicitly defined and documented only if it is verified for this display.

## ST7789_DrawPixel()

Implement:

    ST7789_DrawPixel(x, y, color)

It should:

1. Validate or safely reject coordinates outside the display.
2. Call `ST7789_SetWindow(x, y, x, y)`.
3. Send exactly one RGB565 pixel.

Keep the implementation simple.

## ST7789_FillRect()

Implement:

    ST7789_FillRect(x, y, width, height, color)

It should:

1. Handle zero width/height safely.
2. Clip the rectangle to the display boundaries or otherwise safely reject out-of-range regions.
3. Set the appropriate address window once.
4. Send the required RGB565 pixel data for the complete rectangle.

Do not call `ST7789_DrawPixel()` once per pixel. That would generate excessive command overhead.

Because DMA is explicitly excluded, it is acceptable to transmit the rectangle using blocking `HAL_SPI_Transmit()` calls.

You may use a small temporary RAM buffer containing repeated RGB565 pixels to reduce the number of HAL calls, but keep the implementation simple and avoid unnecessary memory consumption.

## ST7789_FillScreen()

Implement:

    ST7789_FillScreen(color)

It should fill the entire 240×320 display with the specified RGB565 color.

Do not implement this by calling `ST7789_DrawPixel()` 76,800 times.

Use one complete display window followed by sequential pixel data transmission.

## Error handling

Do not introduce a complicated error-handling framework.

However, check the return value of `HAL_SPI_Transmit()` where practical.

If the existing PurrGO project has an established error-reporting convention, follow it.

Otherwise, keep the driver simple and deterministic.

Do not add UART logging to every SPI operation.

## Integration constraints

Do not modify the existing graphics renderer unless absolutely necessary.

Do not modify the future e-Paper architecture.

Do not add a generic display abstraction as part of this task.

Do not add DMA.

Do not add FreeRTOS.

Do not add a framebuffer.

Do not add font rendering.

Do not add lines, circles, text, bitmap loading, or other graphics primitives.

The only required graphics operations are:

- pixel
- filled rectangle
- full-screen fill

The purpose of this driver is to establish a working low-level ST7789 display path that can later be used by the existing PurrGO graphics layer.

## Code quality

Write normal STM32 HAL C code suitable for STM32F411CEU6.

Use clear comments, especially around:

- ST7789 command/data protocol
- CS/DC handling
- reset timing
- RGB565 byte order
- address-window coordinates
- why blocking SPI is intentionally used

Keep hardware-specific code inside `display_st7789.c`.

Do not use direct register manipulation when an existing HAL function is sufficient.

Do not use magic GPIO numbers; use the CubeMX-generated `TFT_*_Pin` and `TFT_*_GPIO_Port` definitions.

## Verification

After implementing the driver, integrate a minimal test into the existing application so that the display can be verified physically.

The test should demonstrate at least:

1. `ST7789_Init()`
2. Fill the screen with one solid color.
3. Fill a smaller rectangle with another color.
4. Draw one or several individual pixels.

Prefer a very simple deterministic test pattern.

For example:

- initialize the display
- fill it black
- draw a white rectangle
- draw several colored pixels or small colored rectangles

Do not build a complete UI for this task.

## Important

This is a temporary debugging driver. Prefer a small, understandable and reliable implementation over abstraction or optimization.

The final result must compile with the existing STM32F411CEU6 CubeMX/HAL project and use the already configured SPI1 and GPIO pins exactly as specified above.
If any required ST7789 parameter for this exact display cannot be verified, do not guess. Clearly identify the missing information.