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

---

## Статус проекта

Проект находится в активной разработке.

Текущий переход:

```text
PC
 │
 ├── GNSS
 ├── navigation
 ├── track
 ├── maps
 └── renderer
 │
 ▼
STM32
 │
 ▼
Release hardware
```

Portable C core разрабатывается и тестируется на PC перед переносом на STM32.

Следующий основной этап — реализация и интеграция firmware для **STM32U585CIU6**.

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

Финальная конфигурация:

| Компонент | Release |
|---|---|
| MCU | STM32U585CIU6 |
| GNSS | G10A F30 |
| Display | Waveshare 2.7inch e-Paper HAT |
| Display resolution | 176 × 264 |
| Display | 4 gray levels, SPI |
| Storage | microSD |
| Battery | 1 × 18650 Li-ion |
| Controls | физические кнопки |

Для разработки STM32 используются **STM32F411CEU6**.

Для разработки GNSS используется **GY-NEO6MV2**.

Подробности аппаратной части находятся в [`HARDWARE.md`](HARDWARE.md).

---

## Maps

Карты подготавливаются на PC и затем используются устройством:

```text
OSM data
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

- [`docs/purrgo_map_specification_v3.md`](docs/purrgo_map_specification_v3.md) — нормативная спецификация Map Format V3;
- [`docs/PurrGO Map Format V3 — Binary Format Conformance.md`](docs/PurrGO%20Map%20Format%20V3%20%E2%80%94%20Binary%20Format%20Conformance.md) — требования соответствия;
- [`tools/map-compiler/README.md`](tools/map-compiler/README.md) — как создать карту для PurrGO.

---

## Repository structure

```text
purrgo/
├── apps/
│   ├── emulator/           # PC-эмулятор (SDL2)
│   └── stm32/              # Прошивка STM32 (CubeMX-проект + пользовательский код)
│       └── PurrGO_STM32/Core/Src/   # STM32-адаптеры: GNSS (UART+DMA), SD, дисплеи, кнопки, питание
│
├── docs/                   # Project documentation
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
├── third_party/            # External dependencies (сабмодули: minmea, e-Paper)
├── tools/                  # Development/build tools
│
├── cmake/
│   ├── toolchains/         # CMake toolchain files (STM32)
│   └── PurrgoWarnings.cmake# Флаги предупреждений проекта
│
├── .github/workflows/      # CI (GitHub Actions)
├── CMakeLists.txt          # Единая точка входа сборки
├── CMakePresets.json       # Пресеты: win-mingw-*, linux-gcc-*, stm32-*
├── HARDWARE.md
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
| `stm32-debug`, `stm32-release` | Прошивка STM32F411 | `arm-none-eabi-gcc` в `PATH`, Ninja |

Пресеты, неподходящие для текущей ОС, в списке не показываются.

### Visual Studio Code

Откройте корень репозитория (или `purrgo.code-workspace`), установите расширение
CMake Tools и выберите пресет в строке состояния. Для STM32 расширение
STM32Cube for VS Code предоставляет `cube-cmake` (см. `.vscode/settings.json`).
Отладка прошивки — конфигурация `Cortex Debug: OpenOCD (PurrGO)`: она берёт `.elf`
активного пресета.

### Командная строка

```sh
cmake --preset stm32-debug
cmake --build --preset stm32-debug
```

Результат сборки: `out/build/<пресет>/`. Прошивка — `out/build/stm32-*/PurrGO_STM32.elf`,
эмулятор — `out/build/<пресет>/apps/emulator/purrgo_emulator`.

Платформу выбирает переменная `PURRGO_PLATFORM` (`pc` | `stm32`), которую задаёт
пресет. Без пресета `cmake -S . -B build` собирает PC-эмулятор.

### STM32CubeMX

CubeMX-проект находится в `apps/stm32/PurrGO_STM32/` (`.ioc`, `Core/`, `Drivers/`, ...).
Перегенерация кода обновляет `cmake/stm32cubemx/CMakeLists.txt` и
`cmake/gcc-arm-none-eabi.cmake` в этой папке — они подхватываются автоматически.
`apps/stm32/PurrGO_STM32/CMakeLists.txt` (пользовательский, CubeMX его не перезаписывает)
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
| `test_map` | Парсер Map Format V3: корректная карта, повреждённые заголовки PGO/SQT/MLP, лимит глубины дерева, `v3_jump`, усечение и подмена байтов (фаззинг) |
| `test_time_logger` | `purrgo_time` и `track_logger` на границах: високосные годы, 2099, полночь и Новый год с часовым поясом, год < 26, прореживание и фильтры точек (качество фикса, стоянка, выбросы, повороты), периодический `sync`, имена файлов, RAM-кольцо трека |
| `test_negative_altitude` | Отрицательная высота (Мёртвое море, -430 м): разбор GGA, перерисовка Trip computer, вывод `-430 m`, `<ele>` в GPX, восход/закат |
| `test_refact_critical` | Исправления критичных ошибок (патч 0007): таймаут фикса, ошибки записи SD, атомарное сохранение конфига, политика перерисовки |

Условия, которые сейчас не выполняются из-за известных дефектов (см. `TODO_refact.md`),
помечены в тестах как `XFAIL`: это не провал. Когда дефект исправлен, тест печатает `XPASS` -
тогда `XFAIL` нужно заменить на `CHECK`.

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
| [`HARDWARE.md`](HARDWARE.md) | Аппаратная конфигурация |
| [`TODO.md`](TODO.md) | Текущие задачи |
| [`TODO_refact.md`](TODO_refact.md) | Результаты ревью кода и план исправлений |
| [`docs/purrgo_map_specification_v3.md`](docs/purrgo_map_specification_v3.md) | Формат карт V3 |
| [`docs/PurrGO Map Format V3 — Binary Format Conformance.md`](docs/PurrGO%20Map%20Format%20V3%20%E2%80%94%20Binary%20Format%20Conformance.md) | Conformance V3 |
| [`tools/map-compiler/README.md`](tools/map-compiler/README.md) | Создание карт |

---

## License

PurrGo распространяется на условиях лицензии, указанной в [`LICENSE`](LICENSE).