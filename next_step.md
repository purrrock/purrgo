Я освободил в CubeMX выводы дисплея ST7789 и временных кнопок и SPI2.
У нас в планах перейти от картридера на SPI2 на SDIO. У меня уже есть картридер SDIO.
Я попробовал включить в CubeMX SDIO, он назначил выводы:
PA6	SDIO_CMD
PA8 SDIO_D1
PA9 SDIO_D2
PB4 SDIO_D0
PB5 SDIO_D3
PB15 SDIO_CK
PB12 Detect_SDIO

И перенёс USART1_TX на вывод PA15.

В FATFS вместо "User-defined" я выбрал "SD"

Теперь нужно:
Удалить из проекта код дисплея ST7789.
Переделай файл PurrGO_STM32_PINOUT.md и проверь файл PurrGO_STM32.ioc и переделай код работы с SD-Картой.