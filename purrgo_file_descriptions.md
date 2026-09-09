# PurrGo — описание файлов и каталогов

`purrgo_file_descriptions.md` — описание структуры исходных файлов проекта (этот файл)  
`HARDWARE.md` — описание аппаратной архитектуры, платформ разработки и планируемого устройства  
`README.md` — основное описание PurrGo, архитектуры и текущего назначения проекта  
`TODO.md` — текущий список задач и направлений дальнейшей разработки  
`ai_project_dump.py` — генератор текстового дампа исходных файлов проекта для передачи AI  
`LICENSE` — лицензия проекта  
`PurrGO_STM32_PINOUT.md` — распиновка и назначение выводов STM32-платы PurrGO  

---

## apps/

`apps/` — приложения и исполняемые точки входа проекта

### apps/emulator/

`apps/emulator/` — PC-эмулятор навигатора с виртуальным дисплеем, GNSS, картами и органами управления

#### apps/emulator/include/

`apps/emulator/include/` — заголовочные файлы интерфейса эмулятора
`apps/emulator/include/display.h` — интерфейс виртуального дисплея и framebuffer  
`apps/emulator/include/emu_window.h` — интерфейс SDL-окна, рендеринга и обработки виртуальных кнопок  

#### apps/emulator/src/

`apps/emulator/src/` — исходные файлы PC-эмулятора
`apps/emulator/src/display.c` — реализация виртуального дисплея и framebuffer  
`apps/emulator/src/emu_fs.c` — адаптер файловой системы между эмулятором и HAL PurrGo  
`apps/emulator/src/emu_window.c` — SDL-окно эмулятора, вывод framebuffer и обработка виртуальных кнопок  
`apps/emulator/src/main.c` — точка входа эмулятора и связывание GNSS, UI, графики, карт и виртуального дисплея  

---

### apps/stm32/

`apps/stm32/` — приложение PurrGo для микроконтроллеров STM32
`apps/stm32/README.md` — описание текущего состояния STM32 application layer

#### apps/stm32/PurrGO_STM32/

`apps/stm32/PurrGO_STM32/` — проект STM32F411 для аппаратной платформы PurrGO
`apps/stm32/PurrGO_STM32/PurrGO_STM32.ioc` — конфигурация STM32CubeMX проекта  

##### apps/stm32/PurrGO_STM32/Core/Inc/

`apps/stm32/PurrGO_STM32/Core/Inc/` — прикладные и аппаратные заголовочные файлы STM32-приложения
`buttons.h` — интерфейс кнопок PurrGO  
`display_st7789.h` — интерфейс временного драйвера TFT-дисплея ST7789  
`display_stm32.h` — STM32-реализация интерфейса дисплея PurrGo  
`fatfs_sd.h` — интерфейс работы с SD-картой через FatFs  
`gpio.h` — конфигурация GPIO  
`main.h` — основной заголовок STM32-приложения  
`purrgo_logger.h` — STM32-адаптер журналирования PurrGo  
`spi.h` — конфигурация SPI  
`usart.h` — конфигурация USART  
`stm32f4xx_hal_conf.h` — конфигурация STM32 HAL  
`stm32f4xx_it.h` — объявления обработчиков прерываний  

##### apps/stm32/PurrGO_STM32/Core/Src/

`apps/stm32/PurrGO_STM32/Core/Src/` — исходный код STM32-приложения
`buttons.c` — обработка кнопок  
`display_st7789.c` — драйвер TFT-дисплея ST7789 по SPI  
`display_stm32.c` — адаптер дисплея для STM32 и интеграция с PurrGo display API  
`fatfs_sd.c` — работа с SD-картой через FatFs  
`fatfs_sd_with_diags.c` — диагностическая версия SD/FatFs-кода  
`fs_hal_stm32.c` — STM32-реализация файлового HAL PurrGo  
`gnss_io.c` — STM32-ввод GNSS-данных  
`gpio.c` — инициализация GPIO  
`main.c` — основной цикл и инициализация STM32-приложения  
`purrgo_logger.c` — STM32-реализация журналирования  
`spi.c` — инициализация SPI  
`stm32f4xx_hal_msp.c` — низкоуровневая инициализация периферии HAL  
`stm32f4xx_it.c` — обработчики аппаратных прерываний  
`syscalls.c` — системные вызовы для C runtime  
`sysmem.c` — управление heap для C runtime  
`system_stm32f4xx.c` — системный код CMSIS для STM32F4  
`system_time.c` — системное время STM32  
`usart.c` — инициализация USART  

##### apps/stm32/PurrGO_STM32/FATFS/

`apps/stm32/PurrGO_STM32/FATFS/` — сгенерированная конфигурация и адаптация FatFs
`FATFS/App/fatfs.c` — инициализация FatFs  
`FATFS/App/fatfs.h` — интерфейс и объявления FatFs-приложения  
`FATFS/Target/ffconf.h` — конфигурация FatFs  
`FATFS/Target/user_diskio.c` — интерфейс дискового устройства для FatFs  
`FATFS/Target/user_diskio.h` — объявления дискового интерфейса  

##### apps/stm32/PurrGO_STM32/Middlewares/Third_Party/FatFs/

`apps/stm32/PurrGO_STM32/Middlewares/Third_Party/FatFs/` — сторонняя библиотека FatFs
`src/ff.c` — основная реализация FatFs  
`src/ff.h` — публичный интерфейс FatFs  
`src/diskio.c` — низкоуровневый дисковый интерфейс FatFs  
`src/diskio.h` — объявления дискового интерфейса  
`src/ff_gen_drv.c` — общий слой драйверов FatFs  
`src/ff_gen_drv.h` — интерфейс общего слоя драйверов  
`src/integer.h` — типы данных FatFs  
`src/option/syscall.c` — системные функции FatFs  

Остальные файлы CMSIS и STM32 HAL в `apps/stm32/PurrGO_STM32/Drivers/` являются библиотечным/сгенерированным кодом ST и отдельно в описании проекта не детализируются.

---

## docs/

`docs/` — проектная документация и технические материалы

### docs/Display_modules/

`docs/Display_modules/` — документация используемых и рассматриваемых дисплеев
`docs/Display_modules/display-e-ink-paper-hat-2n7in-user-manual.pdf` — руководство Waveshare для 2.7" e-Paper HAT  
`docs/Display_modules/TTF/From seller.txt` — информация от продавца TTF-дисплея  
`docs/Display_modules/TTF/Pin-Assignment2.4-inch-TFTPins-8P.webp` — схема назначения контактов TTF-дисплея  

### docs/GNSS_modules/

`docs/GNSS_modules/` — документация GNSS-модулей и приёмников

#### docs/GNSS_modules/NEO-6/

`NEO-6_DataSheet_(GPS.G6-HW-09005).pdf` — datasheet u-blox NEO-6  
`NEO-6_ProductSummary_(GPS.G6-HW-09003).pdf` — краткое описание NEO-6  
`u-blox6-GPS-GLONASS-QZSS-V14_ReceiverDescrProtSpec_(GPS.G6-SW-12013)_Public.pdf` — спецификация протокола и возможностей u-blox 6  
`u-blox6_ReceiverDescrProtSpec_(GPS.G6-SW-10018)_Public.pdf` — спецификация протокола u-blox 6  


#### docs/GNSS_modules/U-blox M10/

`G10A-F30_datasheet.pdf` — документация GNSS-модуля G10A F30  
`UBX-F10-M10-SPG7.0x_InterfaceDescription_UBXDOC-304424225-21489.pdf` — описание интерфейсов u-blox M10 SPG 7.x  
`u-blox-M10-SPG-5.30_InterfaceDescription_UBXDOC-304424225-20395.pdf` — описание интерфейсов u-blox M10 SPG 5.30  
`docs/GNSS_modules/gnss-configuration.md` — конфигурация GNSS-приёмника и связанные параметры  

### docs/STM32F411CEU6/

`docs/STM32F411CEU6/` — документация STM32F411CEU6
`PinoutDiagram_STM32F4x1.pdf` — схема назначения выводов STM32F4x1  
`STM32F411CEU6_Datasheet.pdf` — datasheet STM32F411CEU6  
`STM32F411CEU6_ReferenceManual.pdf` — reference manual STM32F411  

### docs/STM32U585CIU6/

`docs/STM32U585CIU6/` — документация STM32U585CIU6
`DataSheet_stm32u585ci.pdf` — datasheet STM32U585CI  

### docs/

`docs/PurrGO Map Format V3 — Binary Format Conformance.md` — требования и проверки соответствия реализации формату PurrGo Map V3  
`docs/PurrGO_font_encoding.md` — описание кодирования и формата растрового шрифта PurrGo  
`docs/USB-UART adapter.md` — описание USB-UART адаптера для разработки и тестирования  
`docs/architecture.md` — программная архитектура, границы модулей и правила разделения portable core и platform layer  
`docs/purrgo_map_specification_v3.md` — техническая спецификация бинарного формата карт PurrGo V3  

---

## include/

`include/` — публичные заголовочные файлы проекта

### include/purrgo/

`include/purrgo/` — публичный API и структуры PurrGo

`app_fsm.h` — интерфейс конечного автомата приложения и состояний пользовательского интерфейса  
`app_ui.h` — интерфейс прикладного UI  
`config.h` — compile-time конфигурация приложения  
`config_controller.h` — интерфейс управления конфигурацией приложения  
`display_hal.h` — аппаратно-независимый интерфейс дисплея  
`font5x7.h` — интерфейс растрового шрифта 5x7  
`fs_hal.h` — абстракция файловой системы для portable core  
`geo.h` — интерфейс географических вычислений  
`gfx_circle.h` — интерфейс рисования окружностей  
`gfx_icon.h` — интерфейс вывода и работы с графическими иконками  
`gfx_line.h` — интерфейс рисования линий, включая специальные типы линий  
`gfx_polygon.h` — интерфейс рисования и заполнения полигонов  
`gfx_rect.h` — интерфейс рисования прямоугольников  
`gfx_renderer.h` — общий интерфейс графического рендерера, цветов и контекста рисования  
`gfx_text.h` — интерфейс вывода растрового текста  
`gnss.h` — базовый интерфейс GNSS-подсистемы  
`gnss_adapter.h` — интерфейс адаптера входного потока GNSS  
`gnss_config.h` — параметры конфигурации GNSS  
`gnss_io.h` — интерфейс ввода GNSS-данных  
`gnss_mock.h` — интерфейс генератора mock GNSS-данных  
`gnss_types.h` — структуры и типы GNSS solution  
`gpx_parser.h` — интерфейс разбора GPX-данных  
`hardware_config.h` — настройка компиляции под различное аппаратное обеспечение  
`logger.h` — интерфейс системы журналирования  
`map.h` — публичный интерфейс map subsystem  
`map_controller.h` — интерфейс управления состоянием и отображением карты  
`map_style.h` — определения PurrGo feature codes, стилей карт и API сопоставления feature code → render style  
`navigation.h` — интерфейс навигационного состояния и расчётов  
`purrgo_poi_icons.h` — данные растровых иконок POI  
`purrgo_time.h` — типы и функции работы со временем проекта  
`sun.h` — интерфейс расчёта солнечных событий  
`sun_tables.h` — интерфейс таблиц солнечных расчётов  
`system_time.h` — интерфейс системного времени  
`track_logger.h` — интерфейс записи и обработки GPS-трека  
`track_renderer.h` — интерфейс рендеринга GPS-трека  
`trip_computer.h` — интерфейс расчёта параметров поездки/движения  
`types.h` — общие типы данных проекта  
`ubx.h` — интерфейс работы с сообщениями протокола u-blox UBX  

---

## src/

`src/` — исходный код реализации проекта

### src/core/

`src/core/` — переносимое аппаратно-независимое ядро PurrGo

`app_fsm.c` — реализация конечного автомата приложения и переходов между состояниями  
`app_ui.c` — прикладной слой UI и его интеграция с core  
`config.c` — реализация конфигурации приложения  
`config_controller.c` — реализация управления конфигурацией приложения  
`geo.c` — географические вычисления: расстояния, направления и координатные преобразования  
`gnss.c` — базовая реализация GNSS-подсистемы  
`gnss_adapter.c` — преобразование входных GNSS/NMEA данных во внутренний GNSS solution  
`gnss_mock.c` — генератор mock GNSS-данных для PC-тестирования и эмулятора  
`gpx_parser.c` — разбор GPX и извлечение трековых данных  
`navigation.c` — логика навигационного состояния и базовых навигационных вычислений  
`purrgo_time.c` — функции преобразования и обработки времени GNSS  
`sun.c` — расчёт времени восхода, заката и других солнечных событий по координатам и дате  
`sun_tables.c` — табличные данные для расчётов солнечных событий  
`track_logger.c` — логика накопления, фильтрации и записи точек трека  
`track_renderer.c` — рендеринг GPS-трека через графический интерфейс  
`trip_computer.c` — расчёт параметров текущей поездки/движения  
`ubx.c` — разбор и обработка сообщений протокола u-blox UBX  

### src/core/gfx/

`src/core/gfx/` — графическая библиотека portable core
`font5x7.c` — данные растрового шрифта 5x7  
`gfx_circle.c` — алгоритмы рисования окружностей и заполненных окружностей  
`gfx_icon.c` — вывод графических иконок  
`gfx_line.c` — алгоритмы рисования обычных, толстых, пунктирных, точечных и железнодорожных линий  
`gfx_polygon.c` — алгоритмы рисования и заполнения полигонов  
`gfx_rect.c` — алгоритмы рисования прямоугольников  
`gfx_renderer.c` — реализация графического контекста, цветов, clipping и вывода примитивов через callback  
`gfx_text.c` — реализация вывода текста с использованием растрового шрифта  

### src/core/map/

Map subsystem разделён на специализированные модули.

`map.c` — публичный/координирующий слой map subsystem  
`map_controller.c` — управление состоянием карты, viewport, pan/follow и связанными операциями  
`map_culling.c` — пространственное отсечение объектов карты  
`map_culling.h` — внутренний интерфейс map culling  
`map_idx.c` — чтение и обход `.idx`, SQT/R-tree и пространственного индекса  
`map_idx.h` — внутренний интерфейс `.idx` subsystem  
`map_internal.h` — внутренние типы и интерфейсы map subsystem  
`map_mlp.c` — чтение геометрических записей `.mlp`  
`map_mlp.h` — внутренний интерфейс `.mlp` subsystem  
`map_projection.c` — преобразование географических координат в координаты viewport  
`map_projection.h` — внутренний интерфейс map projection  
`map_render.c` — преобразование map objects в операции графического рендера  
`map_render.h` — внутренний интерфейс map rendering  
`map_style.c` — таблица соответствия PurrGo feature code → render style  

### src/core/ui/

`src/core/ui/` — специализированные компоненты пользовательского интерфейса

`ui_config.c` — экран и логика конфигурации  
`ui_config.h` — внутренний интерфейс UI configuration  
`ui_dir_select.c` — выбор каталога/файла  
`ui_dir_select.h` — внутренний интерфейс выбора каталога  
`ui_map.c` — экран карты и связанная логика UI  
`ui_map.h` — внутренний интерфейс map UI  
`ui_trip.c` — экран параметров поездки  
`ui_trip.h` — внутренний интерфейс trip UI  

---

## src/platform/

`src/platform/` — платформенные реализации HAL и системных функций

### src/platform/pc/

`src/platform/pc/` — реализации PurrGo для PC
`display_hal.c` — PC-реализация display HAL  
`fs_hal.c` — PC-реализация файлового HAL  
`gnss_io.c` — PC-ввод GNSS  
`serial_hal.c` — абстракция последовательного порта на PC  
`serial_hal.h` — интерфейс PC serial HAL  
`system_time.c` — реализация системного времени на PC  

---

## tools/

`tools/` — вспомогательные инструменты разработки PurrGo

`tools/README.md` — описание инструментов проекта  
`tools/font_viewer.py` — просмотр растрового шрифта  
`tools/generate_sun_tables.py` — генератор таблиц солнечных расчётов  
`tools/map-viewer.py` — просмотр карт PurrGo  
`tools/osm_geometry_stats.py` — статистика геометрии OSM-данных  
`tools/purrgo_font_editor.py` — редактор растрового шрифта PurrGo  
`tools/purrgo_icon_editor11.py` — редактор иконок PurrGo с размером 11×11  
`tools/purrgo_icon_editor7.py` — редактор иконок PurrGo с размером 7×7  
`tools/purrgo_icon_viewer.py` — просмотр иконок PurrGo  

### tools/map-compiler/

`tools/map-compiler/` — компилятор OSM-данных в бинарный формат карт PurrGo

`PurrGO Map Compiler.md` — описание компилятора карт  
`features.csv` — таблица поддерживаемых картографических feature codes  
`purrgo_bin_writer.py` — запись бинарного формата PurrGo Map V3  
`purrgo_geometry.py` — операции над геометрией  
`purrgo_lookup.py` — таблицы и операции поиска  
`purrgo_map_compiler.py` — основной интерфейс компилятора карт  
`purrgo_models.py` — модели данных компилятора  
`purrgo_osm_optimizer.py` — оптимизация OSM-данных  
`purrgo_osmparser.py` — разбор OSM-данных  
`requirements.txt` — зависимости Python-инструментов компилятора  
`utf8_to_pgo.py` — преобразование UTF-8 данных в формат PurrGo  

---

## third_party/

`third_party/` — сторонние зависимости проекта

`third_party/minmea` — сторонняя библиотека разбора NMEA, подключённая как Git submodule

---
