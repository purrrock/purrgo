# PurrGo

**PurrGo** is an open-source GNSS navigator and track logger for resource-constrained embedded hardware.

The project is inspired by classic handheld GPS navigators such as Garmin outdoor devices, but follows a different architectural priority:

> **Useful offline navigation with minimal power consumption and no route calculation.**

PurrGo is being developed as a portable C application whose navigation algorithms can be tested on a PC and subsequently deployed to STM32 microcontrollers with minimal changes to the core code.

---

## Project status

PurrGo is currently under active development.

The current development workflow is:

```text
GNSS data
   │
   ▼
PC / emulator
   │
   ├── GNSS parsing
   ├── navigation state
   ├── geographic calculations
   ├── track logging
   └── map processing/rendering
   │
   ▼
STM32 firmware
```

The PC implementation is not a separate navigation engine. It is intended to execute the same portable `src/core/` code that will eventually run on the STM32 target.

---

# Features

The planned navigator provides:

- GNSS positioning;
- latitude and longitude;
- altitude;
- speed;
- UTC time;
- satellite/fix information;
- track recording;
- GPX support;
- waypoint management;
- offline maps;
- map rendering;
- map zoom and pan;
- current-position marker;
- track display;
- basic navigation to a waypoint;
- physical controls;
- operation without Internet access;
- operation without cellular connectivity.

PurrGo intentionally does **not** implement turn-by-turn route calculation.

The device is a **navigator/logger**, not a routing engine.

---

# Design goals

## Low power consumption

The final device is intended to operate from a single 18650 Li-ion cell.

Power consumption is therefore a first-class architectural requirement.

The firmware is designed to allow independent control of:

- GNSS activity;
- display updates;
- SD-card activity;
- MCU operating modes;
- peripheral power.

The project does not rely on theoretical power figures for the final design. Important power-consumption characteristics will be measured on real hardware.

## Offline operation

PurrGo does not require:

- Internet access;
- mobile network;
- Wi-Fi;
- cloud services.

Maps and recorded tracks are stored locally.

## Portable core

Navigation algorithms must remain independent from the hardware platform.

The core library must not depend on:

- STM32 HAL;
- CMSIS;
- Win32;
- GUI frameworks;
- operating-system APIs.

This allows deterministic PC testing before deployment to the MCU. 

---

# Software architecture

The repository is divided into three main layers:

```text
+------------------------------------------------------+
| apps/pc/                 apps/stm32/                 |
| target-specific composition and entry points         |
+--------------------------+---------------------------+
| src/platform/pc/         | src/platform/stm32/      |
| PC adapters              | STM32 adapters           |
+------------------------------------------------------+
|                    src/core/                         |
| GNSS | navigation | geo | track | map | math        |
|             hardware-independent C                   |
+------------------------------------------------------+
```

## `src/core/`

Portable application logic.

The core is responsible for algorithms and data structures and must not directly access hardware.

Typical responsibilities include:

- GNSS parsing;
- navigation state;
- geographic calculations;
- track processing;
- map-related algorithms;
- mathematical operations.

The core uses standard C and fixed-width integer types where the data width is part of the interface contract. Dynamic allocation is avoided unless a concrete requirement makes it necessary. 

## `src/platform/`

Hardware and operating-system adapters.

Current platform separation includes:

```text
src/platform/pc/
src/platform/stm32/
src/platform/ublox/
```

The platform layer provides the interfaces required by the portable core, including I/O, time and hardware-specific services.

Board-specific pin assignments and peripheral initialization belong outside `src/core/`.

## `apps/`

Application entry points and target-specific composition.

Current PC applications include:

- `purrgo_pc`;
- `pc_realtime_logger`;
- PC emulator.

The STM32 application layer is intended to become the firmware composition point for the embedded target.

---

# GNSS subsystem

PurrGo initially uses standard NMEA data and the [`minmea`](https://github.com/kosma/minmea) library for NMEA sentence parsing.

The GNSS interface is intentionally separated from the navigation core:

```text
GNSS UART / file
       │
       ▼
transport layer
       │
       ▼
NMEA / minmea
       │
       ▼
navigation state
       │
       ▼
track / map / UI
```

This allows development hardware and final GNSS hardware to be changed without coupling the navigation algorithms to a particular receiver.

The repository also contains a separate u-blox platform layer.

The current development hardware uses a GY-NEO6MV2-class receiver.

The release hardware is intended to use a modern u-blox M10-class receiver, subject to final hardware validation.

---

# Fixed-point arithmetic

The project targets microcontrollers where floating-point performance and energy consumption are important considerations.

Where practical, navigation calculations are implemented using integer arithmetic with explicit scaling rather than relying on floating-point calculations.

This allows the same algorithms to be:

- deterministic;
- portable;
- tested on a PC;
- executed efficiently on the target MCU.

The exact scaling and units are part of the corresponding C interfaces and data structures.

---

# Map subsystem

PurrGo does not generate maps on the embedded device.

Maps are prepared on a PC and stored in a compact format suitable for the navigator.

The intended workflow is:

```text
OpenStreetMap / source data
             │
             ▼
       PC preprocessing
             │
             ▼
      PurrGo map format
             │
             ▼
           microSD
             │
             ▼
       STM32 map renderer
             │
             ▼
          display
```

The STM32 therefore acts primarily as a **map renderer and navigator**, rather than as a general-purpose GIS processor.

This reduces:

- RAM requirements;
- CPU requirements;
- storage overhead;
- software complexity;
- power consumption.

---

# Display architecture

The display is treated as a separately managed subsystem.

The navigation update rate and display update rate are deliberately independent.

For example:

```text
GNSS                  1 Hz
Track recording       1 Hz
Navigation state      1 Hz
Display                variable
```

The display does not need to be redrawn after every GNSS update.

This is particularly important for low-power reflective displays and E-Ink.

Current release-display candidates are:

- Memory LCD;
- black/white/red E-Ink.

The final display will be selected after testing actual hardware, especially:

- readability;
- refresh behaviour;
- map usability;
- energy consumption.

---

# Storage

The intended storage medium is microSD.

The card will contain data such as:

```text
/maps
/tracks
/waypoints
/config
```

The storage subsystem is separated from the application logic so that the physical storage interface can be changed without rewriting the navigation algorithms.

Track data should be buffered where practical to reduce unnecessary SD-card activity and power consumption.

---

# Hardware targets

PurrGo has separate development and release hardware targets.

## Development hardware

The initial embedded development platform is:

**NUCLEO-F446RE**

with:

- STM32F446RE;
- integrated ST-LINK;
- USB connection;
- GY-NEO6MV2 GNSS receiver;
- development display;
- microSD;
- physical buttons.

The NUCLEO board is a development platform and is not intended to define the final PCB architecture.

## Release hardware

The release hardware is currently planned around:

- STM32U5 family MCU;
- modern u-blox M10-class GNSS receiver;
- dedicated GNSS antenna;
- microSD;
- low-power display;
- physical controls;
- low-quiescent-current power architecture;
- one 18650 Li-ion cell.

The exact MCU, GNSS module, display and power-management components remain subject to hardware validation.

For the current hardware architecture and roadmap, see [`HARDWARE.md`](HARDWARE.md).

---

# Repository structure

```text
purrgo/
├── apps/
│   ├── pc/                 # PC applications
│   ├── stm32/              # STM32 application layer
│   └── emulator/           # PC hardware/emulator application
│
├── cmake/                  # CMake support files
│
├── docs/
│   └── architecture.md    # Software architecture and portability rules
│
├── include/                # Public C headers
│
├── src/
│   ├── core/               # Hardware-independent navigation code
│   └── platform/
│       ├── pc/             # PC platform adapters
│       ├── stm32/          # STM32 platform adapters
│       └── ublox/          # u-blox-specific code
│
├── tests/
│   └── core/               # Deterministic core tests
│
├── third_party/            # External dependencies
│
├── tools/                  # Development and preprocessing tools
│
├── CMakeLists.txt
├── HARDWARE.md
└── README.md
```

The repository currently builds the portable core, PC platform, PC applications, tests and, when enabled, the emulator through CMake. 

---

# Building on Windows

## Requirements

The current PC development environment requires:

- CMake 3.20 or newer;
- MinGW-w64 GCC or MSVC;
- Visual Studio Code;
- Microsoft C/C++ extension;
- Microsoft CMake Tools extension.

The repository uses CMake and C11. 

A detailed Windows build guide is available in:

[`Руководство по компиляции.md`](Руководство%20по%20компиляции.md)

---

## Command-line build

From the repository root:

```bash
cmake -S . -B build
cmake --build build
```

On Windows, the default CMake configuration enables the PC emulator.

To explicitly control the emulator:

```bash
cmake -S . -B build -DBUILD_EMULATOR=ON
cmake --build build
```

or:

```bash
cmake -S . -B build -DBUILD_EMULATOR=OFF
cmake --build build
```

`BUILD_EMULATOR` defaults to enabled on Windows and disabled for non-Windows/cross-compilation configurations. 

---

# Tests

The PC build includes deterministic tests for the portable core.

Current tests include:

```text
test_geo
test_gnss
```

Run them with:

```bash
ctest --test-dir build --output-on-failure
```

The purpose of these tests is to verify navigation algorithms independently from STM32 hardware.

---

# PC real-time GNSS logger

The repository contains a PC application for reading GNSS data from a serial port.

After building, it can be run as:

```cmd
.\build\apps\pc_realtime_logger\pc_realtime_logger.exe COM3
```

Replace `COM3` with the actual GNSS receiver COM port.

The application is useful for testing the GNSS pipeline with real receiver data before deploying the same core algorithms to the STM32 target.

---

# Development workflow

The preferred development cycle is:

```text
1. Implement algorithm
        │
        ▼
2. Test with deterministic PC input
        │
        ▼
3. Test with recorded GNSS data
        │
        ▼
4. Test with live GNSS receiver
        │
        ▼
5. Integrate with STM32 platform layer
        │
        ▼
6. Measure on real hardware
```

Hardware-specific implementation should be introduced only after the required hardware characteristics are known.

---

# Power-management strategy

Low power consumption is not treated as a final optimization step.

The firmware architecture is intended to support explicit power states such as:

- active;
- navigation;
- logging;
- idle;
- deep sleep.

Potentially power-hungry peripherals such as the SD card and display should be independently controlled where the hardware permits it.

The final power budget will be based on measurements of:

- MCU;
- GNSS receiver;
- display;
- SD card;
- regulators;
- peripheral leakage;
- complete system.

---

# What PurrGo is not

PurrGo is intentionally not:

- an online navigation service;
- a smartphone application;
- a cloud-connected tracker;
- a turn-by-turn routing engine;
- a full GIS workstation.

The project focuses on a small, autonomous embedded navigation device.

---

# Documentation

Important project documents:

- [`HARDWARE.md`](HARDWARE.md) — hardware architecture, development hardware, release hardware and power strategy.
- [`docs/architecture.md`](docs/architecture.md) — software architecture and portability rules.
- [`Руководство по компиляции.md`](Руководство%20по%20компиляции.md) — Windows/VS Code build instructions.

---

# License

PurrGo is distributed under the license included in [`LICENSE`](LICENSE).