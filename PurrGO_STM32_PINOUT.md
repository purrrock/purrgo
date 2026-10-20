# PurrGO — STM32F411CEU6 Development Board Pinout

> Рабочий документ проекта. Здесь фиксируется фактическое назначение выводов STM32F411CEU6 на этапе разработки PurrGO.
>
> **Правило:** если подключение ещё не подтверждено или не решено, оно явно помечается как `НЕ РЕШЕНО`. Не следует считать такое назначение установленным.

## 1. Контроллер

- MCU: **STM32F411CEU6** (UFQFPN48)
- Проект: **PurrGO**
- Назначение текущего этапа: разработка и отладка GNSS-навигатора/логгера.
- Отладочный интерфейс: ST-LINK, только **SWD** (PA13/PA14). JTAG недоступен: PA15 (JTDI) и PB4 (NJTRST) заняты под USART1_TX и SDIO_D0.
- Отладочный текстовый вывод: UART2.
- Тактирование: HSI 16 МГц → PLL → SYSCLK 84 МГц; PLLQ = 42 МГц (тактирование SDIO).

## 2. Основные интерфейсы

| Интерфейс STM32 | Назначение | Статус |
|---|---|---|
| SPI1 | E-Ink дисплей (только передача) | **Определено** |
| SDIO (4 бита) | MicroSD-карта | **Определено** (в CubeMX), на железе не проверено |
| UART1 | GNSS-приёмник | **Определено** |
| UART2 | Отладочный лог | **Определено** |

### SPI1 — E-Ink

SPI1 используется только для E-Ink дисплея Waveshare 2.7" e-Paper HAT. Режим CubeMX: Transmit Only Master, MISO не используется.

| Сигнал | Пин STM32F411CEU6 | Описание |
|---|---|---|
| SPI1_SCK | PA5 | CLK дисплея |
| SPI1_MOSI | PA7 | DIN дисплея |

Остальные линии дисплея — обычные GPIO (см. раздел 4).

### SDIO — MicroSD

MicroSD подключена по SDIO, шина 4 бита. SPI2 больше не используется.

| Сигнал SDIO | Пин STM32F411CEU6 | Альтернативная функция | Статус |
|---|---|---|---|
| SDIO_CK | PB15 | AF12 | **CONFIGURED** |
| SDIO_CMD | PA6 | AF12 | **CONFIGURED** |
| SDIO_D0 | PB4 | AF12 | **CONFIGURED** |
| SDIO_D1 | PA8 | AF12 | **CONFIGURED** |
| SDIO_D2 | PA9 | AF12 | **CONFIGURED** |
| SDIO_D3 | PB5 | AF12 | **CONFIGURED** |
| Detect_SDIO | PB12 | GPIO_Input | **CONFIGURED** |

Питание карты — 3.3 В.

Настройки CubeMX/FatFs:

- SDIO: SD 4 bits Wide bus, `SDIO_HARDWARE_FLOW_CONTROL_ENABLE`, ClockDiv = 0.
- Частоты: идентификация карты ≈ 350 кГц (42 МГц / 120), рабочая шина 21 МГц (42 МГц / 2).
- FatFs: драйвер диска «SD» (`FATFS/Target/sd_diskio.c`, `bsp_driver_sd.c`), обмен в режиме опроса. DMA SDIO (DMA2 Stream3 — RX, Stream6 — TX) настроен в CubeMX, но в этом режиме не используется.
- Detect_SDIO: вход. По умолчанию прошивка считает, что карта вставлена, если на выводе **низкий** уровень (контакт гнезда замыкается на GND), как в коде CubeMX. Нужна подтяжка к 3.3 В (на модуле или Pull-up в CubeMX). Полярность и отключение Detect задаются макросом `PURRGO_SD_DETECT_MODE` (см. `Core/Inc/sd_card.h`).
- Пины SDIO нельзя использовать как GPIO.

Код: `Core/Src/sd_card.c` (монтирование, Detect, защита от зависаний), `Core/Src/fs_hal_stm32.c` (файловый HAL PurrGO поверх FatFs).

### UART1 — GNSS

| STM32F411 | GNSS | Назначение | Статус |
|---|---|---|---|
| PA15 / USART1_TX | RX | STM32 → GNSS | **CONFIGURED** (перенесён с PA9) |
| PA10 / USART1_RX | TX | GNSS → STM32 | **FIXED** |
| GND | GND | Общая земля | **FIXED** |

PA9 освобождён от USART1_TX и теперь занят SDIO_D2. Провод к RX GNSS-модуля нужно переставить на PA15.

### UART2 — отладочный лог

UART2 предназначен исключительно для диагностического текстового вывода во время разработки.

- Подключение: STM32 → USB-UART адаптер (PL2303TA TTL) → ПК.
- Параметры терминала: `115200`, `8N1`.

| STM32F411 | USB-UART | Цвет провода | Назначение |
|---|---|---|---|
| PA2 / USART2_TX | RX | белый | STM32 → ПК |
| PA3 / USART2_RX | TX | зелёный | ПК → STM32 |
| GND | GND | чёрный | Общая земля |

## 3. GPIO, назначение которых уже известно

| GPIO | Назначение | Статус |
|---|---|---|
| PB2 | E-Ink BUSY | **FIXED** |
| PB7 | E-Ink CS | **FIXED** |
| PB8 | E-Ink DC | **FIXED** |
| PB9 | E-Ink RST | **FIXED** |
| PB12 | Detect_SDIO (MicroSD) | **CONFIGURED** |
| PC13 | Alive LED | **FIXED** |

## 4. E-Ink дисплей

**Waveshare 2.7" e-Paper HAT, 264×176.**

| Контакт дисплея | Пин STM32F411CEU6 | Описание и настройка в CubeMX | Статус |
|---|---|---|---|
| VCC | 3V3 | Питание | **FIXED** |
| GND | GND | Общая земля | **FIXED** |
| DIN | PA7 | SPI1_MOSI | **FIXED** |
| CLK | PA5 | SPI1_SCK | **FIXED** |
| CS | PB7 | GPIO_Output, `EINK_CS` | **FIXED** |
| DC | PB8 | GPIO_Output, `EINK_DC` | **FIXED** |
| RST | PB9 | GPIO_Output, `EINK_RST` | **FIXED** |
| BUSY | PB2 | GPIO_Input, `EINK_BUSY` | **FIXED** |

Для E-Ink не используется SPI1_MISO.

Отладочный TFT-дисплей ST7789 удалён из проекта; его выводы (PA4, PB0, PB1) свободны.

## 5. GNSS

| Сигнал | STM32 | Статус |
|---|---|---|
| GNSS TX → STM32 RX | PA10 / USART1_RX | **FIXED** |
| STM32 TX → GNSS RX | PA15 / USART1_TX | **CONFIGURED** |
| GND | GND | **FIXED** |
| питание | согласно модулю | **FIXED** |

## 6. Кнопки

Временные отладочные кнопки (PB3, PB4, PB5, PB6) удалены. PB4 и PB5 теперь SDIO_D0 и SDIO_D3.

Обработка кнопок в прошивке временно **отключена** (закомментирована в `stm32_app.c`, `buttons.c` исключён из сборки в `CMakeLists.txt`) до подключения кнопок E-Ink HAT.

Кнопки E-Ink HAT: четыре кнопки K1…K4, каждая замыкает на GND свой вывод разъёма HAT (по схеме HAT — GPIO5, GPIO6, GPIO13, GPIO19 в нумерации Raspberry Pi). Подключение этих сигналов к выводам STM32 — **НЕ РЕШЕНО**.

## 7. Светодиод

| GPIO | Назначение | Статус |
|---|---|---|
| PC13 | Alive LED | **FIXED** |

## 8. Свободные выводы

Не используются в CubeMX:

| GPIO | Примечание | Статус |
|---|---|---|
| PA0, PA1 | — | **FREE** |
| PA4 | ранее ST7789 CS | **FREE** |
| PB0, PB1 | ранее ST7789 RST, DC | **FREE** |
| PB3 | SWO ST-LINK | **FREE** |
| PB6 | ранее отладочная кнопка | **FREE** |
| PB10, PB13, PB14 | PB10, PB14 ранее SPI2 (MicroSD) | **FREE** |
| PA11, PA12 | на отладочных платах обычно USB D−/D+ | **FREE** |
| PC14, PC15 | на отладочных платах обычно выводы кварца LSE | **FREE** |

## 9. Полная таблица ресурсов STM32

Рабочий реестр. Новые назначения следует добавлять сюда до использования GPIO в коде.

| GPIO / ресурс | Назначение | Статус |
|---|---|---|
| PA2 | USART2_TX / Debug | **FIXED** |
| PA3 | USART2_RX / Debug | **FIXED** |
| PA5 | SPI1_SCK / E-Ink | **FIXED** |
| PA6 | SDIO_CMD / MicroSD | **CONFIGURED** |
| PA7 | SPI1_MOSI / E-Ink | **FIXED** |
| PA8 | SDIO_D1 / MicroSD | **CONFIGURED** |
| PA9 | SDIO_D2 / MicroSD | **CONFIGURED** |
| PA10 | USART1_RX / GNSS | **FIXED** |
| PA13 | SWDIO | **FIXED** |
| PA14 | SWCLK | **FIXED** |
| PA15 | USART1_TX / GNSS | **CONFIGURED** |
| PB2 | E-Ink BUSY | **FIXED** |
| PB4 | SDIO_D0 / MicroSD | **CONFIGURED** |
| PB5 | SDIO_D3 / MicroSD | **CONFIGURED** |
| PB7 | E-Ink CS | **FIXED** |
| PB8 | E-Ink DC | **FIXED** |
| PB9 | E-Ink RST | **FIXED** |
| PB12 | Detect_SDIO / MicroSD | **CONFIGURED** |
| PB15 | SDIO_CK / MicroSD | **CONFIGURED** |
| PC13 | Alive LED | **FIXED** |
| SPI1 | E-Ink (TX only) | **FIXED** |
| SDIO | MicroSD, 4 бита | **CONFIGURED** |
| UART1 | GNSS | **FIXED** |
| UART2 | Debug log | **FIXED** |
| DMA2 Stream2 | USART1_RX (GNSS, circular) | **FIXED** |
| DMA2 Stream3 / Stream6 | SDIO_RX / SDIO_TX (настроены, в режиме опроса не используются) | **RESERVED** |

## 10. Правила ведения pinout

1. **Не использовать GPIO в коде, если его назначение не зафиксировано здесь или в актуальной конфигурации CubeMX.**
2. После изменения распиновки сначала обновлять этот документ, затем код.
3. Не переносить распиновку из примеров для других STM32 автоматически.
4. Если GPIO назначен в CubeMX, но физическое подключение ещё не проверено, статус должен быть `CONFIGURED`, а не `FIXED`.
5. После фактической проверки на железе статус можно изменить на `FIXED`.
6. Освобождённый GPIO необходимо удалить из назначения или явно пометить как `FREE`.
7. Конфликты GPIO между периферийными устройствами должны быть устранены до реализации драйверов.
8. SPI1 сейчас используется только E-Ink. Если на неё будут добавлены другие устройства, у каждого должен быть собственный CS, и CS разных устройств не должны быть активны одновременно.
9. Выводы SDIO (PA6, PA8, PA9, PB4, PB5, PB15) закреплены за периферией SDIO и не должны использоваться как GPIO.

## 11. Статусы

- **FIXED** — назначение принято и подтверждено.
- **CONFIGURED** — назначение настроено в CubeMX, но физически ещё не подтверждено.
- **RESERVED** — ресурс зарезервирован под устройство.
- **TBD** — назначение ещё не решено.
- **FREE** — GPIO свободен.

---

## История изменений

| Дата | Изменение |
|---|---|
| 2026-09-07 | Создан рабочий документ распиновки PurrGO. Зафиксированы SPI2 → MicroSD, PB12 → MicroSD CS, UART1 → GNSS, UART2 → debug log, SPI1 зарезервирован под периферию/E-Ink. |
| 2026-09-16 | Зафиксировано подключение Waveshare 2.7" E-Ink: PA5/PA7 — общие SPI1 SCK/MOSI, PB7 — CS, PB8 — DC, PB9 — RST, PB2 — BUSY. ST7789 остаётся на PA4/PB0/PB1. |
| 2026-10-19 | MicroSD переведена с SPI2 на SDIO (4 бита): PB15 CK, PA6 CMD, PB4 D0, PA8 D1, PA9 D2, PB5 D3, PB12 Detect. SPI2 удалён. USART1_TX перенесён с PA9 на PA15. Дисплей ST7789 удалён, PA4/PB0/PB1 свободны. Временные кнопки PB3…PB6 удалены, обработка кнопок отключена до подключения кнопок E-Ink HAT. |
