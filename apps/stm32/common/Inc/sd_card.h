#ifndef PURRGO_STM32_SD_CARD_H
#define PURRGO_STM32_SD_CARD_H

/*
 * microSD-карта PurrGO + FatFs. Общий для F411 и U585 интерфейс; реализация
 * (sd_card.c) у каждой платы своя, в каталоге её проекта.
 *
 * F411 (apps/stm32/PurrGO_STM32): SDIO, 4 бита.
 *   Низкоуровневую часть (HAL SD, FatFs-драйвер "SD") генерирует CubeMX:
 *     Core/Src/sdio.c                 - SDIO, DMA, пины
 *     FATFS/Target/bsp_driver_sd.c    - BSP поверх HAL SD
 *     FATFS/Target/sd_diskio.c        - диск FatFs
 *   sd_card.c подменяет слабые (__weak) функции BSP версиями, ограниченными по
 *   времени, и даёт буфер-посредник для невыровненных буферов FatFs.
 *   Detect: вывод Detect_SDIO (PB12).
 *
 * U585 (apps/stm32/PurrGO_STM32U5): SPI2, режим SPI карты (в корпусе
 *   UFQFPN48 у STM32U585CI нет SDMMC). Протокол карты - в
 *   apps/stm32/common/sd_spi (без HAL, проверяется host-тестом), диск FatFs -
 *   apps/stm32/common/fatfs/diskio.c, привязка к HAL - sd_card.c проекта.
 *   Detect: вывод Detect_SD (PB10), CS: SD_CS (PB12).
 *
 * Модуль sd_card.c (пользовательский, CubeMX его не перезаписывает) отвечает за:
 *   - монтирование файловой системы и создание дерева каталогов PURRGO;
 *   - отслеживание вставки/извлечения карты по выводу Detect;
 *   - защиту от зависаний при извлечении карты во время обмена.
 */

#include <stdbool.h>
#include <stdint.h>

/*
 * Полярность вывода Detect (F411: Detect_SDIO PB12, U585: Detect_SD PB10).
 *
 *   PURRGO_SD_DETECT_ACTIVE_LOW   - карта вставлена: вывод = 0 (контакт
 *                                   гнезда замыкается на GND). Используется
 *                                   на F411 и U585 (одинаковый картридер).
 *                                   Нужна подтяжка к питанию (Pull-up в
 *                                   CubeMX либо на самом модуле).
 *   PURRGO_SD_DETECT_ACTIVE_HIGH  - карта вставлена: вывод = 1.
 *   PURRGO_SD_DETECT_NONE         - у модуля нет вывода CD. Карта считается
 *                                   вставленной всегда, горячая замена не
 *                                   отслеживается.
 *
 * Значение можно переопределить в compile definitions.
 */
#define PURRGO_SD_DETECT_ACTIVE_LOW   0
#define PURRGO_SD_DETECT_ACTIVE_HIGH  1
#define PURRGO_SD_DETECT_NONE         2

#ifndef PURRGO_SD_DETECT_MODE
#define PURRGO_SD_DETECT_MODE PURRGO_SD_DETECT_ACTIVE_LOW
#endif

#if (PURRGO_SD_DETECT_MODE != PURRGO_SD_DETECT_ACTIVE_LOW) && \
    (PURRGO_SD_DETECT_MODE != PURRGO_SD_DETECT_ACTIVE_HIGH) && \
    (PURRGO_SD_DETECT_MODE != PURRGO_SD_DETECT_NONE)
#error "Invalid PURRGO_SD_DETECT_MODE"
#endif

typedef enum
{
    /* Карты нет (по выводу Detect_SDIO). */
    PURRGO_SD_NO_CARD = 0,

    /* Карта есть, но смонтировать не удалось (нет FAT, сбой обмена и т.д.). */
    PURRGO_SD_MOUNT_ERROR,

    /* Файловая система смонтирована, 0:/PURRGO/... доступен. */
    PURRGO_SD_READY
} purrgo_sd_state_t;

/**
 * @brief Определить наличие карты, смонтировать FatFs, создать каталоги.
 *
 * Вызывается один раз при старте, после инициализации периферии CubeMX
 * (F411: MX_FATFS_Init() и MX_SDIO_SD_Init(); U585: MX_SPI2_Init()).
 *
 * @return true, если файловая система смонтирована.
 */
bool purrgo_sd_init(void);

/**
 * @brief Обслуживание горячей замены карты.
 *
 * Вызывать из главного цикла. Сама ограничивает частоту работы.
 * При извлечении карты размонтирует FatFs (открытые файлы становятся
 * недействительными и возвращают ошибки), при вставке - монтирует заново.
 * Повторное монтирование блокирует цикл на время инициализации карты
 * (десятки-сотни миллисекунд).
 *
 * @param now_ms Текущее время, мс (purrgo_system_time_ms()).
 */
void purrgo_sd_poll(uint32_t now_ms);

/** @brief Текущее состояние карты. */
purrgo_sd_state_t purrgo_sd_state(void);

/** @brief true, если файловая система смонтирована. */
bool purrgo_sd_is_ready(void);

#endif /* PURRGO_STM32_SD_CARD_H */
