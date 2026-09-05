# STM32 application

This directory is the composition point for the STM32 firmware.

Planned targets:

- STM32F411CEU6 prototype ( or NUCLEO-F446RE )
- STM32U585CIU6 final hardware

Keep MCU startup code, HAL initialization, interrupt handlers and board wiring here or under `src/platform/stm32/`. Do not move those dependencies into `src/core/`.
