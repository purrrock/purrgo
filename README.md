# PurrGO

**PurrGO** — автономный ультра-энергоэффективный GNSS-навигатор и трекер.

Основная идея:

> **Офлайн-навигация с минимальным энергопотреблением, без расчёта маршрутов.**

PurrGO предназначен для:

- GNSS-позиционирования;
- отображения офлайн-векторных карт;
- записи треков;
- работы с Waypoint;
- базовой навигации на Waypoint;
- полностью автономной работы без Internet и облачных сервисов.

PurrGO **не выполняет turn-by-turn route calculation**. Это навигатор и трекер, а не routing engine.

### Что уже есть в прошивке

- **Карта**: предкомпилированные векторные карты Map Format V3 (слои landuse, water, waterways, roads, POI),
  4 уровня серого, автоследование за GNSS и ручной сдвиг (в ручном режиме в центре карты перекрестие),
  включение слоёв в меню MAP LAYERS.
- **Trip computer**: время (UTC и местное), скорость, курс, координаты, высота, качество фикса,
  цель навигации (расстояние и пеленг).
- **Запись трека** в GPX на microSD с фильтрацией точек (режимы OFF / STANDARD / EXPEDITION) и режим
  **трекера с отключённым дисплеем** (пункт меню TRACKER (SCREEN OFF), выход - K1+K4).
- **Путевые точки и маршруты**: цель навигации (WAYPOINT: HERE, MAP CENTER, точка из GPX), слой Waypoints,
  слой Route из GPX-файла.
- **Читалка книг** (BOOKS): формат PGB, поиск по книге, GNSS на время чтения выключен.
- **Питание**: выбор типа батарей (шесть источников) с кривыми разряда, остановка записи на SD при низком
  напряжении, выключение (POWER OFF) с финальным экраном.

Подробности по кнопкам - [`docs/user-guide.md`](docs/user-guide.md), по читалке - [`docs/e-books.md`](docs/e-books.md).

---

## Статус проекта

Проект находится в активной разработке.
Текущий этап — реализация firmware для **STM32U585CIU6**.

Текущий список работ находится в [`TODO.md`](TODO.md).

---

## Архитектура

Программная архитектура разделена на portable core и платформенный код:

```text
+---------------------------+
|        Application        |
+-------------+-------------+
              |
              v
+---------------------------+
|      Platform layer       |
|        PC / STM32         |
+-------------+-------------+
              |
              v
+---------------------------+
|       Portable core       |
| GNSS / map / track / geo  |
| navigation / graphics     |
+---------------------------+
```

Подробное описание архитектуры и правил разделения находится в [`docs/architecture.md`](docs/architecture.md).

---

## Release hardware

MCU: **STM32U585CIU6**, GNSS: G10A F30 (u-blox M10), дисплей: Waveshare 2.7" e-Paper HAT (176 × 264, 4 gray),
microSD по SPI2.

Подробности аппаратной части находятся в [`docs/hardware.md`](docs/hardware.md).

---

## Maps

Карты подготавливаются на PC и затем используются устройством:

```text
OSM data
   │
   ▼
Map Optimizer
   │
   ▼
Map compiler
   │
   ▼
PurrGO map package
   │
   ▼
microSD
   │
   ▼
STM32
   │
   ▼
Map renderer
   │
   ▼
E-Ink display
```

STM32 использует **предкомпилированные векторные карты** и не выполняет общее GIS-процессирование.

Карта состоит из package-level metadata `map.name` и бинарных файлов слоёв:

```text
map.name
*.idx
*.mlp
*.db
```

Подробности:

- [`docs/map-format-v3.md`](docs/map-format-v3.md) — нормативная спецификация Map Format V3;
- [`docs/map-conformance.md`](docs/map-conformance.md) — требования соответствия;
- [`tools/map-compiler/README.md`](tools/map-compiler/README.md) — как создать карту для PurrGO.

---

## Repository structure

```text
purrgo/
├── apps/
│   ├── emulator/           # PC-эмулятор (SDL2)
│   └── stm32/              # Прошивка STM32U585
│       ├── common/         # STM32-адаптеры: дисплей, кнопки, ФС, логгер, SD по SPI, FatFs
│       └── PurrGO_STM32U5/ # CubeMX-проект; Core/Src: GNSS (UART+DMA), SD, питание
│
├── docs/                   # Документация: hardware, user-guide, architecture, форматы карты, design/, datasheets/
├── include/
│   └── purrgo/             # Public interfaces
│
├── src/
│   ├── core/               # Portable core
│   └── platform/
│       └── pc/             # PC-адаптеры (ФС, serial, дисплей, логгер)
│
├── tests/
│   ├── host/               # Host-тесты ядра (ctest, ASan/UBSan) и тестовые данные
│   └── audit_maps.py       # Аудит бинарных файлов карт
├── third_party/            # External dependencies (сабмодули: minmea, eink_part_refresh (драйвер E-Ink epd4g))
├── tools/                  # Компиляторы карт и книг, анализ треков, калибровка батареи, редакторы шрифта и иконок
│
├── cmake/
│   ├── toolchains/         # CMake toolchain file (STM32U5)
│   └── PurrgoWarnings.cmake# Флаги предупреждений проекта
│
├── .github/workflows/      # CI (GitHub Actions)
├── CMakeLists.txt          # Единая точка входа сборки
├── CMakePresets.json       # Пресеты: win-mingw-*, linux-gcc-*, stm32u5-*
├── PurrGO_STM32U585_PINOUT.md
├── TODO.md
└── README.md
```

---

## Building

Инициализация сабмодулей:

```sh
git submodule update --init --recursive
```

Проект собирается из **корня репозитория**. Что именно собирать (PC-эмулятор или
прошивку STM32, Debug или Release) выбирается **CMake-пресетом** из
[`CMakePresets.json`](CMakePresets.json), а не выбором папки проекта.

| Пресет | Что собирается | Окружение |
|---|---|---|
| `win-mingw-debug`, `win-mingw-release` | PC-эмулятор (SDL2) | Windows, MSYS2 ucrt64 (`C:/msys64/ucrt64`) |
| `linux-gcc-debug`, `linux-gcc-release` | PC-эмулятор (SDL2) | Linux, GCC, Ninja, `libsdl2-dev` |
| `linux-gcc-test` | Только ядро + host-тесты, ASan/UBSan (без SDL2) | Linux, GCC, Ninja |
| `stm32u5-debug`, `stm32u5-release` | Прошивка STM32U585 | `arm-none-eabi-gcc` в `PATH`, Ninja |
| `web-release` | Эмулятор в браузере (WebAssembly) | Emscripten, см. [`docs/WEB_EMULATOR.md`](docs/WEB_EMULATOR.md) |

Пресеты, неподходящие для текущей ОС, в списке не показываются.

### Visual Studio Code

Откройте корень репозитория (или `purrgo.code-workspace`), установите расширение
CMake Tools и выберите пресет в строке состояния. Для STM32 расширение
STM32Cube for VS Code предоставляет `cube-cmake` (см. `.vscode/settings.json`).
Отладка прошивки — конфигурация `Cortex Debug: OpenOCD (PurrGO)`: она берёт `.elf`
активного пресета.

### Командная строка

```sh
cmake --preset stm32u5-debug
cmake --build --preset stm32u5-debug
```

Уровень логирования UART/stderr (`include/purrgo/logger.h`) выбирается кэш-переменной
`PURRGO_LOG_LEVEL`: 0 NONE, 1 ERROR, 2 WARN, 3 INFO, 4 DEBUG. По умолчанию Release собирается
с уровнем 1 (только ошибки, светодиод PC13 не мигает), остальные типы сборки - с 3. Пример:
`cmake --preset stm32u5-release -DPURRGO_LOG_LEVEL=3`.

Другие параметры сборки прошивки:

| Параметр | Назначение |
|---|---|
| `-DPURRGO_HW_TEST=SD_SPEED\|EINK_4GRAY\|EINK_PARTIAL` | Аппаратный тест вместо навигатора, см. [`docs/hardware.md`](docs/hardware.md) |
| `-DPURRGO_TRACK_DIAG=ON` | Диагностическая запись трека (каждая эпоха GNSS), см. [`docs/track-field-test.md`](docs/track-field-test.md) |
| `-DPURRGO_WERROR=ON` | Предупреждения - ошибки (кроме `-Wconversion`) |

Результат сборки: `out/build/<пресет>/`. Прошивка — `out/build/stm32u5-*/PurrGO_STM32U5.elf`,
эмулятор — `out/build/<пресет>/apps/emulator/purrgo_emulator`.

Платформу выбирает переменная `PURRGO_PLATFORM` (`pc` | `stm32u5`), которую задаёт
пресет. Без пресета `cmake -S . -B build` собирает PC-эмулятор.

### STM32CubeMX

CubeMX-проект находится в `apps/stm32/PurrGO_STM32U5/` (`PurrGO_STM32U5.ioc`, `Core/`, `Drivers/`, ...).
Перегенерация кода обновляет `cmake/stm32cubemx/CMakeLists.txt` и
`cmake/gcc-arm-none-eabi.cmake` в этой папке — они подхватываются автоматически.
`apps/stm32/PurrGO_STM32U5/CMakeLists.txt` (пользовательский, CubeMX его не перезаписывает)
подключается из корневого `CMakeLists.txt` и самостоятельно не конфигурируется.

---

## Тестирование

Host-тесты проверяют переносимое ядро на ПК, без железа и без SDL2. Код запускается под
AddressSanitizer и UndefinedBehaviorSanitizer; платформенный слой подменяется фейковой ФС
с внедрением сбоев (`tests/host/fake_fs.c`).

```sh
cmake --preset linux-gcc-test
cmake --build --preset linux-gcc-test
ctest --preset linux-gcc-test
```

Без CMake (только `gcc`, из корня репозитория): `sh tests/host/run.sh`.

| Тест | Что проверяет |
|---|---|
| `test_gnss` | Кольцевой буфер приёма GNSS (`purrgo/gnss_ring.h`), разбор NMEA на записанном логе `tests/host/data/gnss_sample.nmea`, повреждённые и усечённые фразы, GSA от разных навигационных систем |
| `test_gnss_receiver` | Настройка приёмника: формат UBX-кадра, контрольная сумма Fletcher-8, UBX-CFG-VALSET, стартовая конфигурация G10A F30 |
| `test_gnss_link` | Установление связи с модулем (`gnss_link`): поиск скорости UART, смена 38400 → 115200, откат при неудаче |
| `test_gnss_diag` | Диагностика связи с GNSS: разбор NMEA, контрольная сумма, ошибки UART, определение состояния |
| `test_gnss_pps` | Статистика 1PPS: число фронтов, классификация интервалов, пропуски, переход через 2^32 мс |
| `test_map` | Парсер Map Format V3: корректная карта, повреждённые заголовки PGO/SQT/MLP, лимит глубины дерева, `v3_jump`, усечение и подмена байтов (фаззинг) |
| `test_map_mlp_stream` | Буферизованное чтение геометрии MLP: границы секторов, число обращений к ФС, усечённый файл |
| `test_geometry_clip` | Геометрия далёких вершин: проекция без насыщения int16, отсечение отрезков, заливка составных полигонов против независимого эталона |
| `test_gfx_fast_paths` | Быстрые пути без изменения результата: `fill_span` (2-bpp) против пиксельной заливки на случайных операциях, `project_to_screen` с 32-битным делением против 64-битного эталона |
| `test_marker_pan_battery` | Маркёр GNSS (порог перерисовки), шаг ручного сдвига карты, заряд батареи в строке статуса и в GPX, общий монитор заряда |
| `test_ui_refresh_fixes` | Обновление экрана: смена значения пункта меню, отрезки трека между обновлениями, маркёр, лимит частичных обновлений до полного, сдвиг карты |
| `test_display_eink` | Очередь E-Ink (`display_eink.c`) поверх библиотеки `epd4g` и модели панели: сбросы, активации, deep sleep, объём SPI-данных (нужен сабмодуль `eink_part_refresh`) |
| `test_time_logger` | `purrgo_time` и `track_logger` на границах: високосные годы, 2099, полночь и Новый год с часовым поясом, год < 26, прореживание и фильтры точек (качество фикса, стоянка, выбросы), периодический `sync`, имена файлов, RAM-кольцо трека |
| `test_track_filter` | Фильтр «незначительных изменений» трека: прямая не пишется, поворот и разворот («туда и обратно») пишутся, допуски STANDARD/EXPEDITION, шаг и предельная хорда, потеря направления после стоянки и сброса по выбросам, хорда в пол-Земли без переполнения, теги `<fix>/<sat>/<hdop>`/`pg:spd` в точке GPX |
| `test_track_diag` | Диагностическая сборка трека (`PURRGO_TRACK_DIAG=1`): в GPX каждая эпоха, `pg:rej` показывает причины отбраковки, экранный трек не меняется, `sync` |
| `test_tracker_mode` | Режим «трекер с отключённым дисплеем»: пункт меню, вход (логгер в EXPEDITION, одно предупреждение на экране), запись без перерисовок, выход по K1+K4 |
| `test_navigation` | Расстояние и азимут до точки (`geo.c`, `navigation.c`): эталонные пары, стороны света, 180-й меридиан, радиус прибытия |
| `test_waypoint` | Цель навигации: чтение `<wpt>` из GPX, цель в `PURRGO.CFG`, меню WAYPOINT (HERE, MAP CENTER), запись точки MAP CENTER в WAYPTS |
| `test_wpt_layer` | Слой Waypoints: точки маршрута и всех GPX каталога WAYPTS, фильтр имён, кэш, предел 64 точек, отрисовка иконки |
| `test_route` | Маршрут из GPX: разбор `trkpt`/`rtept`, прореживание, загрузка из каталога ROUTES, отрисовка, ключ `ROUTE_FILE`, выбор файла в меню |
| `test_power_off_screen` | Экран выключения: карта и маркер остаются, верхняя и нижняя строки - инверсный «POWER OFF», масштаб карты сохраняется |
| `test_power_sequence` | Последовательность `purrgo_system_power_off`: логгер остановлен, конфиг записан, файлы закрыты, размонтирование раньше входа в Standby |
| `test_power_source` | Выбор источника питания: `POWER_SRC` в PURRGO.CFG, порядок пунктов главного меню (BOOKS, источник, POWER OFF, TRACKER, TZ, ...), перебор шести источников, кривые разряда и индикация «5V» |
| `test_supply_low` | Низкое напряжение (PVD и АЦП): трек закрывается, карта остаётся доступной для чтения, возобновление записи новым файлом |
| `test_negative_altitude` | Отрицательная высота (-430 м): разбор GGA, перерисовка Trip computer, вывод `-430 m`, `<ele>` в GPX, восход/закат |
| `test_refact_critical` | Исправления критичных ошибок (патч 0007): таймаут фикса, ошибки записи SD, атомарное сохранение конфига, политика перерисовки |
| `test_pgb`, `test_book_reader`, `test_book_search`, `test_book_resume_track` | Читалка: формат PGB (LZ4, CRC), список книг, предзагрузка в RAM, поиск по книге, возобновление трека после выхода из читалки |
| `test_keyboard` | Экранная клавиатура En/Ru: раскладки, ввод, Shift/Caps, курсор, отрисовка |
| `test_pgo_text` | Перекодировка имён с SD (UTF-8 → PurrGO-256) |
| `test_sd_spi` | Протокол microSD в SPI-режиме на модели карты (SDHC, SDSC v1/v2, сбои) |

Python-проверки в CI: `tests/check_pgo_text_table.py` (таблица PurrGO-256 в прошивке и компиляторе совпадает) и
`python3 -m unittest tests/test_book_compiler.py` (конвертер книг). `tests/audit_maps.py` (аудит бинарных файлов карты)
запускается вручную и рекурсивно проверяет все карты в каталоге: `python3 tests/audit_maps.py <каталог с картами>`.

Тесты на железе (SD, дисплей, ток, время кадра) в этот набор не входят.

## CI

`.github/workflows/ci.yml` на каждый push и pull request: host-тесты (ASan/UBSan), сборка
PC-эмулятора, сборка прошивки STM32 (с выводом использования RAM/FLASH), `cppcheck`
(блокирующий) и `clang-tidy` (информативный, набор проверок в `.clang-tidy`).

### Флаги сборки

Код проекта (`src/core`, `src/platform`, `apps/emulator`, пользовательские файлы прошивки)
собирается с `-Wall -Wextra -Wshadow -Wformat=2 -Wconversion -Wdouble-promotion`
(`cmake/PurrgoWarnings.cmake`); сторонний и сгенерированный CubeMX код - без них. Опция
`-DPURRGO_WERROR=ON` превращает предупреждения в ошибки (кроме `-Wconversion`). Для
прошивки `-ffunction-sections -fdata-sections -fstack-usage`, `--gc-sections` и
`--print-memory-usage` задаёт CubeMX-тулчейн; для ПК секции добавляются тем же файлом.

---

## Эмулятор в браузере

Эмулятор собирается в WebAssembly (Emscripten) и публикуется на GitHub Pages
workflow'ом `.github/workflows/pages.yml`: работает на ПК и на смартфоне без установки.
Сборка, настройка Pages и ограничения - в [`docs/WEB_EMULATOR.md`](docs/WEB_EMULATOR.md).

---

## GNSS на ПК

Эмулятор (`apps/emulator`) получает GNSS одним из двух способов:

- по умолчанию (`USE_MOCK_GNSS=ON`) - встроенный генератор NMEA;
- с реального приёмника через последовательный порт: сконфигурируйте с `-DUSE_MOCK_GNSS=OFF`
  и запустите `purrgo_emulator COM3` (`COM3` замените на порт приёмника, скорость 9600).

Так GNSS pipeline проверяется на реальном приёмнике до проверки на STM32.

Единственный источник времени ядра - `purrgo_system_time_ms()` (на ПК это монотонные
миллисекунды, `src/platform/pc/system_time.c`); эмулятор передаёт в `purrgo_app_tick()`
именно его. Смешивать его с другими часами (например `SDL_GetTicks()`) нельзя: ядро
сравнивает отметки времени, и фикс GNSS ложно сбрасывается по таймауту.

Стартовую точку mock-приёмника можно задать при конфигурации. Например, берег Мёртвого
моря (отрицательная высота):

```sh
cmake --preset linux-gcc-debug -DPURRGO_MOCK_LAT_1E7=315590000 \
      -DPURRGO_MOCK_LON_1E7=354731666 -DPURRGO_MOCK_ALT_M=-430
```

---

## Documentation

| Документ | Содержание |
|---|---|
| [`docs/architecture.md`](docs/architecture.md) | Архитектура программного обеспечения |
| [`docs/hardware.md`](docs/hardware.md) | Аппаратная конфигурация |
| [`docs/eink.md`](docs/eink.md) | Драйвер E-Ink (epd4g) |
| [`docs/design/poi-search.md`](docs/design/poi-search.md), [`docs/design/pgo-v4.md`](docs/design/pgo-v4.md) | Проекты: поиск объектов, блочный индекс карты V4 |
| [`TODO.md`](TODO.md) | Текущие задачи |
| [`docs/map-format-v3.md`](docs/map-format-v3.md) | Формат карт V3 |
| [`docs/map-conformance.md`](docs/map-conformance.md) | Conformance V3 |
| [`tools/map-compiler/README.md`](tools/map-compiler/README.md) | Создание карт |
| [`tools/book-compiler/README.md`](tools/book-compiler/README.md), [`PGB-format.md`](tools/book-compiler/PGB-format.md) | Конвертер книг и формат PGB |
| [`docs/e-books.md`](docs/e-books.md) | Режим чтения книг |
| [`PurrGO_STM32U585_PINOUT.md`](PurrGO_STM32U585_PINOUT.md) | Распиновка платы |
| [`docs/user-guide.md`](docs/user-guide.md) | Назначение кнопок |
| [`docs/map-rendering.md`](docs/map-rendering.md) | Порядок и стиль слоёв карты |
| [`docs/SD_SPI_MODE.md`](docs/SD_SPI_MODE.md) | microSD в SPI-режиме |
| [`docs/track-field-test.md`](docs/track-field-test.md) | Полевой тест фильтров трека |
| [`docs/WEB_EMULATOR.md`](docs/WEB_EMULATOR.md) | Эмулятор в браузере |
| [`docs/font-encoding.md`](docs/font-encoding.md) | Кодировка PurrGO-256 |

---

## License

PurrGo распространяется на условиях лицензии, указанной в [`LICENSE`](LICENSE).