# PurrGo software architecture

## Goal

Develop navigation algorithms on a Windows PC and move the same algorithmic modules to STM32 with minimal or no source changes.

## Layering

```text
+-------------------------------------------------------+
| apps/pc/                apps/stm32/                  |
| target composition / main loop                       |
+---------------------------+---------------------------+
| src/platform/pc/          | src/platform/stm32/     |
| PC adapters               | STM32 adapters           |
+---------------------------+---------------------------+
|                 src/core/                         |
| GNSS parsing, navigation state, geo, track, math    |
| deterministic, hardware-independent C               |
+-------------------------------------------------------+
```

## `src/core/` rules

Code in `src/core/` must:

1. Be standard C and compile without STM32Cube/HAL/CMSIS headers.
2. Not access GPIO, UART, timers, DMA, Flash, RTC or display hardware directly.
3. Not depend on Windows APIs, threads, sockets or GUI libraries.
4. Avoid dynamic allocation unless a concrete requirement appears; prefer caller-owned/static buffers.
5. Use fixed-width integer types from `<stdint.h>` where width matters.
6. Make byte order and integer units explicit at interfaces.
7. Keep time, GNSS input, storage and display behind interfaces supplied by `platform/` or `apps/`.
8. Be testable from deterministic input on PC.

## Platform layer

`src/platform/pc/` supplies adapters used by desktop tools/tests.

`src/platform/stm32/` will contain STM32-facing adapters. Board-specific pin mapping and peripheral initialization should stay outside `src/core/` and should be introduced only after the exact MCU/board configuration is fixed.

## Application layer

`apps/pc/` is for desktop executables such as GNSS log replay and algorithm experiments.

`apps/stm32/` is the firmware composition point. NUCLEO-F446RE and Blue Pill are deployment targets, not separate algorithm implementations.

## Data flow

```text
GNSS bytes -> platform UART/file -> core GNSS parser
           -> navigation state -> track/logging logic
           -> application/UI/output adapters
```

The preferred development workflow is to feed recorded GNSS data to the PC application and run the same `src/core/` code that will later run on STM32.
