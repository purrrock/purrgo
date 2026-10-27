# STM32 application

This directory is the composition point for the STM32 firmware.

Target: **STM32U585CIU6** (WeAct board), CubeMX project `PurrGO_STM32U5/`. The microSD card is on SPI2 (the 48-pin package has no SDMMC). Pinout: [`PurrGO_STM32U585_PINOUT.md`](../../PurrGO_STM32U585_PINOUT.md).

Support for STM32F411CEU6 has been removed; its code is kept in the archive release on GitHub. The migration history is in [`TODO_migrate.md`](../../TODO_migrate.md).

Keep MCU startup code, HAL initialization, interrupt handlers and board wiring in the CubeMX project (`PurrGO_STM32U5/Core/Src/`). STM32 platform adapters live in `common/` (`common/Src`, `common/Inc`); what is tied to the MCU peripherals stays in the project: `sd_card.c` (SPI2 binding), `gnss_io.c` (GPDMA1), `power_stm32.c` (Standby). `common/sd_spi` is the HAL-free SD-card SPI protocol (host-tested), `common/fatfs` is FatFs R0.16 (CubeMX has no FATFS middleware for STM32U5). Do not move any of those dependencies into `src/core/`.

## Build

The firmware is not a standalone CMake project. Build it from the repository root with the `stm32u5-debug` / `stm32u5-release` presets; see the root `README.md`, section *Building*. The output is `out/build/<preset>/PurrGO_STM32U5.elf`.

`PurrGO_STM32U5/CMakeLists.txt` is the user-owned part of the STM32CubeMX CMake project and is added by the root `CMakeLists.txt` when `PURRGO_PLATFORM=stm32u5`.
