# PurrGo — описание файлов и каталогов

purrgo_file_descriptions.txt этот файл
HARDWARE.md описание архитектуры аппаратной части, платформ разработки и планируемого устройства
README.md основное описание PurrGo, архитектуры, сборки и текущего назначения проекта
ai_project_dump.py генератор текстового дампа исходных файлов проекта для передачи AI
Руководство по компиляции.md инструкция по сборке и отладке PurrGo в Visual Studio Code на Windows

apps/ приложения и исполняемые точки входа проекта
apps/emulator/ PC-эмулятор навигатора с виртуальным дисплеем и органами управления
apps/emulator/include/ заголовочные файлы интерфейса эмулятора
apps/emulator/include/display.h интерфейс виртуального дисплея и его framebuffer
apps/emulator/include/emu_fs.h интерфейс файлового адаптера эмулятора
apps/emulator/include/emu_window.h интерфейс SDL-окна, рендеринга и обработки кнопок
apps/emulator/src/ исходные файлы PC-эмулятора
apps/emulator/src/display.c реализация виртуального дисплея с 2-битным framebuffer
apps/emulator/src/emu_fs.c адаптер файловой системы между эмулятором и HAL PurrGo
apps/emulator/src/emu_window.c SDL-окно эмулятора, вывод framebuffer и обработка виртуальных кнопок
apps/emulator/src/main.c точка входа эмулятора и связывание GNSS, UI, графики, карты и виртуального дисплея
apps/pc/ PC-приложения проекта
apps/pc/main.c простая точка входа основного PC-приложения PurrGo
apps/pc/pc_realtime_logger/ PC-приложение для работы с реальным GNSS-приёмником через последовательный порт
apps/pc/pc_realtime_logger/main.c точка входа realtime GNSS logger
apps/stm32/ каталог приложения для STM32
apps/stm32/README.md описание текущего состояния STM32 application layer

docs/ проектная документация и технические материалы
docs/E-ink_interface.webp иллюстрация интерфейса E-Ink дисплея
docs/GPS Модуль Quescan G10A F30.png изображение GNSS-модуля Quescan G10A F30
docs/PowerManagement_AppNote_(UBX-13005162).pdf application note u-blox по управлению питанием GNSS-приёмников
docs/UBX-G7020-KT.PDF документация на GNSS-чип/платформу u-blox G7020-KT
docs/architecture.md описание программной архитектуры и правил разделения portable core и platform layer
docs/docs-map-format.md подробное описание текущего формата карт PurrGo и его файлов
docs/dtg1_map_specification.md спецификация исходного DTG1/map формата, взятого за основу формата карт PurrGo
docs/gnss-configuration.md описание конфигурации GNSS-приёмника и связанных параметров
docs/import-papertrail.md анализ Papertrail и перечень идей/компонентов, которые можно перенести в PurrGo
docs/u-blox7-V14_ReceiverDescriptionProtocolSpec_(GPS.G7-SW-12001)_Public.pdf спецификация протокола и возможностей u-blox 7
include/ публичные заголовочные файлы проекта
include/purrgo/ публичный API и структуры PurrGo
include/purrgo/app_fsm.h интерфейс конечного автомата приложения и состояний пользовательского интерфейса
include/purrgo/app_ui.h интерфейс прикладного UI
include/purrgo/config.h compile-time конфигурация приложения и дисплея
include/purrgo/font5x7.h таблица растрового шрифта 5x7
include/purrgo/fs_hal.h абстракция файловой системы для portable core
include/purrgo/geo.h интерфейс географических вычислений
include/purrgo/gfx_circle.h интерфейс рисования окружностей
include/purrgo/gfx_line.h интерфейс рисования линий
include/purrgo/gfx_polygon.h интерфейс рисования полигонов
include/purrgo/gfx_rect.h интерфейс рисования прямоугольников
include/purrgo/gfx_renderer.h общий интерфейс графического рендерера и контекста рисования
include/purrgo/gfx_text.h интерфейс вывода растрового текста
include/purrgo/gnss.h базовый интерфейс GNSS-подсистемы
include/purrgo/gnss_adapter.h интерфейс адаптера входного потока GNSS
include/purrgo/gnss_config.h параметры конфигурации GNSS
include/purrgo/gnss_mock.h интерфейс генератора тестовых/mock GNSS-данных
include/purrgo/gnss_types.h структуры и типы данных GNSS solution
include/purrgo/gpx_parser.h интерфейс разбора GPX-данных
include/purrgo/hardware_config.h настройка компиляции под разное "железо"
include/purrgo/map.h интерфейс загрузки, чтения и работы с картой
include/purrgo/navigation.h интерфейс навигационного состояния и расчётов
include/purrgo/purrgo_time.h типы и функции работы со временем проекта
include/purrgo/sun.h интерфейс расчёта восхода, заката и солнечных событий
include/purrgo/sun_tables.h интерфейс таблиц солнечных расчётов
include/purrgo/track_logger.h интерфейс записи и обработки GPS-трека
include/purrgo/types.h общие типы данных проекта
include/purrgo/ubx.h интерфейс работы с сообщениями протокола u-blox UBX

src/ исходный код реализации проекта
src/core/ переносимое аппаратно-независимое ядро PurrGo
src/core/app_fsm.c реализация конечного автомата приложения и переходов между состояниями
src/core/app_ui.c реализация пользовательского интерфейса навигатора
src/core/geo.c географические вычисления: расстояния, направления и координатные преобразования
src/core/gfx/ графическая примитивная библиотека portable core
src/core/gfx/gfx_circle.c алгоритмы рисования окружностей
src/core/gfx/gfx_line.c алгоритмы рисования линий
src/core/gfx/gfx_polygon.c алгоритмы рисования полигонов
src/core/gfx/gfx_rect.c алгоритмы рисования прямоугольников
src/core/gfx/gfx_renderer.c реализация графического контекста и вывода примитивов через callback
src/core/gfx/gfx_text.c реализация вывода текста с использованием растрового шрифта
src/core/gnss.c базовая реализация GNSS-подсистемы
src/core/gnss_adapter.c преобразование входных GNSS/NMEA данных в внутреннее GNSS solution
src/core/gnss_mock.c генератор mock GNSS-данных для PC-тестирования и эмулятора
src/core/gpx_parser.c разбор GPX и извлечение трековых данных
src/core/map.c реализация чтения, индексации и рендеринга карт PurrGo
src/core/navigation.c логика навигационного состояния и базовых навигационных вычислений
src/core/purrgo_time.c функции преобразования и обработки времени GNSS
src/core/sun.c расчёт времени восхода, заката и других солнечных событий по координатам и дате
src/core/sun_tables.c табличные данные для расчётов солнечных событий
src/core/track_logger.c логика накопления, фильтрации и записи точек трека
src/core/ubx.c разбор/обработка сообщений протокола u-blox UBX

src/platform/ платформенные адаптеры, отделяющие core от конкретного оборудования и ОС
src/platform/pc/ реализация platform layer для PC
src/platform/pc/fs_hal.c реализация файлового HAL через файловую систему хоста
src/platform/pc/platform_pc.c базовые функции платформы PC
src/platform/pc/platform_pc.h публичный интерфейс PC platform layer
src/platform/pc/serial_hal.c реализация последовательного порта для PC
src/platform/pc/serial_hal.h интерфейс HAL последовательного порта
src/platform/pc/serial_win32.c Windows-специфичная реализация/адаптер serial-порта
src/platform/stm32/ платформа для микроконтроллеров STM32
src/platform/stm32/platform_stm32.c базовая реализация platform layer для STM32
src/platform/stm32/platform_stm32.h интерфейс STM32 platform layer
src/platform/ublox/ код, специфичный для GNSS-приёмников u-blox
src/platform/ublox/ublox7_config.c конфигурация u-blox 7 через команды/параметры приёмника
src/platform/ublox/ublox7_config.h интерфейс конфигурации u-blox 7

tests/ автоматические тесты проекта
tests/core/ тесты аппаратно-независимого ядра
tests/core/test_geo.c тесты географических вычислений
tests/core/test_gnss.c тесты разбора и формирования GNSS solution
tests/core/test_map.c тесты чтения и обработки карт
tests/data/ входные данные для автоматических тестов
tests/data/maps/ тестовые файлы карт
tests/data/maps/landuse.db база данных названий объектов landuse тестовой карты
tests/data/maps/landuse.idx индекс объектов landuse тестовой карты
tests/data/maps/landuse.mlp геометрические данные landuse тестовой карты
tests/data/maps/map.name имя/метаданные тестового набора карты
tests/data/maps/pois.db база данных названий POI тестовой карты
tests/data/maps/pois.idx индекс POI тестовой карты
tests/data/maps/roads.db база данных названий дорог тестовой карты
tests/data/maps/roads.idx индекс дорог тестовой карты
tests/data/maps/roads.mlp геометрические данные дорог тестовой карты
tests/data/maps/water.db база данных названий водных объектов тестовой карты
tests/data/maps/water.idx индекс водных объектов тестовой карты
tests/data/maps/water.mlp геометрические данные водных объектов тестовой карты

third_party/ внешние зависимости проекта
third_party/README.md описание сторонних зависимостей
third_party/minmea/ Git submodule библиотеки minmea для разбора NMEA

tools/ инструменты подготовки данных и вспомогательные программы разработки
tools/generate_sun_tables.py Python-генератор таблиц, используемых расчётом солнечных событий
tools/map-parser/ Python-инструменты разбора/подготовки карт
tools/map-parser/dtmap-parser.py парсер DTG1/DT map-данных и генератор подготовленного представления
