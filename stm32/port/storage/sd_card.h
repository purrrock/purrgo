#ifndef PURRGO_STM32_SD_CARD_H
#define PURRGO_STM32_SD_CARD_H

/*
 * microSD-карта PurrGO + FatFs (STM32U585, stm32).
 *
 * Карта подключена по SPI2 и работает в SPI-режиме (в корпусе UFQFPN48 у
 * STM32U585CI нет SDMMC). Протокол карты - в stm32/port/storage (без HAL,
 * проверяется host-тестом), диск FatFs (disk_*) и привязка к HAL -
 * stm32/port/storage/sd_card.c проекта.
 * Detect: вывод Detect_SD (PB10), CS: SD_CS (PB12).
 *
 * Модуль sd_card.c (пользовательский, CubeMX его не перезаписывает) отвечает за:
 *   - монтирование файловой системы и создание дерева каталогов PURRGO;
 *   - отслеживание вставки/извлечения карты по выводу Detect;
 *   - защиту от зависаний при извлечении карты во время обмена;
 *   - питание карты (состояния OFF / ON, см. ниже).
 *
 * Состояния питания (последовательность - sd_power.h):
 *   OFF  - питание карты снято (если в схеме есть ключ, вывод CubeMX SD_PWR), SPI2
 *          выключен, линии припаркованы, ФС не смонтирована, вставка не отслеживается;
 *          purrgo_sd_state() == PURRGO_SD_OFF. Без ключа карта остаётся запитанной, но не
 *          выбрана (CS = High), SPI2 выключен.
 *   ON   - питание подано, дальше состояние карты по purrgo_sd_state(): NO_CARD,
 *          MOUNT_ERROR, READY.
 * Переход OFF -> ON: purrgo_sd_init() (при старте) и purrgo_sd_power_up(); ON -> OFF:
 * purrgo_sd_power_down() (при выключении устройства purrgo_platform_storage_unmount()).
 */

#include <stdbool.h>
#include <stdint.h>

/*
 * Полярность вывода Detect (Detect_SD, PB10).
 *
 *   PURRGO_SD_DETECT_ACTIVE_HIGH  - карта вставлена: вывод = 1. По умолчанию:
 *                                   так работает используемый картридер
 *                                   (проверено на плате U585).
 *   PURRGO_SD_DETECT_ACTIVE_LOW   - карта вставлена: вывод = 0 (контакт
 *                                   гнезда замыкается на GND). Нужна подтяжка
 *                                   к питанию (Pull-up в CubeMX либо на модуле).
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
#define PURRGO_SD_DETECT_MODE PURRGO_SD_DETECT_ACTIVE_HIGH
#endif

#if (PURRGO_SD_DETECT_MODE != PURRGO_SD_DETECT_ACTIVE_LOW) && \
    (PURRGO_SD_DETECT_MODE != PURRGO_SD_DETECT_ACTIVE_HIGH) && \
    (PURRGO_SD_DETECT_MODE != PURRGO_SD_DETECT_NONE)
#error "Invalid PURRGO_SD_DETECT_MODE"
#endif

typedef enum
{
    /* Карты нет (по выводу Detect_SD). */
    PURRGO_SD_NO_CARD = 0,

    /* Карта есть, но смонтировать не удалось (нет FAT, сбой обмена и т.д.). */
    PURRGO_SD_MOUNT_ERROR,

    /* Файловая система смонтирована, 0:/PURRGO/... доступен. */
    PURRGO_SD_READY,

    /* Питание карты снято (purrgo_sd_power_down()), ФС не смонтирована. */
    PURRGO_SD_OFF
} purrgo_sd_state_t;

/**
 * @brief Определить наличие карты, смонтировать FatFs, создать каталоги.
 *
 * Вызывается один раз при старте, после инициализации периферии CubeMX
 * (MX_SPI2_Init()).
 *
 * @return true, если файловая система смонтирована.
 */
bool purrgo_sd_init(void);

/**
 * @brief Обесточить карту и перевести её выводы (в том числе Detect_SD) в состояние OFF.
 *
 * Вызывать как можно раньше после MX_GPIO_Init(), до долгих этапов старта (баннер,
 * ожидание панели): до этого подтяжка Detect_SD подпитывает VCC карты через модуль
 * (около 1,9 В на выходе ключа). Повторные вызовы и purrgo_sd_init() безопасны; время
 * выдержки в OFF перед включением отсчитывается от этого вызова.
 */
void purrgo_sd_park(void);

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

/**
 * @brief Подать питание карты (если снято), дождаться его, смонтировать ФС.
 *
 * Если ФС уже смонтирована - ничего не делает. Блокирует главный цикл: пауза без питания
 * (PURRGO_SD_POWER_OFF_MIN_MS, если ключ был выключен недавно), нарастание питания
 * (PURRGO_SD_POWER_RAMP_MS) и инициализация карты (десятки - сотни мс). После успеха
 * открытые прежде файлы недействительны (их закрыл purrgo_sd_power_down()), ядро открывает
 * их заново; stm32_app.c по переходу в READY закрывает слои карты.
 *
 * @return true, если файловая система смонтирована.
 */
bool purrgo_sd_power_up(void);

/**
 * @brief Дождаться конца внутренней записи карты, размонтировать ФС, снять питание.
 *
 * Все файлы (трек, конфигурация, слои карты) вызывающий код закрывает ДО вызова.
 * После вызова purrgo_sd_state() == PURRGO_SD_OFF, purrgo_sd_poll() ничего не делает.
 */
void purrgo_sd_power_down(void);

/**
 * @brief Парковка карты по тайм-ауту простоя (PURRGO_SD_PARK_TIMEOUT_MS, 30 с; 0 - выключена).
 *
 * Если ФС смонтирована, открытых файлов и каталогов нет и обмена с картой не было
 * PURRGO_SD_PARK_TIMEOUT_MS, питание снимается (purrgo_sd_state() == PURRGO_SD_OFF,
 * purrgo_sd_is_parked() == true). Первое обращение к ФС через purrgo_fs_open/opendir/
 * exists/replace включает питание и монтирует ФС (purrgo_sd_wake(): блокирует вызывающего
 * на время выдержки в OFF и инициализации карты, десятки - сотни мс); если карты уже нет,
 * вызов ФС просто вернёт ошибку. Вставка карты в припаркованном состоянии замечается при
 * следующем обращении. Явное purrgo_sd_power_down() парковкой не считается: само питание
 * не вернётся.
 *
 * Вызывать из главного цикла (после purrgo_sd_poll()). Без ключа питания ничего не делает.
 */
void purrgo_sd_idle_service(void);

/**
 * @brief Отметить обращение к карте (сдвигает отсчёт тайм-аута парковки).
 *        Обмен с картой отмечается сам; fs_hal вызывает это при open/close.
 */
void purrgo_sd_touch(void);

/**
 * @brief Если карта припаркована по тайм-ауту - включить и смонтировать (иначе только
 *        отметить обращение).
 * @return true, если ФС смонтирована после вызова.
 */
bool purrgo_sd_wake(void);

/**
 * @brief true один раз, если последний переход в READY был пробуждением из парковки.
 *        В этом случае открытых файлов не было, и слои карты закрывать не нужно.
 */
bool purrgo_sd_take_park_wake(void);

/** @brief true, если питание снято парковкой по тайм-ауту (а не явным power_down). */
bool purrgo_sd_is_parked(void);

/** @brief Число открытых файлов и каталогов fs_hal_stm32.c (для решения о парковке). */
uint32_t purrgo_fs_stm32_open_count(void);

/** @brief true, если питание карты подано (состояние ON). */
bool purrgo_sd_is_powered(void);

/**
 * @brief Удержать ключ питания карты в состоянии «выключено» на время Standby МК.
 *
 * Вызывается перед входом в Standby (до HAL_PWREx_EnablePullUpPullDownConfig()). Без
 * ключа питания (нет вывода SD_PWR) ничего не делает.
 */
void purrgo_sd_standby_hold(void);

/** @brief Текущее состояние карты. */
purrgo_sd_state_t purrgo_sd_state(void);

/** @brief true, если файловая система смонтирована. */
bool purrgo_sd_is_ready(void);

/**
 * @brief Диагностика: принудительно использовать обмен через HAL (true) или
 *        быстрый обмен по регистрам (false, по умолчанию).
 *
 * Нужна hw_test для сверки данных двумя способами.
 */
void purrgo_sd_spi_force_hal_xfer(bool force);

#endif /* PURRGO_STM32_SD_CARD_H */
