# PurrGo

PurrGo is an open-source GNSS navigator/logger inspired by classic Garmin handheld devices.

## Development targets

- **PC** — algorithm development, GNSS log replay, deterministic tests.
- **NUCLEO-F446RE** — first embedded prototype.
- **Blue Pill (STM32F103C8T6)** — planned final hardware platform.

## Architecture

The project is split into three layers:

- `src/core/` — portable navigation and GNSS algorithms. No STM32 HAL, CMSIS, Win32 or GUI dependencies.
- `src/platform/` — hardware/platform adapters.
- `apps/` — target-specific composition and entry points.

The same files from `src/core/` are intended to compile on Windows and STM32 with the platform layer providing I/O, time and hardware services.

See [`docs/architecture.md`](docs/architecture.md) for the portability rules.
