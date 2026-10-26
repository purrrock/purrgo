# PurrGO — STM32U585CIU6 Development Board Pinout

> Рабочий документ проекта. Здесь фиксируется назначение выводов платы **WeAct STM32U585CIU6** (проект `apps/stm32/PurrGO_STM32U5`, microSD по SPI). Для платы WeAct STM32F411CEU6 (microSD по SDIO) см. [`PurrGO_STM32F411_PINOUT.md`](PurrGO_STM32F411_PINOUT.md). Поддержка обеих плат сохраняется.
>
> **Правило:** если подключение ещё не подтверждено или не решено, оно явно помечается как `PLANNED` / `TBD`. Не следует считать такое назначение установленным.
>
> **Состояние на 2026-10-24:** проект CubeMX для U585 ещё не создан, ни одно назначение не настроено в CubeMX и не проверено на железе U585. Решения по распиновке приняты (`TODO_migrate.md`, раздел 2), поэтому все назначения имеют статус `PLANNED`. Выводы E-Ink, UART2 и кнопок совпадают с F411 и проверены на F411, но на U585 их нужно проверить заново.

## 1. Контроллер и плата

- MCU: **STM32U585CIU6** (UFQFPN48, вариант без SMPS: питание ядра от внутреннего LDO, на VCAP1 4.7 мкФ). SDMMC в этом корпусе нет.
- Плата: WeAct STM32U585Cx CoreBoard V3.1. Схема: `docs/STM32U585CIU6/`.
- Отладочный интерфейс: ST-LINK, **SWD** (PA13/PA14), 4-контактный разъём `SW 3V3 DIO SCK GND`. Запасной путь прошивки: BOOT0 + системный DFU через USB-C.
- Отладочный текстовый вывод: USART2.
- Тактирование (решения D4, D9): **HSI16 → PLL1 → SYSCLK 80 МГц** (M=1, N=10, R=2), Voltage Scaling **Range 2**, HSE и LSE выключены. Кварцы на плате (Y1 32.768 кГц на PC14/PC15, Y2 25 МГц на PH0/PH1) не используются.
- Питание на период разработки: **5 В по USB-C** (LDO платы ME6231A33M3G → 3.3 В). Питание от батарей (TPS63802) и мониторинг батареи отложены; **не подавать 3.3 В от внешнего преобразователя на вывод `3V3`, пока подключён USB.**

## 2. Основные интерфейсы

| Интерфейс STM32 | Назначение | Статус |
|---|---|---|
| SPI1 | E-Ink дисплей (только передача) | **PLANNED** |
| SPI2 | MicroSD-карта (SPI-режим) | **PLANNED** |
| GPIO | Кнопки E-Ink HAT K1…K4 | **PLANNED** |
| USART1 | GNSS-приёмник (RX через GPDMA1, кольцевой) | **PLANNED** |
| USART2 | Отладочный лог | **PLANNED** |
| ADC1 | Мониторинг батареи (PB1, ADC1_IN16) | **RESERVED**, отложено |

### SPI1 — E-Ink

Режим CubeMX: Transmit Only Master, MISO не используется, `Master Keep IO State` включён.

| Сигнал | Пин STM32U585 | AF | Описание |
|---|---|---|---|
| SPI1_SCK | PA5 | AF5 | CLK дисплея |
| SPI1_MOSI | PA7 | AF5 | DIN дисплея |

Остальные линии дисплея — обычные GPIO (раздел 4). **Не распаивать** flash W25Qxx на плате: её посадочное место подключено к PA4/PA5/PA6/PA7, тем же линиям SPI1.

### SPI2 — MicroSD

MicroSD подключена по SPI (решение D1). SDMMC в UFQFPN48 у STM32U585CI отсутствует.

| Сигнал SPI | Пин STM32U585 | AF | Вывод microSD-модуля | Статус |
|---|---|---|---|---|
| SPI2_SCK | PB13 | AF5 | CLK | **PLANNED** |
| SPI2_MISO | PB14 | AF5 | DAT0 (DO) | **PLANNED** |
| SPI2_MOSI | PB15 | AF5 | CMD (DI) | **PLANNED** |
| SD_CS | PB12 | GPIO_Output | DAT3 (CS) | **PLANNED** |
| Detect_SD | PB10 | GPIO_Input | CD | **PLANNED** |

DAT1 и DAT2 модуля не подключаются (подтяжки на модуле есть).

- Питание карты — 3.3 В.
- Подтяжки на модуле есть, внешние не нужны. Проверить мультиметром, что DAT3 (CS) подтянут к 3.3 В: это держит CS высоким при сбросе МК.
- `SD_CS`: GPIO_Output, начальный уровень **High**, скорость Very high. Скорость SCK/MOSI/MISO — Very high.
- Частота: инициализация карты ≤ 400 кГц (при PCLK1 = 80 МГц делитель `/256` ≈ 312 кГц), рабочая 12.5…25 МГц переключается программно после инициализации.
- `Detect_SD`: вход с Pull-up. Полярность: `PURRGO_SD_DETECT_MODE = PURRGO_SD_DETECT_ACTIVE_HIGH` (карта вставлена = высокий уровень, проверено на плате).
- FatFs: в CubeMX для U5 недоступен (только FileX), подключается вручную, SPI-драйвер диска пишется поверх SPI2.

### USART1 — GNSS

| STM32U585 | GNSS | Назначение | Статус |
|---|---|---|---|
| PA9 / USART1_TX (AF7) | RX | STM32 → GNSS (решение D2) | **PLANNED** |
| PA10 / USART1_RX (AF7) | TX | GNSS → STM32 | **PLANNED** |
| GND | GND | Общая земля | **PLANNED** |

9600 8N1, приём через GPDMA1 в кольцевом режиме (linked-list circular, решение D5). Резерв: приём по прерыванию в кольцевой буфер.

### USART2 — отладочный лог

- Подключение: STM32 → USB-UART адаптер (PL2303TA TTL) → ПК.
- Параметры терминала: `115200`, `8N1`.

| STM32U585 | USB-UART | Цвет провода | Назначение |
|---|---|---|---|
| PA2 / USART2_TX (AF7) | RX | белый | STM32 → ПК |
| PA3 / USART2_RX (AF7) | TX | зелёный | ПК → STM32 |
| GND | GND | чёрный | Общая земля |

## 3. E-Ink дисплей

**Waveshare 2.7" e-Paper HAT, 264×176.**

| Контакт дисплея | Пин STM32U585CIU6 | Описание и настройка в CubeMX | Статус |
|---|---|---|---|
| VCC | 3V3 | Питание | **PLANNED** |
| GND | GND | Общая земля | **PLANNED** |
| DIN | PA7 | SPI1_MOSI | **PLANNED** |
| CLK | PA5 | SPI1_SCK | **PLANNED** |
| CS | PB7 | GPIO_Output, `EINK_CS`, High | **PLANNED** |
| DC | PB8 | GPIO_Output, `EINK_DC`, Low | **PLANNED** |
| RST | PB9 | GPIO_Output, `EINK_RST`, High | **PLANNED** |
| BUSY | PB2 | GPIO_Input, `EINK_BUSY`, без подтяжки | **PLANNED** |

Для E-Ink не используется SPI1_MISO. PB2 на плате подтянут к GND резистором 10 кОм (R3); для BUSY это не мешает, дисплей управляет линией активно.

## 4. Кнопки

Используются четыре кнопки Waveshare 2.7" e-Paper HAT (K1…K4) на гребёнке 40 pin, совместимой с Raspberry Pi. Каждая замыкает вывод на GND, поэтому входы настроены с внутренним Pull-up: отпущена = 1, нажата = 0.

| Кнопка | Вывод HAT (нумерация Raspberry Pi) | Пин STM32 | Метка CubeMX | Статус |
|---|---|---|---|---|
| K1 (KEY1) | GPIO5 | PA0 | `K1` | **PLANNED** |
| K2 (KEY2) | GPIO6 | PA1 | `K2` | **PLANNED** |
| K3 (KEY3) | GPIO13 | PC14 | `K3` | **PLANNED** |
| K4 (KEY4) | GPIO19 | PC15 | `K4` | **PLANNED** |

- PA0: на плате встроенная кнопка KEY через 330 Ом на GND, **подтяжки к питанию на плате нет**, внутренний Pull-up в CubeMX обязателен. PA0 = `WKUP1` (пробуждение по низкому уровню).
- PC14/PC15 — выводы LSE (OSC32), на плате установлен кварц Y1 32.768 кГц; LSE в проекте выключен. На F411 кварц не мешал, **на U585 проверить заново** (этап 5, п. 5 `TODO_migrate.md`). Если K3/K4 не читаются, кандидаты для переноса: PA8, PB0, PB6.
- Код: `Core/Src/buttons.c`, выводы берутся из меток CubeMX `K1_Pin`…`K4_Pin` (`main.h`). Назначение функций — `docs/PURRGO_KEYS.md`.

## 5. Светодиод

| GPIO | Назначение | Статус |
|---|---|---|
| PC13 | `Alive_LED`, GPIO_Output; анод через 1.5 кОм к 3.3 В, **включается низким уровнем** (как на F411) | **PLANNED** |

Красный светодиод PWR (D3, 5.1 кОм) горит постоянно. Светодиоды платы отпаиваются после отладки устройства.

## 6. Полная таблица ресурсов STM32U585

Рабочий реестр. Новые назначения следует добавлять сюда до использования GPIO в коде.

| GPIO / ресурс | Назначение | Статус |
|---|---|---|
| PA0 | K1 (кнопка E-Ink HAT), WKUP1 | **PLANNED** |
| PA1 | K2 (кнопка E-Ink HAT) | **PLANNED** |
| PA2 | USART2_TX / Debug | **PLANNED** | (зеленый)
| PA3 | USART2_RX / Debug | **PLANNED** | (белый)
| PA5 | SPI1_SCK / E-Ink | **PLANNED** |
| PA7 | SPI1_MOSI / E-Ink | **PLANNED** |
| PA9 | USART1_TX / GNSS | **PLANNED** |
| PA10 | USART1_RX / GNSS | **PLANNED** |
| PB1 | V_MAIN_BAT, ADC1_IN16: мониторинг батареи отложен, в CubeMX не настраивать | **RESERVED** |
| PB2 | E-Ink BUSY | **PLANNED** |
| PB7 | E-Ink CS | **PLANNED** |
| PB8 | E-Ink DC | **PLANNED** |
| PB9 | E-Ink RST | **PLANNED** |
| PB10 | Detect_SD / MicroSD CD | **PLANNED** |
| PB12 | SD_CS / MicroSD | **PLANNED** |
| PB13 | SPI2_SCK / MicroSD | **PLANNED** |
| PB14 | SPI2_MISO / MicroSD | **PLANNED** |
| PB15 | SPI2_MOSI / MicroSD | **PLANNED** |
| PC13 | Alive_LED | **PLANNED** |
| PC14 | K3 (кнопка E-Ink HAT), LSE-вывод | **PLANNED** |
| PC15 | K4 (кнопка E-Ink HAT), LSE-вывод | **PLANNED** |

| SPI1 | E-Ink (TX only) | **PLANNED** |
| SPI2 | MicroSD | **PLANNED** |
| USART1 | GNSS | **PLANNED** |
| USART2 | Debug log | **PLANNED** |
| GPDMA1 | USART1_RX (GNSS, linked-list circular) | **PLANNED** |
| ADC1 | мониторинг батареи, отложен | **RESERVED** |

## 7. Правила ведения pinout

1. **Не использовать GPIO в коде, если его назначение не зафиксировано здесь или в актуальной конфигурации CubeMX.**
2. После изменения распиновки сначала обновлять этот документ, затем код.
3. Не переносить распиновку из примеров для других STM32 автоматически.
4. Назначение, принятое решением, но ещё не настроенное в CubeMX, имеет статус `PLANNED`. После настройки в CubeMX — `CONFIGURED`, после проверки на железе U585 — `FIXED`.
5. Освобождённый GPIO необходимо удалить из назначения или явно пометить как `FREE`.
6. Конфликты GPIO между периферийными устройствами должны быть устранены до реализации драйверов.
7. SPI1 используется только E-Ink, SPI2 только microSD. Если на шину будут добавлены другие устройства, у каждого должен быть собственный CS, и CS разных устройств не должны быть активны одновременно.
8. Выводы SPI2 (PB12…PB15) закреплены за microSD и не должны использоваться как GPIO.
9. Распиновка F411 и U585 ведётся в отдельных документах и не должна смешиваться.

## 8. Отличия от F411

| Что | F411 | U585 |
|---|---|---|
| microSD | SDIO 4 бита: PB15 CK, PA6 CMD, PB4 D0, PA8 D1, PA9 D2, PB5 D3, PB12 Detect | SPI2: PB13 SCK, PB14 MISO, PB15 MOSI, PB12 CS, PB10 Detect |
| GNSS TX (STM32 → GNSS RX) | PA15 | PA9 |
| DMA для GNSS | DMA2 Stream2 (NDTR) | GPDMA1, linked-list circular |
| Тактирование | HSI → PLL, 84 МГц | HSI16 → PLL1, 80 МГц, Range 2 |
| Батарейный АЦП | PB1 = ADC1_IN9 (не настроен) | PB1 = ADC1_IN16, отложено |
| PB2 | BOOT1 | обычный GPIO |

Остальная проводка (E-Ink, UART2, кнопки K1…K4, LED) одинакова на обеих платах.

## 9. Статусы

- **FIXED** — назначение принято и подтверждено на железе U585 (или задано платой).
- **CONFIGURED** — назначение настроено в CubeMX, но физически ещё не подтверждено.
- **PLANNED** — решение принято, в CubeMX ещё не настроено.
- **RESERVED** — ресурс зарезервирован.
- **TBD** — назначение ещё не решено.
- **FREE** — GPIO свободен.

---

## История изменений

| Дата | Изменение |
|---|---|
| 2026-10-24 | Создан документ распиновки U585 по принятым решениям D1…D5, D7, D9 (`TODO_migrate.md`): microSD на SPI2 (PB12…PB15, Detect PB10), GNSS TX на PA9, USART1_RX через GPDMA1, HSI16 → 80 МГц. Мониторинг батареи (PB1) отложен, питание на период разработки от USB 5 В. |
