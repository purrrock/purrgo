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
| PC / STM32 / u-blox         |
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
│   ├── pc/                 # PC applications
│   ├── stm32/              # STM32 application
│   └── emulator/           # PC emulator
│
├── include/
│   └── purrgo/             # Public interfaces
│
├── src/
│   ├── core/               # Portable application logic
│   └── platform/
│       ├── pc/             # PC adapters
│       ├── stm32/          # STM32 adapters
│       └── ublox/          # u-blox-specific functionality
│
├── tests/
│   └── core/               # Core tests
│
├── tools/                  # Build-time and development tools
├── third_party/            # External dependencies
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

`src/platform/pc/` provides services required by desktop applications and tests, such as:

* serial I/O;
* filesystem access;
* GNSS input/replay;
* development utilities.

The PC environment is a development and validation platform, not a separate navigation implementation.

## STM32

`src/platform/stm32/` contains hardware-facing functionality, including:

* UART;
* SPI;
* DMA;
* timers;
* RTC/time sources;
* microSD;
* display interface;
* buttons;
* power control;
* MCU low-power operation.

Board-specific peripheral configuration and pin assignments belong outside the portable core.

## u-blox

`src/platform/ublox/` contains receiver-specific functionality.

Generic GNSS processing remains in the portable application layer, while u-blox-specific configuration and control remain isolated in this platform layer.

---

# 6. Application Layer

## PC

`apps/pc/` contains desktop applications used for development, testing and real-time GNSS work.

A typical data flow is:

```text
GNSS
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

## Emulator

`apps/emulator/` provides a PC environment for testing target-oriented behaviour without the physical device.

It should use the same portable core as the real application whenever practical.

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

Hardware-specific power control belongs to the STM32 platform layer.

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

Portable core functionality should be tested on the PC using deterministic input.

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
