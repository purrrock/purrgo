/*
 * Политика повторных попыток трек-логгера (src/core/nav/logger_retry.c):
 *   - паузы после неудач растут: 5 с, 30 с, затем 5 мин;
 *   - сбой записи во время сессии (RECORDING -> ERROR) планирует попытку, а ERROR/IDLE
 *     без предшествующей записи - нет;
 *   - счётчик неудач сбрасывается только после PURRGO_LOGGER_STABLE_MS непрерывной записи;
 *   - сравнение времени устойчиво к переполнению uint32_t;
 *   - reset не трогает prev_state, sync только запоминает состояние.
 * Модуль чистый: логгер и часы не нужны, состояние и время передаются параметрами.
 */
#include "nav/logger_retry.h"
#include "test_common.h"

static purrgo_logger_retry_t fresh(void)
{
    purrgo_logger_retry_t r = { .prev_state = LOGGER_STATE_IDLE };
    purrgo_logger_retry_reset(&r);
    return r;
}

static void test_initial_start_allowed(void)
{
    purrgo_logger_retry_t r = fresh();

    CHECK(purrgo_logger_retry_start_allowed(&r, 0U));
    CHECK(purrgo_logger_retry_start_allowed(&r, 123456U));
    CHECK(r.start_failures == 0U);
}

static void test_backoff_ladder(void)
{
    purrgo_logger_retry_t r = fresh();
    uint32_t t = 1000U;

    /* 1-я неудача: пауза 5 с. */
    purrgo_logger_retry_on_start_failed(&r, t);
    CHECK(r.start_failures == 1U);
    CHECK(!purrgo_logger_retry_start_allowed(&r, t));
    CHECK(!purrgo_logger_retry_start_allowed(&r, t + PURRGO_LOGGER_RETRY_DELAY_1_MS - 1U));
    CHECK(purrgo_logger_retry_start_allowed(&r, t + PURRGO_LOGGER_RETRY_DELAY_1_MS));

    /* 2-я: 30 с. */
    t += PURRGO_LOGGER_RETRY_DELAY_1_MS;
    purrgo_logger_retry_on_start_failed(&r, t);
    CHECK(r.start_failures == 2U);
    CHECK(!purrgo_logger_retry_start_allowed(&r, t + PURRGO_LOGGER_RETRY_DELAY_2_MS - 1U));
    CHECK(purrgo_logger_retry_start_allowed(&r, t + PURRGO_LOGGER_RETRY_DELAY_2_MS));

    /* 3-я и дальше: 5 мин, дальше не растёт. */
    for (int i = 0; i < 4; i++) {
        t += PURRGO_LOGGER_RETRY_DELAY_MAX_MS;
        purrgo_logger_retry_on_start_failed(&r, t);
        CHECK(!purrgo_logger_retry_start_allowed(&r, t + PURRGO_LOGGER_RETRY_DELAY_MAX_MS - 1U));
        CHECK(purrgo_logger_retry_start_allowed(&r, t + PURRGO_LOGGER_RETRY_DELAY_MAX_MS));
    }
    CHECK(r.start_failures == 6U);
}

static void test_failure_counter_saturates(void)
{
    purrgo_logger_retry_t r = fresh();

    for (int i = 0; i < 400; i++) {
        purrgo_logger_retry_on_start_failed(&r, 0U);
    }
    CHECK(r.start_failures == 255U);   /* не заворачивается в 0 (это сняло бы паузу) */
    CHECK(!purrgo_logger_retry_start_allowed(&r, PURRGO_LOGGER_RETRY_DELAY_MAX_MS - 1U));
}

static void test_write_failure_schedules_retry(void)
{
    purrgo_logger_retry_t r = fresh();

    purrgo_logger_retry_observe(&r, 10000U, LOGGER_STATE_RECORDING);
    CHECK(r.recording_since_ms == 10000U);
    CHECK(r.start_failures == 0U);

    purrgo_logger_retry_observe(&r, 11000U, LOGGER_STATE_ERROR);   /* RECORDING -> ERROR */
    CHECK(r.start_failures == 1U);
    CHECK(!purrgo_logger_retry_start_allowed(&r, 11000U + PURRGO_LOGGER_RETRY_DELAY_1_MS - 1U));
    CHECK(purrgo_logger_retry_start_allowed(&r, 11000U + PURRGO_LOGGER_RETRY_DELAY_1_MS));

    /* Логгер остаётся в ERROR: это не новый сбой, повторно паузу не планируем. */
    purrgo_logger_retry_observe(&r, 12000U, LOGGER_STATE_ERROR);
    purrgo_logger_retry_observe(&r, 13000U, LOGGER_STATE_ERROR);
    CHECK(r.start_failures == 1U);
}

static void test_error_without_recording_is_not_a_write_failure(void)
{
    purrgo_logger_retry_t r = fresh();

    /* IDLE -> ERROR (старт не удался внутри логгера) паузу здесь не планирует:
     * её планирует on_start_failed по результату purrgo_logger_start(). */
    purrgo_logger_retry_observe(&r, 1000U, LOGGER_STATE_ERROR);
    CHECK(r.start_failures == 0U);
    CHECK(r.prev_state == LOGGER_STATE_ERROR);
}

static void test_counter_reset_needs_stable_recording(void)
{
    purrgo_logger_retry_t r = fresh();
    uint32_t t = 100000U;

    purrgo_logger_retry_on_start_failed(&r, t);
    purrgo_logger_retry_on_start_failed(&r, t);
    CHECK(r.start_failures == 2U);

    /* Запись началась (в такте, где старт удался). */
    purrgo_logger_retry_on_start_ok(&r, t, LOGGER_STATE_RECORDING);
    CHECK(r.recording_since_ms == t);

    /* Почти минута записи - счётчик ещё держится. */
    purrgo_logger_retry_observe(&r, t + PURRGO_LOGGER_STABLE_MS - 1U, LOGGER_STATE_RECORDING);
    CHECK(r.start_failures == 2U);

    /* Ровно PURRGO_LOGGER_STABLE_MS - сброс. */
    purrgo_logger_retry_observe(&r, t + PURRGO_LOGGER_STABLE_MS, LOGGER_STATE_RECORDING);
    CHECK(r.start_failures == 0U);
    CHECK(purrgo_logger_retry_start_allowed(&r, t + PURRGO_LOGGER_STABLE_MS));
}

static void test_flapping_never_resets_counter(void)
{
    purrgo_logger_retry_t r = fresh();
    uint32_t t = 0U;

    /* "Переоткрылся и сразу упал": каждая сессия живёт 10 с (< 60 с). */
    for (int i = 0; i < 10; i++) {
        purrgo_logger_retry_observe(&r, t, LOGGER_STATE_RECORDING);
        t += 10000U;
        purrgo_logger_retry_observe(&r, t, LOGGER_STATE_ERROR);
        t += PURRGO_LOGGER_RETRY_DELAY_MAX_MS;   /* пауза любой длины истекла */
        purrgo_logger_retry_observe(&r, t, LOGGER_STATE_IDLE);
    }
    CHECK(r.start_failures == 10U);
    CHECK(r.recording_since_ms != 0U);
}

static void test_recording_restart_restamps_since(void)
{
    purrgo_logger_retry_t r = fresh();

    purrgo_logger_retry_observe(&r, 1000U, LOGGER_STATE_RECORDING);
    purrgo_logger_retry_observe(&r, 2000U, LOGGER_STATE_RECORDING);
    CHECK(r.recording_since_ms == 1000U);   /* непрерывная запись: метка не двигается */

    purrgo_logger_retry_observe(&r, 3000U, LOGGER_STATE_IDLE);
    purrgo_logger_retry_observe(&r, 4000U, LOGGER_STATE_RECORDING);
    CHECK(r.recording_since_ms == 4000U);   /* запись прервалась и началась заново */
}

static void test_time_wraparound(void)
{
    purrgo_logger_retry_t r = fresh();
    const uint32_t t = 0xFFFFFFFFU - 1000U;   /* за 1 с до переполнения uint32_t */

    purrgo_logger_retry_on_start_failed(&r, t);   /* next_retry = t + 5000 -> заворачивается */
    CHECK(r.next_retry_ms < t);
    CHECK(!purrgo_logger_retry_start_allowed(&r, t));
    CHECK(!purrgo_logger_retry_start_allowed(&r, 0U));                                       /* ещё идёт пауза */
    CHECK(!purrgo_logger_retry_start_allowed(&r, PURRGO_LOGGER_RETRY_DELAY_1_MS - 1002U));
    CHECK(purrgo_logger_retry_start_allowed(&r, PURRGO_LOGGER_RETRY_DELAY_1_MS - 1001U));

    /* Устойчивая запись тоже считается через переполнение. */
    purrgo_logger_retry_t s = fresh();
    s.start_failures = 3U;
    purrgo_logger_retry_observe(&s, t, LOGGER_STATE_RECORDING);
    purrgo_logger_retry_observe(&s, t + PURRGO_LOGGER_STABLE_MS - 1U, LOGGER_STATE_RECORDING);
    CHECK(s.start_failures == 3U);
    purrgo_logger_retry_observe(&s, t + PURRGO_LOGGER_STABLE_MS, LOGGER_STATE_RECORDING);
    CHECK(s.start_failures == 0U);
}

static void test_reset_keeps_prev_state(void)
{
    purrgo_logger_retry_t r = fresh();

    purrgo_logger_retry_observe(&r, 1000U, LOGGER_STATE_RECORDING);
    purrgo_logger_retry_on_start_failed(&r, 1000U);
    purrgo_logger_retry_reset(&r);

    CHECK(r.start_failures == 0U);
    CHECK(r.next_retry_ms == 0U);
    CHECK(r.recording_since_ms == 0U);
    CHECK(r.prev_state == LOGGER_STATE_RECORDING);   /* отражает логгер, а не приложение */
    CHECK(purrgo_logger_retry_start_allowed(&r, 0U));
}

static void test_sync_only_remembers_state(void)
{
    purrgo_logger_retry_t r = fresh();

    purrgo_logger_retry_observe(&r, 1000U, LOGGER_STATE_RECORDING);

    /* Питание просело, запись остановлена снаружи: логгер ушёл в IDLE. */
    purrgo_logger_retry_sync(&r, LOGGER_STATE_IDLE);
    CHECK(r.start_failures == 0U);
    CHECK(r.prev_state == LOGGER_STATE_IDLE);

    /* После снятия блокировки ERROR не воспринимается как сбой записи (RECORDING -> ERROR). */
    purrgo_logger_retry_observe(&r, 2000U, LOGGER_STATE_ERROR);
    CHECK(r.start_failures == 0U);
}

int main(void)
{
    test_initial_start_allowed();
    test_backoff_ladder();
    test_failure_counter_saturates();
    test_write_failure_schedules_retry();
    test_error_without_recording_is_not_a_write_failure();
    test_counter_reset_needs_stable_recording();
    test_flapping_never_resets_counter();
    test_recording_restart_restamps_since();
    test_time_wraparound();
    test_reset_keeps_prev_state();
    test_sync_only_remembers_state();
    return test_finish("test_logger_retry");
}
