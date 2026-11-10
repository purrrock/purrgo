#ifndef PURRGO_GNSS_LINK_H
#define PURRGO_GNSS_LINK_H

/*
 * Установление связи с GNSS-модулем: определение скорости UART и переход с
 * заводской скорости модуля на рабочую.
 *
 * Модуль после включения работает на заводской скорости (G10A F30: 38400), а
 * после сброса одного МК (без выключения модуля) может оставаться на рабочей.
 * Алгоритм (все ожидания - без блокировки, по gnss_link_poll()):
 *
 *   PROBE   слушать на одной скорости probe_ms; корректное предложение NMEA
 *           (верная контрольная сумма) - модуль найден.
 *             * это рабочая скорость            -> READY;
 *             * это заводская, рабочая другая   -> отправить команду смены
 *               скорости, SWITCH;
 *             * тишина                          -> другая скорость (рабочая
 *               после заводской и наоборот); обе без ответа - новый цикл.
 *   SWITCH  пауза settle_ms, чтобы модуль применил скорость, затем UART на
 *           рабочую скорость.
 *   VERIFY  слушать на рабочей скорости; NMEA есть -> READY; нет -> вернуться
 *           на заводскую (PROBE): модуль не переключился, команду можно
 *           повторить.
 *   READY     связь на рабочей скорости (можно отправлять настройку NMEA);
 *   DEGRADED  модуль отвечает, но не переключается (после attempts команд):
 *             работа на заводской скорости;
 *   FAILED    модуль молчит на обеих скоростях attempts циклов подряд: UART
 *             остаётся на заводской скорости (диагностика - gnss_monitor).
 *
 * Модуль не зависит от HAL и ядра PurrGO: UART и команды приёмника подключаются
 * функциями (gnss_link_ops_t), поэтому логика проверяется host-тестом
 * (tests/host/test_gnss_link.c). Все функции вызываются из основного цикла.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gnss_diag.h"

typedef enum
{
    GNSS_LINK_IDLE = 0,   /* не запущен (GNSS выключен) */
    GNSS_LINK_PROBE,
    GNSS_LINK_SWITCH,
    GNSS_LINK_VERIFY,
    GNSS_LINK_READY,
    GNSS_LINK_DEGRADED,
    GNSS_LINK_FAILED
} gnss_link_state_t;

typedef struct
{
    /* Перенастроить UART на скорость и перезапустить приём; false - ошибка железа. */
    bool (*set_baud)(uint32_t baud, void *ctx);
    /*
     * Отправить модулю команду смены скорости на baud; вернуться после полной
     * передачи. false - команду отправить нельзя (модель не умеет, ошибка TX).
     */
    bool (*send_baud_cmd)(uint32_t baud, void *ctx);
    /* Смена состояния; baud - скорость UART в этом состоянии. Может быть NULL. */
    void (*on_state)(gnss_link_state_t state, uint32_t baud, void *ctx);
    void *ctx;
} gnss_link_ops_t;

typedef struct
{
    uint32_t default_baud;   /* заводская скорость модуля */
    uint32_t target_baud;    /* рабочая скорость (может совпадать с default_baud) */
    uint32_t probe_ms;       /* сколько слушать на одной скорости (>= 1100 при 1 Гц) */
    uint32_t settle_ms;      /* пауза после команды смены скорости */
    uint8_t attempts;        /* попыток смены скорости и циклов поиска (>= 1) */
} gnss_link_cfg_t;

typedef struct
{
    gnss_link_cfg_t cfg;
    gnss_link_ops_t ops;
    gnss_link_state_t state;
    uint32_t baud;           /* текущая скорость UART */
    uint32_t t0;             /* начало текущего ожидания, мс */
    uint8_t probed;          /* скорости, прослушанные в этом цикле (1 - заводская, 2 - рабочая) */
    uint8_t cycles;          /* циклов поиска без ответа */
    uint8_t switch_tries;    /* отправленных команд смены скорости */
    gnss_diag_t diag;        /* разбор NMEA, пока идёт поиск */
} gnss_link_t;

/** @brief Подготовить структуру (состояние IDLE). */
void gnss_link_init(gnss_link_t *l, const gnss_link_cfg_t *cfg, const gnss_link_ops_t *ops);

/**
 * @brief Начать поиск: UART на заводскую скорость, слушать. Вызывать после
 *        включения модуля (EN в High) и запуска приёма.
 */
void gnss_link_start(gnss_link_t *l, uint32_t now_ms);

/** @brief Остановить (GNSS выключен): состояние IDLE, байты игнорируются. */
void gnss_link_stop(gnss_link_t *l);

/** @brief Учесть принятый байт (только PROBE и VERIFY, иначе сразу возврат). */
void gnss_link_feed(gnss_link_t *l, uint8_t byte);

/** @brief Продвинуть автомат (таймауты, переходы). Вызывать из основного цикла. */
void gnss_link_poll(gnss_link_t *l, uint32_t now_ms);

gnss_link_state_t gnss_link_state(const gnss_link_t *l);

/** @brief Текущая скорость UART. */
uint32_t gnss_link_baud(const gnss_link_t *l);

/** @brief true, пока идёт поиск или смена скорости (PROBE, SWITCH, VERIFY). */
bool gnss_link_busy(const gnss_link_t *l);

/** @brief Имя состояния для лога. */
const char *gnss_link_state_name(gnss_link_state_t state);

#endif /* PURRGO_GNSS_LINK_H */
