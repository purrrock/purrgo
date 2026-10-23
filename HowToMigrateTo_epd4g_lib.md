## Что нужно сделать в PurrGO
- Заменить `display_eink.c`. Оставить смену делителя SPI1 на `/8` (на 42 МГц панель искажает данные). `DEV_Config.c`, `EPD_2in7_V2.c` и сабмодуль `third_party/e-Paper` убрать из `CMakeLists.txt`.
- Переписать `eink_test.c`: он напрямую вызывает `EPD_2IN7_V2_Init/Clear` и `DEV_Module_Init`.
- Режим выбирается по экрану (карта в 4 gray, меню и маршрутный компьютер в 1-bit) вместо сборочного `PURRGO_HW_EINK_MODE`. В `app_ui.c` при смене экрана перед `display_refresh()` нужно сообщить дисплею режим.
- `display_flush()` и `display_refresh()` должны запускать `epd4g_start_*`. В цикле `purrgo_stm32_process()` нужно вызывать `epd4g_poll()`, а `purrgo_app_ui_render` не вызывать, пока `epd4g_data_busy()` равен 1.
- Нужен буфер `prev` на 11616 байт для 4-gray partial карты.
- В `power_stm32.c` перед STANDBY дождаться конца обновления через блокирующие `epd4g_wait()` и `epd4g_sleep()`.
- Слить правила сна: `display_eink` сейчас засыпает после каждого обновления, и это можно оставить.
- Добавить `epd4g/include` в пути и, при желании, не включать `epd4g` в `purrgo_sources_warnings`: у вас `-Werror` под `PURRGO_WERROR`, а мой код этими флагами не проверялся.

## Чего по-прежнему нет
- Прерывание EXTI по BUSY вместо опроса: достаточно просыпаться по SysTick, но экономия энергии возможна.
- SPI через DMA: не нужен, пока `epd4g_poll()` укладывается в миллисекунду.
