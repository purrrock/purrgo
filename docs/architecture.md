# PurrGO Software Architecture

## 1. Purpose

PurrGO is an autonomous offline GNSS navigator and track logger for resource-constrained embedded hardware.

The device provides:

* GNSS positioning;
* track recording;
* offline vector-map display;
* waypoint navigation;
* physical-button control;
* low-power operation.

PurrGO does **not** perform turn-by-turn route calculation. It is a navigator/logger, not a routing engine.

The software is developed and validated on a PC before deployment to STM32.

---

# 2. Architectural Principles

The software is divided into three main layers:

```text
+-----------------------------+
|         Application         |
|   PC / STM32 / Emulator     |
+--------------+--------------+
               |
               v
+-----------------------------+
|       Platform adapters     |
| PC / STM32                  |
+--------------+--------------+
               |
               v
+-----------------------------+
|        Portable core        |
| GNSS / navigation / map /   |
| track / graphics / geo      |
+-----------------------------+
```

The main rules are:

1. Application logic belongs in the portable core whenever possible.
2. Hardware and operating-system dependencies belong in platform adapters.
3. Application entry points compose the core with a platform.
4. The same core code is used on PC and STM32.
5. The core must not depend on STM32 HAL, CMSIS, Win32 or a concrete filesystem, UART, SPI or display driver.
6. External resources are accessed through interfaces supplied by the application/platform layer.
7. Memory usage should be bounded and predictable.
8. Map data is precompiled on the PC; the STM32 does not perform general-purpose GIS processing.
9. Production map/navigation code does not require floating-point arithmetic.
10. GNSS, storage, display and navigation processing have independent update rates.

---

# 3. Repository Structure

The main architectural boundaries are:

```text
purrgo/
├── apps/
│   ├── emulator/           # PC emulator (SDL2)
│   └── stm32/              # STM32U585 firmware
│       ├── common/         # Platform adapters (Src, Inc, sd_spi, fatfs)
│       └── PurrGO_STM32U5/ # CubeMX project (microSD over SPI2)
│           └── Core/Src/   # MCU-specific glue (sd_card.c, gnss_io.c, power_stm32.c)
│
├── include/
│   └── purrgo/             # Public interfaces
│
├── src/
│   ├── core/               # Portable application logic
│   └── platform/
│       └── pc/             # PC adapters
│
├── tests/
│   ├── host/               # Host unit tests (fake filesystem, sanitizers)
│   └── audit_maps.py       # Binary map auditor
│
├── tools/                  # Build-time and development tools
├── third_party/            # External dependencies (submodules)
├── cmake/                  # Toolchain files, warning flags
└── docs/                   # Project documentation
```

`include/purrgo/` contains public interfaces. Their implementations are located in `src/`.

---

# 4. Portable Core

`src/core/` contains hardware-independent application logic.

Its responsibilities include:

* GNSS data processing;
* navigation state;
* geographic calculations;
* coordinate transformations;
* track processing;
* waypoint calculations;
* map access and rendering logic;
* graphics primitives;
* supporting mathematics.

The core is the primary target for deterministic PC testing.

## Core restrictions

Core code must not directly access:

* STM32 HAL or CMSIS;
* Win32 or other OS-specific APIs;
* GPIO;
* UART;
* SPI;
* DMA;
* timers;
* display controllers;
* SD-card drivers;
* concrete filesystem implementations.

The core should use fixed-width integer types where representation matters and make units, scaling and byte order explicit.

Dynamic allocation should be avoided or strictly controlled.

---

# 5. Platform Layer

`src/platform/` adapts external hardware and operating-system services to interfaces used by the core.

## PC

`src/platform/pc/` provides services required by the emulator and host tests, such as:

* serial I/O;
* filesystem access;
* GNSS input (serial port or mock generator);
* display and logger adapters.

The PC environment is a development and validation platform, not a separate navigation implementation.

## STM32

The STM32 platform adapters implement the interfaces declared in
`include/purrgo/`. Code that does not depend on the CubeMX-generated peripheral
setup lives in `apps/stm32/common/`; code tied to the MCU peripherals lives next
to the CubeMX project (`apps/stm32/PurrGO_STM32U5/Core/Src/`):

* UART + DMA GNSS input (`gnss_io.c`: LPUART1_RX with a GPDMA1 linked-list circular
  channel; the ring-buffer logic is in the portable header
  `include/purrgo/gnss_ring.h` and is covered by host tests);
* microSD + FatFs (`fs_hal_stm32.c` in `common`, `sd_card.c` in the project):
  the card is on SPI2 (the 48-pin package has no SDMMC) with the HAL-free card
  protocol in `common/sd_spi` (host-tested) and FatFs R0.16 in `common/fatfs`
  (see `docs/SD_SPI_MODE.md`). `fs_hal_stm32.c` keeps a 64-sector LRU cache,
  builds a FatFs cluster link map (CLMT, fast seek) for read-only files of 256 KB and
  more (map files), and supports an optional per-file read-ahead
  (`purrgo_fs_set_read_ahead`, a single multi-sector SD request per cache miss; the map
  enables it for `.mlp` only);
* display drivers (`display_eink.c`, `display_stm32.c`);
* buttons (`buttons.c`, `debug_buttons.c`);
* system time and logger (`system_time.c`, `purrgo_logger.c`);
* power control (`power_stm32.c`).

Board-specific peripheral configuration and pin assignments belong outside the portable core.
Code generated by STM32CubeMX (`main.c`, `gpio.c`, `spi.c`, ...) and vendor libraries
(`Drivers/`, `Middlewares/`) are not edited by hand.

## GNSS receiver specifics

Receiver-specific commands (u-blox M10 UBX-CFG-VALSET for the release G10A F30) live in the
portable `src/core/gnss_receiver.c` (`purrgo/gnss_receiver.h`): it only builds the frames, and the
platform passes a byte-send callback (`gnss_io.c` on STM32). The receiver model is selected by
`PURRGO_HW_GNSS_RECEIVER` in `hardware_config.h`. Generic GNSS processing remains in the portable
core (`src/core/gnss.c`, `gnss_adapter.c`).

---

# 6. Application Layer

## Emulator

`apps/emulator/` is the PC application: it runs the portable core with an SDL2 window instead of
the e-ink display and takes GNSS data from a mock generator or from a real receiver on a serial
port. A typical data flow is:

```text
GNSS (mock / serial port)
  |
  v
PC transport
  |
  v
GNSS parser
  |
  v
portable core
  |
  +--> navigation
  +--> track
  +--> map
  +--> UI/output
```

## STM32

`apps/stm32/` is the composition point for the final firmware.

It connects:

```text
STM32 platform
      |
      +--> GNSS
      +--> storage
      +--> display
      +--> buttons
      +--> power
      |
      v
portable application core
```

Navigation algorithms should not be duplicated in the STM32 application.

---

# 7. GNSS and Navigation

GNSS processing is separated into three stages:

```text
GNSS receiver
      |
      v
transport
      |
      v
NMEA / UBX parsing
      |
      v
normalized GNSS data
      |
      v
navigation state
```

Receiver-specific communication belongs to the platform layer.

The navigation subsystem consumes normalized GNSS data and provides state used by:

```text
             navigation state
              /      |       \
             v       v        v
          track    waypoint   map
                    navigation
```

Waypoint navigation provides position-to-waypoint information such as distance and bearing.

PurrGO does not calculate routes.

---

# 8. Track Recording

Track recording consumes normalized navigation data:

```text
GNSS
 |
 v
navigation state
 |
 v
track processing
 |
 v
buffer
 |
 v
GPX
 |
 v
storage
```

Storage access is performed through a platform abstraction.

Buffering and batched writes are used to reduce unnecessary storage activity and support low-power operation.

---

# 9. Map Subsystem

PurrGO uses **precompiled vector maps**.

The map workflow is:

```text
OSM/source data
      |
      v
PC map compiler
      |
      v
PurrGO map package
      |
      v
microSD
      |
      v
STM32 map subsystem
      |
      v
graphics
      |
      v
display
```

The STM32 does not compile OSM data or perform general-purpose GIS processing.

The map subsystem performs:

1. spatial selection;
2. binary map access;
3. visibility/culling;
4. coordinate projection;
5. rendering.

The binary map format is defined exclusively in:

```text
docs/purrgo_map_specification_v3.md
```

Map compiler usage is documented in:

```text
tools/map-compiler/README.md
```

Binary-format conformance requirements are documented in:

```text
docs/PurrGO Map Format V3 — Binary Format Conformance.md
```

---

# 10. Graphics and Display

The map renderer is independent of the physical display controller.

```text
map renderer
     |
     v
graphics primitives
     |
     v
framebuffer / graphics context
     |
     v
display adapter
     |
     v
physical display
```

This allows the same rendering logic to be used by the PC environment and STM32 firmware.

Display updates are independent of GNSS updates. A new GNSS fix does not necessarily require a complete display refresh.

The release display is:

```text
Waveshare 2.7inch e-Paper HAT
176 × 264 pixels
4 grayscale levels
SPI
portrait orientation
```

Hardware details are documented in `HARDWARE.md`.

## E-Ink driver (epd4g)

Панель управляется библиотекой [`epd4g`](https://github.com/purrrock/eink_part_refresh)
(сабмодуль `third_party/eink_part_refresh`, используется только каталог `epd4g/`): 4-gray partial,
fast full, 1-bit режим, неблокирующий API. В `apps/stm32/PurrGO_STM32U5/CMakeLists.txt` подключены
`add_subdirectory(.../epd4g)` и `epd4g` в `target_link_libraries`. Это INTERFACE-библиотека:
исходники компилируются внутри таргета прошивки и видят `main.h`, HAL, `hspi1` и метки
`EINK_CS/DC/RST/BUSY` из CubeMX (порт `epd4g_port_stm32_hal.c`). Определение `EPD4G_BW_THRESHOLD=3`
сохраняет семантику 1-bit режима: белый → белый, любой серый → чёрный (по умолчанию порог 2, светло-серый
стал бы белым). Библиотека не входит в `purrgo_sources_warnings`; она проверена с
`-Wall -Wextra -Wconversion -Wshadow`.

| Файл | Роль |
|---|---|
| `include/purrgo/display_hal.h` | `display_set_mode(DISPLAY_MODE_GRAY4 / DISPLAY_MODE_MONO)`; `display_refresh()` и `display_flush()` неблокирующие, `display_sleep()` блокирующая |
| `src/core/app_ui.c` | режим выбирается по экрану: карта и экран выключения — 4 gray, остальное — 1 bit |
| `common/Src/display_eink.c` | очередь обновлений поверх epd4g |
| `common/Src/display_stm32.c` | framebuffer, накопление области, `display_stm32_poll()`, `display_stm32_ready_for_render()` |
| `common/Src/stm32_app.c` | `display_stm32_poll()` на каждом проходе цикла; рендер только при `display_stm32_ready_for_render()` |
| `common/Src/eink_test.c` | самотесты на железе (`PURRGO_EINK_SELFTEST_4GRAY`, `PURRGO_EINK_SELFTEST_PARTIAL`) |
| `tests/host/test_display_eink.c` | host-тест очереди на модели панели (нужен сабмодуль) |

Framebuffer PurrGO (176×264, 2 бита на пиксель, первый пиксель в старших битах, 0 = чёрный … 3 = белый)
совпадает с буфером epd4g: конвертации и поворота нет. Меню в 1-bit режиме выводятся прямо из него
(`EPD4G_SRC_2BPP`), второй буфер не нужен.

Логика `display_eink.c`:

* Одно обновление в работе. `display_refresh()` / `display_flush()` только ставят запрос; пока идёт
  предыдущее, запросы сливаются (полное перекрывает частичное, области объединяются). Запуск из
  очереди — в `display_eink_poll()`. Простого вызова `epd4g_start_*` из `display_flush()` недостаточно:
  `epd4g_data_busy() == 0` не значит `epd4g_is_busy() == 0`, и `epd4g_start_*` вернёт `EPD4G_ERR_BUSY`.
* Рендер запрещён, пока `epd4g_data_busy()`: передача кадра по SPI (десятки мс), а для 1-bit partial
  ещё и до конца волны (~0,3 с: окно повторно пишется в RAM `0x24`, см. ниже). Волна 4 gray
  (секунды) рендеру не мешает.
* Первое обновление, смена режима и потеря картинки (ошибка) дают полное обновление. Смена режима
  панели невозможна без `epd4g_init()` / `epd4g_bw_init()` и полного обновления; это делается
  автоматически.
* Лимит частичных подряд контролирует сама библиотека: после `EPD4G_PARTIALS_BEFORE_FULL` (4 gray) /
  `EPD4G_BW_PARTIALS_BEFORE_FULL` (1 bit) частичных очередной запрос она выполняет как полное
  обновление того же кадра (`EPD4G_AUTO_FULL_MODE` = FAST; в 1 bit — `EPD4G_BW_NORMAL`, после deep sleep
  с предварительным `epd4g_bw_init()`). Слой PurrGO счётчика не ведёт: `display_eink_partial_count()`
  читает `epd4g_partials_since_full()`, лимиты задаются через `epd4g_set_partials_before_full()` в
  `display_eink_init()` и `display_eink_set_partial_limits()`. Для 4 gray `prev` копируется как и раньше:
  при полном обновлении на панели тот же кадр.
* 4 gray: перерисовываются только пиксели, отличающиеся от буфера `prev` (11616 байт, «что на
  панели»). Сравнивается весь кадр, а не накопленная область, и копируется весь буфер:
  `epd4g_fb_copy_rect()` копирует целые байтовые столбцы (по 4 пикселя), поэтому по произвольной
  области `prev` получил бы пиксели, которые на панель не выводились. `prev` обновляется сразу после
  старта задания.
  Если отличий нет (`changed == 0`), библиотека ставит пустое задание `epd4g_start_noop()`; оно
  завершается в следующем `epd4g_poll()`, и слой PurrGO обязан его дождаться: иначе библиотека остаётся
  занятой и следующий `epd4g_start_*()` вернёт `EPD4G_ERR_BUSY`.
* 1 bit: частичное обновление окна накопленной области.
* Питание панели снимает библиотека: `epd4g_poll()` через `EPD4G_AUTO_POWEROFF_MS` (500 мс) простоя сам
  запускает молчаливое задание power off (~0,3 с, `epd4g_is_busy() == 1`, `epd4g_result()` и `done_cb` не
  затрагиваются). Поэтому `display_eink_poll()` вызывает `epd4g_poll()` на каждом проходе, даже когда
  своей операции нет, и не запускает новое задание, пока библиотека занята.
* Простой 1 с → `epd4g_start_sleep()` (только команда deep sleep: питание уже снято). Следующее обновление будит
  панель: в 4 gray через `epd4g_init()` (картинка на панели сохраняется, обновление остаётся
  частичным), в 1 bit аппаратным reset внутри частичного обновления. Повторный
  `epd4g_start_sleep()` в 4 gray возвращает `ERR_NOT_INIT` (в 1 bit — `OK`), слой PurrGO это учитывает.
* Ошибка (например, таймаут BUSY): панель считается потерянной, запрос ставится заново как полное
  обновление, повтор через 2 с.
* `display_sleep()` (выключение питания): ждёт все поставленные обновления, в том числе экран
  выключения, затем `epd4g_wait()` (доводит до конца автоматическое выключение питания) и `epd4g_sleep()`; ограничение 30 с. Последовательность в ядре:
  `display_refresh()` → `display_sleep()` (`src/core/power.c`), `power_stm32.c` менять не нужно.
  Экран выключения — карта, рисуется без смены экрана, поэтому `purrgo_app_ui_render_power_off()`
  сам вызывает `display_set_mode(DISPLAY_MODE_GRAY4)`.
* Маршрутный компьютер после отрисовки значений регистрирует область всего экрана
  (`display_refresh_region`), иначе на панель уходит только строка подсказок кнопок.

SPI панели: делитель `/8` (`PURRGO_EINK_SPI_PRESCALER`), 10 МГц при ядре SPI1 80 МГц. Делитель 2
даёт ~40 МГц, при такой скорости панель искажает данные.

Остаточные строки в 1-bit partial. Режим 2 контроллера рисует пиксели, у которых «новая» (`0x24`) и
«старая» (`0x26`) RAM различаются, а после каждого обновления плоскости меняются ролями. Штатный
`Display_Partial` Waveshare пишет только окно и только в `0x24`, поэтому вне окна плоскости
перестают совпадать: в меню остаются инвертированные строки, на карте копии маркёра. В `epd4g`
это исправлено (`EPD4G_BW_RESYNC=1`): после волны то же окно повторно пишется в `0x24`, и обе
плоскости снова содержат картинку на стекле. Побочный эффект: `epd4g_data_busy()` для 1-bit partial
остаётся 1 до конца этой записи, рендер откладывается на ~0,3 с. Для 4 gray проблемы нет (каждый
проход пишет обе плоскости целиком). Запись только в `0x26` не помогает.

Не реализовано (по необходимости):

* прерывание EXTI по BUSY вместо опроса (главный цикл просыпается по SysTick 1 кГц);
* SPI через DMA: не нужен, пока `epd4g_poll()` укладывается в миллисекунду (CS и D/C
  переключаются пачками, а не на каждый байт);
* градации серого в 1-bit режиме (серые заливки `landuse` дают чёрное), см. `TODO.md`.

---

# 11. Storage

Persistent storage is provided by microSD on the target device.

The logical application data consists of:

```text
maps
tracks
waypoints
configuration
```

Application logic accesses storage through an abstraction rather than directly through the SD-card or filesystem implementation.

This allows the same core logic to operate with a PC filesystem during development.

---

# 12. Power Management

Low power consumption is an architectural requirement.

The application must be able to manage major power consumers independently where supported by the hardware:

```text
                 +--> GNSS
                 |
                 +--> display activity
                 |
power policy ----+--> storage activity
                 |
                 +--> MCU operating mode
                 |
                 +--> peripherals
```

The system should spend as much time as practical in low-power states between meaningful events.

Display refreshes, storage operations and GNSS activity should therefore be controlled independently rather than tied to a single continuous processing loop.

Hardware-specific power control belongs to the STM32 platform adapters.

---

# 13. Data and Dependency Flow

The preferred dependency direction is:

```text
        applications
             |
             v
        platform
             |
             v
           core
```

The core may receive abstract interfaces for external resources:

```text
application
     |
     +---- creates/configures adapter
     |
     v
  interface
     |
     v
    core
```

The reverse dependency is prohibited:

```text
core
 |
 +--> STM32 HAL       forbidden
 +--> Win32           forbidden
 +--> concrete SD     forbidden
 +--> display driver  forbidden
 +--> concrete UART   forbidden
```

This separation is what allows the portable core to move from PC to STM32 without rewriting the application algorithms.

---

# 14. Resource Management

PurrGO targets resource-constrained STM32 hardware.

Preferred techniques are:

* fixed-width integer types;
* caller-owned buffers;
* bounded arrays;
* static buffers where practical;
* streaming parsing;
* spatial culling before geometry processing;
* batched storage operations;
* avoidance of unnecessary data copies;
* controlled dynamic allocation.

The production map/navigation path uses integer/fixed-point representations and does not depend on floating-point arithmetic.

PC-side tools are not subject to this runtime restriction.

---

# 15. Error Handling

External data must be treated as potentially malformed.

This includes:

* GNSS input;
* map files;
* GPX files;
* waypoint data;
* configuration;
* filesystem results.

Parsers must validate:

* sizes;
* offsets;
* counts;
* read results;
* numeric ranges;
* binary structure;
* byte order.

Malformed input must not cause out-of-bounds memory or file access.

---

# 16. Testing Strategy

Portable core functionality is tested on the PC using deterministic input
(`tests/host`, run with `ctest --preset linux-gcc-test` or `sh tests/host/run.sh`; the code
runs under AddressSanitizer and UndefinedBehaviorSanitizer, and CI runs the suite on every
push). The platform layer is replaced by a fake filesystem with fault injection.
Covered today: GNSS ring buffer and NMEA parsing on a recorded log, the map parser on valid
and corrupted files, `purrgo_time` and `track_logger` at their boundaries, SD write-failure
handling, atomic config saving and redraw policy.

Examples:

```text
recorded GNSS data
       |
       v
portable GNSS processing
       |
       v
expected navigation state
```

and:

```text
test map
    |
    v
portable map parser/renderer
    |
    v
expected result
```

Hardware testing remains necessary for:

* GNSS receiver behaviour;
* UART/SPI/DMA;
* microSD;
* display refresh;
* buttons;
* power consumption;
* low-power modes;
* wake-up behaviour.

The purpose of the PC environment is to detect algorithmic and data-processing errors before introducing hardware-specific variables.

---

# 17. Architectural Invariants

The following are architectural invariants of PurrGO:

1. PurrGO is an offline GNSS navigator/logger.
2. PurrGO does not implement turn-by-turn routing.
3. Portable application logic is kept independent of hardware.
4. PC and STM32 use the same portable core.
5. Hardware dependencies are isolated in platform adapters.
6. Maps are compiled on the PC and rendered on the device.
7. The STM32 map path does not perform general-purpose GIS processing.
8. Map data is spatially filtered before geometry is rendered.
9. Resource usage is bounded where practical.
10. Production map/navigation code does not depend on floating-point arithmetic.
11. Display and storage activity are independently controllable.
12. Binary map-format details are defined only by the V3 map specification.
