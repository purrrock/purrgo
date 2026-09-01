# PurrGO

**PurrGO** — автономный ултра-энергоэффективный GNSS-навигатор и трекер.

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
|   PC / STM32 / u-blox     |
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

Для разработки STM32 используются **NUCLEO-F446RE** и **STM32F411CEU6** — та плата, которая будет доступна первой.

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
│   ├── pc/                 # PC applications
│   ├── stm32/              # STM32 application
│   └── emulator/           # PC emulator
│
├── docs/                   # Project documentation
├── include/
│   └── purrgo/             # Public interfaces
│
├── src/
│   ├── core/               # Portable core
│   └── platform/           # Platform-specific code
│
├── tests/                  # Tests
├── third_party/            # External dependencies
├── tools/                  # Development/build tools
│
├── CMakeLists.txt
├── HARDWARE.md
├── TODO.md
└── README.md
```

---

## Building

PC-версия использует:

- C11;
- CMake;
- MinGW-w64 GCC или MSVC;
- Visual Studio Code.

---

## PC GNSS

В репозитории имеется PC-приложение для работы с GNSS-приёмником через последовательный порт.

Пример запуска:

```cmd
.\build\apps\pc_realtime_logger\pc_realtime_logger.exe COM3
```

`COM3` необходимо заменить на порт GNSS-приёмника.

Приложение используется для проверки GNSS pipeline на реальном приёмнике до переноса соответствующего кода на STM32.

---

## Documentation

| Документ | Содержание |
|---|---|
| [`docs/architecture.md`](docs/architecture.md) | Архитектура программного обеспечения |
| [`HARDWARE.md`](HARDWARE.md) | Аппаратная конфигурация |
| [`TODO.md`](TODO.md) | Текущие задачи |
| [`docs/purrgo_map_specification_v3.md`](docs/purrgo_map_specification_v3.md) | Формат карт V3 |
| [`docs/PurrGO Map Format V3 — Binary Format Conformance.md`](docs/PurrGO%20Map%20Format%20V3%20%E2%80%94%20Binary%20Format%20Conformance.md) | Conformance V3 |
| [`tools/map-compiler/README.md`](tools/map-compiler/README.md) | Создание карт |

---

## License

PurrGo распространяется на условиях лицензии, указанной в [`LICENSE`](LICENSE).