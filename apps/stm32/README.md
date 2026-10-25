# STM32 application

This directory is the composition point for the STM32 firmware.

Targets (both are supported, each is a separate CubeMX project):

| Target | Directory | microSD | Pinout |
|---|---|---|---|
| STM32F411CEU6 (WeAct) | `PurrGO_STM32/` | SDIO, 4 bit | [`PurrGO_STM32F411_PINOUT.md`](../../PurrGO_STM32F411_PINOUT.md) |
| STM32U585CIU6 (WeAct), final hardware | `PurrGO_STM32U5/` (to be created in migration stage 2) | SPI2 (no SDMMC in the 48-pin package) | [`PurrGO_STM32U585_PINOUT.md`](../../PurrGO_STM32U585_PINOUT.md) |

The migration plan and accepted decisions are in [`TODO_migrate.md`](../../TODO_migrate.md). F411 support is kept after the migration.

Keep MCU startup code, HAL initialization, interrupt handlers, board wiring and the STM32 platform adapters here (`PurrGO_STM32/Core/Src/`). Do not move those dependencies into `src/core/`.

## Build

The firmware is not a standalone CMake project. Build it from the repository root with the `stm32-debug` / `stm32-release` presets (F411; the U585 presets `stm32u5-debug` / `stm32u5-release` will be added in migration stage 3) (see the root `README.md`, section *Building*). The output is `out/build/<preset>/PurrGO_STM32.elf`.

`PurrGO_STM32/CMakeLists.txt` is the user-owned part of the STM32CubeMX CMake project and is added by the root `CMakeLists.txt` when `PURRGO_PLATFORM=stm32`.
