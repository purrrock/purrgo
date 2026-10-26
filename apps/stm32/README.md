# STM32 application

This directory is the composition point for the STM32 firmware.

Targets (both are supported, each is a separate CubeMX project):

| Target | Directory | microSD | Pinout |
|---|---|---|---|
| STM32F411CEU6 (WeAct) | `PurrGO_STM32/` | SDIO, 4 bit | [`PurrGO_STM32F411_PINOUT.md`](../../PurrGO_STM32F411_PINOUT.md) |
| STM32U585CIU6 (WeAct), final hardware | `PurrGO_STM32U5/` | SPI2 (no SDMMC in the 48-pin package) | [`PurrGO_STM32U585_PINOUT.md`](../../PurrGO_STM32U585_PINOUT.md) |

The migration plan and accepted decisions are in [`TODO_migrate.md`](../../TODO_migrate.md). F411 support is kept after the migration.

Keep MCU startup code, HAL initialization, interrupt handlers and board wiring in each CubeMX project (`PurrGO_STM32/Core/Src/`, `PurrGO_STM32U5/Core/Src/`). STM32 platform adapters that do not depend on the MCU family live in `common/` (`common/Src`, `common/Inc`) and are built into both firmwares; what differs per board stays in the project: `sd_card.c` (SDIO vs SPI), `gnss_io.c`, `power_stm32.c`. `common/sd_spi` is the HAL-free SD-card SPI protocol (host-tested), `common/fatfs` is FatFs R0.12c for U585 (CubeMX has no FATFS middleware for STM32U5). Do not move any of those dependencies into `src/core/`.

## Build

The firmware is not a standalone CMake project. Build it from the repository root with the `stm32-debug` / `stm32-release` presets (F411) or the `stm32u5-debug` / `stm32u5-release` presets (U585); see the root `README.md`, section *Building*. The output is `out/build/<preset>/PurrGO_STM32.elf` (F411) or `PurrGO_STM32U5.elf` (U585).

`PurrGO_STM32/CMakeLists.txt` is the user-owned part of the STM32CubeMX CMake project and is added by the root `CMakeLists.txt` when `PURRGO_PLATFORM=stm32`; `PurrGO_STM32U5/CMakeLists.txt` likewise for `PURRGO_PLATFORM=stm32u5`.
