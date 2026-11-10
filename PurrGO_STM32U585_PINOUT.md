# PurrGO — распиновка STM32U585CIU6 (что куда запаять)

Краткий справочник для монтажа. Настройка выводов, правила ведения и статусы - в [`docs/hardware.md`](docs/hardware.md); в коде выводы
берутся из CubeMX (`PurrGO_STM32U5.ioc`, `main.h`).

Плата: WeAct STM32U585Cx CoreBoard V3.1. Питание: три AA (TPS63802) или USB-C 5 В. **Не подавать 5 V и батареи одновременно**, пока
в схему не включён ORing LM66200 WeAct.

## GNSS G10A F30 (LPUART1)

| STM32U585 | Вывод модуля | Назначение |
|---|---|---|
| PA2 (LPUART1_TX) | 4 (R, RX) | STM32 → GNSS |
| PA3 (LPUART1_RX) | 3 (T, TX) | GNSS → STM32 |
| 3V3 | 2 (V, VCC) | питание 3.3 В |
| GND | 5 (G, GND) | земля |
| PB0 (`GNSS_EN`) | 6 (E, EN) | High или не подключен - модуль работает |
| PA8 (`GNSS_PPS`, EXTI8) | 1 (P, 1PPS) | секундная метка при наличии фикса |

## E-Ink Waveshare 2.7" HAT (SPI1)

| Контакт дисплея | Пин гребёнки HAT (физ. / BCM) | STM32U585 |
|---|---|---|
| VCC | 1 или 17 / 3V3 | 3V3 |
| GND | 6 (или 9, 14, 20, 25, 30, 34, 39) | GND |
| DIN | 19 / GPIO10 | PA7 (SPI1_MOSI) |
| CLK | 23 / GPIO11 | PA5 (SPI1_SCK) |
| CS | 24 / GPIO8 | PB7 (`EINK_CS`) |
| DC | 22 / GPIO25 | PB8 (`EINK_DC`) |
| RST | 11 / GPIO17 | PB9 (`EINK_RST`) |
| BUSY | 18 / GPIO24 | PB2 (`EINK_BUSY`) |

Переключатель BS на HAT - 4-line SPI. Пины 2 и 4 (5V), I2C (3, 5) и остальные выводы гребёнки не подключаются.

## Кнопки (E-Ink HAT, каждая на GND)

| Кнопка | BCM | Пин гребёнки (физ.) | STM32U585 |
|---|---|---|---|
| K1 | GPIO5 | 29 | PA0 (`K1`, WKUP1) |
| K2 | GPIO6 | 31 | PA1 (`K2`) |
| K3 | GPIO13 | 33 | PC14 (`K3`) |
| K4 | GPIO19 | 35 | PC15 (`K4`) |

## microSD (SPI2)

| STM32U585 | Вывод microSD-модуля |
|---|---|
| PB13 (SPI2_SCK) | CLK |
| PB14 (SPI2_MISO) | DAT0 (DO) |
| PB15 (SPI2_MOSI) | CMD (DI) |
| PB12 (`SD_CS`) | DAT3 (CS) |
| PB10 (`Detect_SD`) | CD |
| 3V3 / GND | VCC / GND |

DAT1 и DAT2 не подключаются.

## Прочее

| STM32U585 | Назначение |
|---|---|
| PA9 (USART1_TX, AF7) | отладочный лог → RX USB-UART адаптера (белый провод) |
| PA10 (USART1_RX, AF7) | TX USB-UART адаптера (зелёный провод) |
| GND | GND адаптера (чёрный провод) |
| PB1 (ADC1_IN16) | `V_MAIN_BAT`: делитель 1 МΩ + 1 МΩ с «+» батарейного отсека, 100 нФ на GND |
| PC13 | `Alive_LED`, включается низким уровнем |
| PA13 / PA14 | SWD (ST-LINK) |
