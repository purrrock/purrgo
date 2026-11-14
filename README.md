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

- **Карта**: предкомпилированные векторные карты,
  4 уровня серого, автоследование за GNSS и ручной сдвиг, включение слоёв.
- **Trip computer**: время (UTC и местное), скорость, курс, координаты, высота, качество фикса,
  цель навигации (расстояние и пеленг).
- **Запись трека** в GPX на microSD с фильтрацией точек (режимы OFF / STANDARD / EXPEDITION) и режим
  **трекера с отключённым дисплеем** (пункт меню TRACKER).
- **Путевые точки и маршруты**: цель навигации (WAYPOINT: HERE, MAP CENTER, точка из GPX), слой Waypoints,
  слой Route из GPX-файла.
- **Читалка книг** (BOOKS): формат PGB, поиск по книге, GNSS на время чтения выключен.
- **Питание**: выбор типа батарей (шесть источников), остановка записи на SD при низком
  напряжении, выключение (POWER OFF) с показом на выключенном экране последней карты.

Подробности - в [`docs/user-guide.md`](docs/user-guide.md).

---

## Статус проекта

Проект находится в активной разработке.
Текущий этап — реализация firmware для **STM32U585CIU6**.

Текущий список работ находится в [`TODO.md`](TODO.md).

---

## Архитектура


Подробное описание архитектуры находится в [`docs/architecture.md`](docs/architecture.md).

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
├── emulator/               # PC-эмулятор (SDL2, WebAssembly)
├── stm32/                  # Прошивка STM32U585: CubeMX-проект (.ioc, Core/, Drivers/)
│   └── common/             # STM32-адаптеры: дисплей, кнопки, ФС, логгер, SD по SPI, FatFs
│                           # (GNSS UART+DMA, SD, питание - в Core/Src)
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

## Сборка и тесты

Кратко (подробности, пресеты, флаги, CI, эмуляторы и аппаратные тесты - в [`docs/development.md`](docs/development.md)):

```bash
cmake --preset stm32u5-release && cmake --build --preset stm32u5-release                      # прошивка STM32U585
```

## Documentation

| Документ | Назначение |
|---|---|
| [`docs/user-guide.md`](docs/user-guide.md) | Руководство пользователя: кнопки, меню, режимы, читалка, файлы на microSD |
| [`docs/hardware.md`](docs/hardware.md) | Аппаратная часть: платформа, GNSS, дисплей, microSD (SPI), питание, правила ведения распиновки |
| [`PurrGO_STM32U585_PINOUT.md`](PurrGO_STM32U585_PINOUT.md) | Краткая распиновка: что куда запаять |
| [`docs/architecture.md`](docs/architecture.md) | Архитектура программного обеспечения |
| [`docs/development.md`](docs/development.md) | Сборка, тесты, CI, аппаратные тесты, эмуляторы, полевой тест трека |
| [`docs/eink.md`](docs/eink.md) | Драйвер E-Ink (epd4g) |
| [`docs/map-format-v3.md`](docs/map-format-v3.md), [`docs/map-conformance.md`](docs/map-conformance.md), [`docs/map-rendering.md`](docs/map-rendering.md) | Формат карт V3, проверка соответствия, порядок и стиль слоёв |
| [`tools/map-compiler/README.md`](tools/map-compiler/README.md) | Создание карт |
| [`tools/book-compiler/README.md`](tools/book-compiler/README.md), [`PGB-format.md`](tools/book-compiler/PGB-format.md) | Конвертер книг и формат PGB |
| [`docs/font-encoding.md`](docs/font-encoding.md) | Кодировка PurrGO-256 |
| [`docs/design/poi-search.md`](docs/design/poi-search.md), [`docs/design/pgo-v4.md`](docs/design/pgo-v4.md) | Проекты: поиск объектов, блочный индекс карты V4 |
| [`docs/measurements/`](docs/measurements/README.md) | Результаты замеров и логи |
| [`docs/datasheets/`](docs/datasheets/) | Даташиты, схемы, фотографии |
| [`TODO.md`](TODO.md) | Открытые задачи и проверки на железе |

## License

PurrGo распространяется на условиях лицензии, указанной в [`LICENSE`](LICENSE).