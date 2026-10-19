# STM32 application

This directory is the composition point for the STM32 firmware.

Planned targets:

- STM32F411CEU6 prototype ( or NUCLEO-F446RE )
- STM32U585CIU6 final hardware

Keep MCU startup code, HAL initialization, interrupt handlers and board wiring here or under `src/platform/stm32/`. Do not move those dependencies into `src/core/`.

## Build

The firmware is not a standalone CMake project. Build it from the repository root with the `stm32-debug` / `stm32-release` presets (see the root `README.md`, section *Building*). The output is `out/build/<preset>/PurrGO_STM32.elf`.

`PurrGO_STM32/CMakeLists.txt` is the user-owned part of the STM32CubeMX CMake project and is added by the root `CMakeLists.txt` when `PURRGO_PLATFORM=stm32`.
