# PurrGo — описание файлов и каталогов

`purrgo_file_descriptions.md` — этот файл  
`HARDWARE.md` — описание архитектуры аппаратной части, платформ разработки и планируемого устройства  
`README.md` — основное описание PurrGo, архитектуры, сборки и текущего назначения проекта  
`TODO.md` — текущий список задач и направлений дальнейшей разработки  
`ai_project_dump.py` — генератор текстового дампа исходных файлов проекта для передачи AI  
`CMakeLists.txt` — корневой CMake-файл сборки проекта  
`LICENSE` — лицензия проекта  

---

## apps/

`apps/` — приложения и исполняемые точки входа проекта

### apps/emulator/

`apps/emulator/` — PC-эмулятор навигатора с виртуальным дисплеем, GNSS, картами и органами управления  
`apps/emulator/CMakeLists.txt` — CMake-конфигурация сборки эмулятора  

#### apps/emulator/include/

`apps/emulator/include/` — заголовочные файлы интерфейса эмулятора

`apps/emulator/include/display.h` — интерфейс виртуального дисплея и его framebuffer  
`apps/emulator/include/display.h.orig` — сохранённая предыдущая версия заголовка виртуального дисплея  
`apps/emulator/include/emu_fs.h` — интерфейс файлового адаптера эмулятора  
`apps/emulator/include/emu_window.h` — интерфейс SDL-окна, рендеринга и обработки виртуальных кнопок  

#### apps/emulator/src/

`apps/emulator/src/` — исходные файлы PC-эмулятора

`apps/emulator/src/display.c` — реализация виртуального дисплея и framebuffer  
`apps/emulator/src/emu_fs.c` — адаптер файловой системы между эмулятором и HAL PurrGo  
`apps/emulator/src/emu_window.c` — SDL-окно эмулятора, вывод framebuffer и обработка виртуальных кнопок  
`apps/emulator/src/main.c` — точка входа эмулятора и связывание GNSS, UI, графики, карт и виртуального дисплея  

---

### apps/pc/

`apps/pc/` — PC-приложения проекта

`apps/pc/main.c` — простая точка входа основного PC-приложения PurrGo  

#### apps/pc/pc_realtime_logger/

`apps/pc/pc_realtime_logger/` — PC-приложение для работы с реальным GNSS-приёмником через последовательный порт  

`apps/pc/pc_realtime_logger/main.c` — точка входа realtime GNSS logger  

---

### apps/stm32/

`apps/stm32/` — приложение для STM32

`apps/stm32/README.md` — описание текущего состояния STM32 application layer  

---

## docs/

`docs/` — проектная документация и технические материалы

`docs/DS_stm32u585ci.pdf` — документация на STM32U585CI  
`docs/GPS Модуль Quescan G10A F30.png` — изображение используемого/планируемого GNSS-модуля Quescan G10A F30  
`docs/PowerManagement_AppNote_(UBX-13005162).pdf` — application note u-blox по управлению питанием GNSS-приёмников  
`docs/PurrGO Map Format V3 — Binary Format Conformance.md` — требования и тесты соответствия реализации формату PurrGo Map V3  
`docs/PurrGO_font_encoding.md` — описание кодирования и формата растрового шрифта PurrGo  
`docs/UBX-G7020-KT.PDF` — документация на GNSS-чип/платформу u-blox G7020-KT  
`docs/USB-UART adapter.md` — описание USB-UART адаптера для разработки и тестирования  
`docs/architecture.md` — описание программной архитектуры, границ модулей и правил разделения portable core и platform layer  
`docs/display-e-ink-paper-hat-2n7in-user-manual.pdf` — руководство Waveshare для 2.7" e-Paper HAT  
`docs/gnss-configuration.md` — описание конфигурации GNSS-приёмника и связанных параметров  
`docs/purrgo_map_specification_v3.md` — техническая спецификация бинарного формата карт PurrGo V3  
`docs/u-blox7-V14_ReceiverDescriptionProtocolSpec_(GPS.G7-SW-12001)_Public.pdf` — спецификация протокола и возможностей u-blox 7  

---

## include/

`include/` — публичные заголовочные файлы проекта

### include/purrgo/

`include/purrgo/` — публичный API и структуры PurrGo

`include/purrgo/app_fsm.h` — интерфейс конечного автомата приложения и состояний пользовательского интерфейса  
`include/purrgo/app_ui.h` — интерфейс прикладного UI  
`include/purrgo/config.h` — compile-time конфигурация приложения  
`include/purrgo/config_controller.h` — интерфейс управления конфигурацией приложения  
`include/purrgo/display_hal.h` — аппаратно-независимый интерфейс дисплея  
`include/purrgo/font5x7.h` — интерфейс/таблица растрового шрифта 5x7  
`include/purrgo/fs_hal.h` — абстракция файловой системы для portable core  
`include/purrgo/geo.h` — интерфейс географических вычислений  
`include/purrgo/gfx_circle.h` — интерфейс рисования окружностей  
`include/purrgo/gfx_line.h` — интерфейс рисования линий, включая специальные типы линий  
`include/purrgo/gfx_polygon.h` — интерфейс рисования и заполнения полигонов  
`include/purrgo/gfx_rect.h` — интерфейс рисования прямоугольников  
`include/purrgo/gfx_renderer.h` — общий интерфейс графического рендерера, цветов и контекста рисования  
`include/purrgo/gfx_text.h` — интерфейс вывода растрового текста  
`include/purrgo/gnss.h` — базовый интерфейс GNSS-подсистемы  
`include/purrgo/gnss_adapter.h` — интерфейс адаптера входного потока GNSS  
`include/purrgo/gnss_config.h` — параметры конфигурации GNSS  
`include/purrgo/gnss_mock.h` — интерфейс генератора тестовых/mock GNSS-данных  
`include/purrgo/gnss_types.h` — структуры и типы GNSS solution  
`include/purrgo/gpx_parser.h` — интерфейс разбора GPX-данных  
`include/purrgo/hardware_config.h` — настройка компиляции под разное аппаратное обеспечение  
`include/purrgo/logger.h` — интерфейс системы журналирования  
`include/purrgo/map.h` — публичный интерфейс map subsystem  
`include/purrgo/map_controller.h` — интерфейс управления состоянием и отображением карты  
`include/purrgo/map_style.h` — определения PurrGo feature codes, стилей карт и API сопоставления feature code → render style  
`include/purrgo/navigation.h` — интерфейс навигационного состояния и расчётов  
`include/purrgo/purrgo_time.h` — типы и функции работы со временем проекта  
`include/purrgo/sun.h` — интерфейс расчёта восхода, заката и солнечных событий  
`include/purrgo/sun_tables.h` — интерфейс таблиц солнечных расчётов  
`include/purrgo/track_logger.h` — интерфейс записи и обработки GPS-трека  
`include/purrgo/trip_computer.h` — интерфейс расчёта параметров поездки/трека  
`include/purrgo/types.h` — общие типы данных проекта  
`include/purrgo/ubx.h` — интерфейс работы с сообщениями протокола u-blox UBX  

---

## src/

`src/` — исходный код реализации проекта

### src/core/

`src/core/` — переносимое аппаратно-независимое ядро PurrGo

`src/core/app_fsm.c` — реализация конечного автомата приложения и переходов между состояниями  
`src/core/app_ui.c` — прикладной слой UI и его интеграция с core  
`src/core/config.c` — реализация конфигурации приложения  
`src/core/config_controller.c` — реализация управления конфигурацией приложения  
`src/core/geo.c` — географические вычисления: расстояния, направления и координатные преобразования  

---

### src/core/gfx/

`src/core/gfx/` — графическая библиотека portable core

`src/core/gfx/font5x7.c` — данные растрового шрифта 5x7  
`src/core/gfx/gfx_circle.c` — алгоритмы рисования окружностей и заполненных окружностей  
`src/core/gfx/gfx_line.c` — алгоритмы рисования обычных, толстых, пунктирных, точечных и железнодорожных линий  
`src/core/gfx/gfx_polygon.c` — алгоритмы рисования и заполнения полигонов  
`src/core/gfx/gfx_rect.c` — алгоритмы рисования прямоугольников  
`src/core/gfx/gfx_renderer.c` — реализация графического контекста, цветов, clipping и вывода примитивов через callback  
`src/core/gfx/gfx_text.c` — реализация вывода текста с использованием растрового шрифта  

---

### src/core/map/

Map subsystem в текущем проекте разделён на специализированные модули.

`src/core/map.c` — публичный/координирующий слой map subsystem  
`src/core/map_controller.c` — управление состоянием карты, viewport, pan/follow и связанными операциями  
`src/core/map_culling.c` — пространственное отсечение объектов карты  
`src/core/map_culling.h` — внутренний интерфейс map culling  
`src/core/map_idx.c` — чтение и обход `.idx`, SQT/R-tree и пространственного индекса  
`src/core/map_idx.h` — внутренний интерфейс `.idx` subsystem  
`src/core/map_internal.h` — внутренние типы и интерфейсы map subsystem  
`src/core/map_mlp.c` — чтение геометрических записей `.mlp`  
`src/core/map_mlp.h` — внутренний интерфейс `.mlp` subsystem  
`src/core/map_projection.c` — преобразование географических координат в координаты viewport  
`src/core/map_projection.h` — внутренний интерфейс map projection  
`src/core/map_render.c` — преобразование map objects в операции графического рендера  
`src/core/map_render.h` — внутренний интерфейс map rendering  
`src/core/map_style.c` — таблица соответствия PurrGo feature code → render style  

---

### src/core/navigation.c

`src/core/navigation.c` — логика навигационного состояния и базовых навигационных вычислений  

---

### src/core/track/navigation/time/sun

`src/core/navigation.c` — базовые навигационные расчёты и состояние навигации  
`src/core/purrgo_time.c` — функции преобразования и обработки времени GNSS  
`src/core/sun.c` — расчёт времени восхода, заката и других солнечных событий по координатам и дате  
`src/core/sun_tables.c` — табличные данные для расчётов солнечных событий  
`src/core/track_logger.c` — логика накопления, фильтрации и записи точек трека  
`src/core/trip_computer.c` — расчёт параметров текущей поездки/движения  
`src/core/ubx.c` — разбор и обработка сообщений протокола u-blox UBX  

---

### src/core/gnss/

`src/core/gnss.c` — базовая реализация GNSS-подсистемы  
`src/core/gnss_adapter.c` — преобразование входных GNSS/NMEA данных во внутренний GNSS solution  
`src/core/gnss_mock.c` — генератор mock GNSS-данных для PC-тестирования и эмулятора  
`src/core/gpx_parser.c` — разбор GPX и извлечение трековых данных  
`src/core/utf8rus.c` — преобразование/обработка UTF-8 русских символов для использования PurrGo  

---

### src/core/ui/

`src/core/ui/` — специализированные модули пользовательского интерфейса

`src/core/ui/ui_config.c` — реализация UI-конфигурации  
`src/core/ui/ui_config.h` — внутренний интерфейс UI configuration  
`src/core/ui/ui_dir_select.c` — UI выбора каталога/файловой директории  
`src/core/ui/ui_dir_select.h` — интерфейс UI выбора каталога  
`src/core/ui/ui_map.c` — UI экрана карты и взаимодействие с map controller  
`src/core/ui/ui_map.h` — интерфейс map UI  
`src/core/ui/ui_trip.c` — UI экрана информации о поездке  
`src/core/ui/ui_trip.h` — интерфейс trip UI  

---

## src/platform/

`src/platform/` — платформенные адаптеры, отделяющие core от конкретного оборудования и ОС

### src/platform/pc/

`src/platform/pc/` — реализация platform layer для PC

`src/platform/pc/display_hal.c` — PC-реализация display HAL  
`src/platform/pc/fs_hal.c` — реализация файлового HAL через файловую систему хоста  
`src/platform/pc/platform_pc.c` — базовые функции платформы PC  
`src/platform/pc/platform_pc.h` — публичный интерфейс PC platform layer  
`src/platform/pc/serial_hal.c` — реализация последовательного порта для PC  
`src/platform/pc/serial_hal.h` — интерфейс HAL последовательного порта  

---

### src/platform/stm32/

`src/platform/stm32/` — платформа для микроконтроллеров STM32

`src/platform/stm32/platform_stm32.c` — базовая реализация platform layer для STM32  
`src/platform/stm32/platform_stm32.h` — интерфейс STM32 platform layer  

На текущем состоянии репозитория полноценная аппаратная реализация приложения для STM32 ещё не представлена; каталог содержит platform layer, отделяющий portable core от будущей конкретной STM32 application implementation.

---

### src/platform/ublox/

`src/platform/ublox/` — код, специфичный для GNSS-приёмников u-blox

`src/platform/ublox/ublox7_config.c` — конфигурация u-blox 7 через команды/параметры приёмника  
`src/platform/ublox/ublox7_config.h` — интерфейс конфигурации u-blox 7  

---

## tests/

`tests/` — автоматические тесты и тестовые данные проекта

`tests/audit_maps.py` — Python-аудитор тестовых карт и их бинарного содержимого  

### tests/core/

`tests/core/` — тесты аппаратно-независимого ядра

`tests/core/test_app_fsm.c` — тесты конечного автомата приложения  
`tests/core/test_config.c` — тесты конфигурации приложения  
`tests/core/test_geo.c` — тесты географических вычислений  
`tests/core/test_gfx_clipping.c` — тесты clipping графического рендера  
`tests/core/test_gfx_polygon.c` — тесты полигонального рендера  
`tests/core/test_gnss.c` — тесты GNSS solution и GNSS pipeline  
`tests/core/test_map_lod.c` — тесты LOD map subsystem  
`tests/core/test_purrgo_time.c` — тесты обработки времени  
`tests/core/test_track_logger.c` — тесты track logger  

---

## tests/data/maps/

`tests/data/maps/` — набор тестовых карт и исходных OSM-данных для проверки map compiler и firmware map parser

---

## third_party/

`third_party/` — внешние зависимости проекта

`third_party/CMakeLists.txt` — CMake-конфигурация сторонних зависимостей  
`third_party/minmea/` — Git submodule библиотеки minmea для разбора NMEA  

---

## tools/

`tools/` — инструменты подготовки данных, тестирования и вспомогательные программы разработки

`tools/README.md` — описание инструментов проекта  
`tools/font_viewer.py` — просмотрщик растрового шрифта PurrGo  
`tools/generate_sun_tables.py` — Python-генератор таблиц, используемых расчётом солнечных событий  
`tools/purrgo_font_editor.py` — редактор растрового шрифта PurrGo  

---

## tools/map-compiler/

`tools/map-compiler/` — текущий Python-компилятор карт PurrGo

`tools/map-compiler/README.md` — описание компилятора карт, workflow и формата выходных данных  
`tools/map-compiler/features.csv` — таблица классификации OSM objects в PurrGo feature definitions  
`tools/map-compiler/purrgo_bin_writer.py` — запись бинарных `.idx`, `.mlp`, `.db` и связанных структур  
`tools/map-compiler/purrgo_geometry.py` — операции с геометрией при компиляции карт  
`tools/map-compiler/purrgo_lookup.py` — классификация OSM объектов и поиск подходящего feature rule  
`tools/map-compiler/purrgo_map_compiler.py` — основной entry point компилятора карт  
`tools/map-compiler/purrgo_models.py` — внутренние модели данных компилятора  
`tools/map-compiler/purrgo_osmparser.py` — разбор OSM XML и преобразование объектов во внутренние map features  
`tools/map-compiler/requirements.txt` — Python-зависимости компилятора  
`tools/map-compiler/test_purrgo_bin_writer.py` — тесты бинарного writer  
`tools/map-compiler/test_purrgo_models.py` — тесты внутренних моделей компилятора  
`tools/map-compiler/utf8_to_pgo.py` — преобразование UTF-8 текста в представление PurrGo  

---

## tools/map-parser/

`tools/map-parser/` — инструменты разбора и визуализации карт

`tools/map-parser/dtmap-parser.py` — Python-парсер DTG1/PurrGo map-данных и визуальный renderer подготовленного представления карты  
