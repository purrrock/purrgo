#include "nav/logger_retry.h"

/* Планирует следующую попытку старта/восстановления логгера: 5 с, 30 с, затем 5 мин. */
static void schedule_retry(purrgo_logger_retry_t *r, uint32_t now_ms) {
    if (r->start_failures < 255U) {
        r->start_failures++;
    }

    uint32_t delay_ms;
    if (r->start_failures == 1U) {
        delay_ms = PURRGO_LOGGER_RETRY_DELAY_1_MS;
    } else if (r->start_failures == 2U) {
        delay_ms = PURRGO_LOGGER_RETRY_DELAY_2_MS;
    } else {
        delay_ms = PURRGO_LOGGER_RETRY_DELAY_MAX_MS;
    }
    r->next_retry_ms = now_ms + delay_ms;
}

void purrgo_logger_retry_reset(purrgo_logger_retry_t *r) {
    r->next_retry_ms = 0;
    r->start_failures = 0;
    r->recording_since_ms = 0;
    /* prev_state сохраняется, см. комментарий в заголовке. */
}

void purrgo_logger_retry_observe(purrgo_logger_retry_t *r, uint32_t now_ms,
                                 track_logger_state_t state) {
    /* Сбой записи во время сессии (RECORDING -> ERROR): пауза с нарастанием. */
    if (state == LOGGER_STATE_ERROR && r->prev_state == LOGGER_STATE_RECORDING) {
        schedule_retry(r, now_ms);
    }

    if (state == LOGGER_STATE_RECORDING) {
        if (r->prev_state != LOGGER_STATE_RECORDING) {
            r->recording_since_ms = now_ms;
        } else if (r->start_failures != 0 &&
                   (uint32_t)(now_ms - r->recording_since_ms) >= PURRGO_LOGGER_STABLE_MS) {
            r->start_failures = 0;
        }
    }
    r->prev_state = state;
}

bool purrgo_logger_retry_start_allowed(const purrgo_logger_retry_t *r, uint32_t now_ms) {
    return r->start_failures == 0 || (int32_t)(now_ms - r->next_retry_ms) >= 0;
}

void purrgo_logger_retry_on_start_ok(purrgo_logger_retry_t *r, uint32_t now_ms,
                                     track_logger_state_t state) {
    r->prev_state = state;
    r->recording_since_ms = now_ms;
}

void purrgo_logger_retry_on_start_failed(purrgo_logger_retry_t *r, uint32_t now_ms) {
    schedule_retry(r, now_ms);
}

void purrgo_logger_retry_sync(purrgo_logger_retry_t *r, track_logger_state_t state) {
    r->prev_state = state;
}
