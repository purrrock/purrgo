#ifndef PURRGO_SD_POWER_H
#define PURRGO_SD_POWER_H

/*
 * Последовательность включения и выключения питания microSD (без HAL, без FatFs;
 * проверяется host-тестом tests/host/test_sd_power.c). Привязка к GPIO и SPI2 -
 * stm32/port/storage/sd_card.c.
 *
 * Модуль знает только про питание карты и безопасные уровни на её линиях:
 *
 *   OFF  - питание карты снято (ключ выключен), линии «припаркованы»: SCK и MOSI
 *          в Low, CS и MISO в Hi-Z (подтяжка модуля тянет CS к питанию карты, то есть
 *          к нулю). Ни одна линия не подаёт высокий уровень в обесточенную карту:
 *          иначе она запитывается через защитные диоды или подтяжки модуля и остаётся
 *          в неопределённом состоянии. Если ключа нет (rail == NULL), карта запитана
 *          всегда, а «OFF» - лишь «карта не выбрана»: CS = High, SCK = Low.
 *   ON   - питание подано и устоялось, линии в состоянии «простой» (CS = High,
 *          MOSI = High, SCK = Low). Дальше карта получает >= 74 такта при высоком CS
 *          и CMD0 (sd_spi_init()).
 *
 * Переход OFF -> ON:
 *   1. выдержать off_min_ms с момента снятия питания (карта должна успеть разрядиться
 *      ниже 0,5 В, SD Physical Layer Spec, power cycle);
 *   2. включить ключ;
 *   3. подождать ramp_ms (нарастание напряжения <= 35 мс по спецификации + время
 *      включения самой карты, не менее 1 мс до первого такта);
 *   4. перевести линии в «простой».
 * Переход ON -> OFF: линии в «парковку», затем ключ выключается; момент выключения
 * запоминается для пункта 1.
 */

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    SD_POWER_OFF = 0,
    SD_POWER_ON
} sd_power_state_t;

typedef enum
{
    SD_PINS_PARKED = 0,   /* питание карты снято (или карта не выбрана, если ключа нет) */
    SD_PINS_IDLE          /* питание подано, обмена нет: CS = High, MOSI = High, SCK = Low */
} sd_pins_t;

typedef struct
{
    /* Ключ питания карты: on == true - питание подано. NULL - ключа нет. */
    void (*rail)(bool on, void *ctx);

    /* Уровни и режимы линий SCK/MISO/MOSI/CS. */
    void (*pins)(sd_pins_t mode, void *ctx);

    void (*delay_ms)(uint32_t ms, void *ctx);

    /* Монотонный счётчик миллисекунд (переполнение uint32_t допустимо). */
    uint32_t (*tick_ms)(void *ctx);

    /* Минимальная пауза без питания перед повторным включением, мс. */
    uint32_t off_min_ms;

    /* Пауза после включения ключа до первого обращения к карте, мс. */
    uint32_t ramp_ms;

    void *ctx;
} sd_power_port_t;

typedef struct
{
    sd_power_port_t port;
    sd_power_state_t state;
    uint32_t off_since_ms;
    uint32_t cycles;          /* число выполненных включений (диагностика) */
} sd_power_t;

/**
 * @brief Привести питание карты в известное состояние OFF.
 *
 * Вызывается один раз при старте: состояние ключа после сброса МК неизвестно (карта
 * могла остаться запитанной), поэтому питание снимается явно, а отсчёт паузы off_min_ms
 * начинается с этого момента.
 *
 * @return false при неверных аргументах (нужны pins, delay_ms, tick_ms).
 */
bool sd_power_init(sd_power_t *p, const sd_power_port_t *port);

/**
 * @brief OFF -> ON. Блокирует на время паузы off_min_ms (если она не истекла) и ramp_ms.
 *
 * Если питание уже включено - ничего не делает.
 *
 * @return false при неверных аргументах или неинициализированном p.
 */
bool sd_power_up(sd_power_t *p);

/** @brief ON -> OFF. Если питание уже снято - ничего не делает. */
void sd_power_down(sd_power_t *p);

sd_power_state_t sd_power_state(const sd_power_t *p);

/** @brief true, если есть управляемый ключ питания. */
bool sd_power_has_switch(const sd_power_t *p);

#endif /* PURRGO_SD_POWER_H */
