# STM32 application

This directory is the composition point for the STM32 firmware.

Planned targets:

-  NUCLEO-F446RE **or** STM32F411CEU6 prototype
- STM32U585CIU6 final hardware

Keep MCU startup code, HAL initialization, interrupt handlers and board wiring here or under `src/platform/stm32/`. Do not move those dependencies into `src/core/`.
