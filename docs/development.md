# Development workflow

## PC first

1. Build the portable core with a native C compiler.
2. Run deterministic unit tests.
3. Replay recorded GNSS data through the PC adapter.
4. Validate navigation calculations and state transitions.
5. Reuse the same `src/core/` sources in the STM32 build.

## STM32 later

The firmware target should provide the platform services required by the core without changing core algorithms. STM32Cube/HAL code belongs in the platform/application layer.

## Board separation

Do not encode NUCLEO-F446RE or Blue Pill pin numbers into portable modules. Keep board configuration isolated so that moving between the prototype and final hardware does not require modifying navigation algorithms.
