# PurrGo

**PurrGo** is an open-source GNSS navigator and track logger for resource-constrained embedded hardware.

The project is inspired by classic handheld GPS navigators, with a different priority:

> **Useful offline navigation with minimal power consumption and no route calculation.**

PurrGo is being developed as portable C software. The same core algorithms are developed and tested on a PC and are intended to run on STM32 with minimal source changes.

---

## Project status

PurrGo is under active development.

Current development flow:

```text
GNSS data
   │
   ▼
PC / emulator
   │
   ├── GNSS
   ├── navigation
   ├── geo
   ├── track
   └── maps
   │
   ▼
STM32 firmware
```

The PC implementation is a development environment for the same portable core that will run on the embedded target.

---

## Features

Planned and currently developed functionality includes:

- GNSS positioning;
- latitude, longitude and altitude;
- speed, course and UTC time;
- fix and satellite information;
- track recording;
- GPX support;
- waypoint management;
- offline vector maps;
- map rendering, zoom and pan;
- current-position marker;
- track display;
- basic waypoint navigation;
- physical controls;
- completely offline operation.

PurrGo intentionally does **not** implement turn-by-turn route calculation.

It is a **navigator/logger**, not a routing engine.

---

## Design goals

### Low power

The final device is intended to operate from a single 18650 Li-ion cell.

Power management is treated as an architectural requirement. GNSS, display, storage and MCU power states are designed to be controlled independently where the hardware allows it.

Actual power consumption will be measured on hardware rather than estimated from theoretical component figures.

### Offline operation

PurrGo does not require:

- Internet;
- cellular connectivity;
- Wi-Fi;
- Bluetooth;
- cloud services.

Maps, tracks and waypoints are stored locally.

### Portable software

Navigation logic is kept independent from:

- STM32 HAL;
- CMSIS;
- Win32;
- GUI frameworks;
- operating-system APIs.

Detailed software architecture and portability rules are described in [`docs/architecture.md`](docs/architecture.md).

---

## Hardware

PurrGo currently uses three conceptual hardware profiles.

| Profile | Platform | GNSS | Display |
|---|---|---|---|
| Development | PC | Mock / USB GNSS | Emulator |
| Prototype | NUCLEO-F446RE | GY-NEO6MV2 / u-blox NEO-6M | 2.4" 240×320 ST7789 |
| Release | STM32U5-class | u-blox M10-class | 2.9" 128×296 E-Ink |

The release device is planned around:

- STM32U5-class MCU;
- modern u-blox M10-class receiver;
- microSD;
- low-power display;
- physical buttons;
- one 18650 Li-ion cell.

Hardware details and the current hardware roadmap are documented in [`HARDWARE.md`](HARDWARE.md).

---

## Map system

Maps are prepared on a PC and stored in a compact binary format for the navigator.

```text
source map data
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
STM32 renderer
      │
      ▼
display
```

The embedded device renders precompiled map data; it is not intended to perform general-purpose GIS processing.

The map format, parser and rendering architecture are documented in [`docs/architecture.md`](docs/architecture.md).

---

## Repository

```text
purrgo/
├── apps/
│   ├── pc/
│   ├── stm32/
│   └── emulator/
│
├── docs/
├── include/
├── src/
│   ├── core/
│   └── platform/
│       ├── pc/
│       ├── stm32/
│       └── ublox/
│
├── tests/
├── third_party/
├── tools/
│
├── CMakeLists.txt
├── HARDWARE.md
└── README.md
```

For the detailed source-level architecture, see [`docs/architecture.md`](docs/architecture.md).

---

## Building on Windows

The current PC development environment uses:

- CMake 3.20+;
- MinGW-w64 GCC or MSVC;
- Visual Studio Code;
- C/C++ extension;
- CMake Tools.

The project uses C11 and CMake.

Detailed build instructions are available in:

[`Руководство по компиляции.md`](Руководство%20по%20компиляции.md)

---

## PC real-time GNSS logger

The repository includes a PC application for reading GNSS data from a serial port.

Example:

```cmd
.\build\apps\pc_realtime_logger\pc_realtime_logger.exe COM3
```

Replace `COM3` with the actual receiver port.

This application is intended for testing the GNSS pipeline with real receiver data before deploying the same core algorithms to STM32.

---

## Documentation

- [`docs/architecture.md`](docs/architecture.md) — detailed software architecture, module boundaries, data flow, map subsystem and portability rules.
- [`HARDWARE.md`](HARDWARE.md) — hardware profiles, peripherals and power architecture.
- [`Руководство по компиляции.md`](Руководство%20по%20компиляции.md) — Windows / VS Code build instructions.

---

## License

PurrGo is distributed under the license included in [`LICENSE`](LICENSE).