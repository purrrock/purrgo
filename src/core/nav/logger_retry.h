#ifndef PURRGO_LOGGER_RETRY_H
#define PURRGO_LOGGER_RETRY_H

#include <stdbool.h>
#include <stdint.h>

#include "purrgo/track_logger.h"

/*
 * Политика повторных попыток старта/восстановления трек-логгера (карта SD
 * вынута, ФС не смонтирована, сбой записи). Чистая логика без обращений к
 * логгеру и часам: состояние логгера и текущее время передаёт вызывающий,
 * поэтому модуль проверяется host-тестами (tests/host/test_logger_retry.c).
 * Пользуется app_fsm.c (track_logger_service).
 *
 * Паузы между попытками растут: 5 с, 30 с, затем 5 мин. Счётчик неудач
 * сбрасывается, только когда запись непрерывно идёт PURRGO_LOGGER_STABLE_MS:
 * иначе "переоткрылся и сразу упал" давал бы повторные попытки каждую секунду.
 *
 * Порядок вызовов в каждом такте обслуживания логгера:
 *   1. purrgo_logger_retry_observe()      - всегда, с текущим состоянием логгера;
 *   2. purrgo_logger_retry_start_allowed() - если логгер IDLE/ERROR и нужен старт;
 *   3. purrgo_logger_retry_on_start_ok() / _on_start_failed() - по итогу старта.
 * При блокировке записи по питанию вместо этого - purrgo_logger_retry_sync().
 */

/* Запись должна идти без сбоев столько времени, чтобы сбросить счётчик неудач. */
#define PURRGO_LOGGER_STABLE_MS 60000U

/* Паузы перед 1-й, 2-й и последующими повторными попытками. */
#define PURRGO_LOGGER_RETRY_DELAY_1_MS    5000U
#define PURRGO_LOGGER_RETRY_DELAY_2_MS   30000U
#define PURRGO_LOGGER_RETRY_DELAY_MAX_MS 300000U

typedef struct {
    uint32_t next_retry_ms;        /* не раньше этого момента (сравнение со знаком, переполнение безопасно) */
    uint8_t start_failures;        /* подряд неудачных стартов/сбоев; 0 - пауз нет */
    track_logger_state_t prev_state;   /* состояние логгера в прошлом такте */
    uint32_t recording_since_ms;   /* когда запись последний раз перешла в RECORDING */
} purrgo_logger_retry_t;

/*
 * Начало работы приложения: пауз и неудач нет. prev_state НЕ трогается намеренно:
 * он отражает фактическое состояние логгера, которое переживает повторную
 * инициализацию приложения, а не состояние самого приложения.
 */
void purrgo_logger_retry_reset(purrgo_logger_retry_t *r);

/*
 * Наблюдение за состоянием логгера в очередном такте: сбой записи во время сессии
 * (RECORDING -> ERROR) планирует повторную попытку; достаточно долгая запись без
 * сбоев сбрасывает счётчик неудач. Запоминает state как prev_state.
 */
void purrgo_logger_retry_observe(purrgo_logger_retry_t *r, uint32_t now_ms,
                                 track_logger_state_t state);

/* true, если старт можно пробовать сейчас: неудач не было или пауза истекла. */
bool purrgo_logger_retry_start_allowed(const purrgo_logger_retry_t *r, uint32_t now_ms);

/* Старт удался: state - состояние логгера после старта. */
void purrgo_logger_retry_on_start_ok(purrgo_logger_retry_t *r, uint32_t now_ms,
                                     track_logger_state_t state);

/* Старт не удался: планирует следующую попытку. */
void purrgo_logger_retry_on_start_failed(purrgo_logger_retry_t *r, uint32_t now_ms);

/*
 * Запись заблокирована снаружи (низкое питание): повторных попыток не планируем,
 * счётчики не меняем, только запоминаем состояние логгера, чтобы после снятия
 * блокировки не принять смену состояния за свежий сбой.
 */
void purrgo_logger_retry_sync(purrgo_logger_retry_t *r, track_logger_state_t state);

#endif /* PURRGO_LOGGER_RETRY_H */
