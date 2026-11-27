#ifndef PURRGO_GNSS_DUTY_H
#define PURRGO_GNSS_DUTY_H

/*
 * Цикл питания GNSS-приёмника в трекере (PURRGO_GNSS_TRACKER_MODE_EN).
 *
 * Конечный автомат без привязки к железу: он решает, когда включить приёмник (EN = High), когда принять фикс и
 * выключить (EN = Low), как долго ждать и как растят паузы при отсутствии сигнала. Действия выполняют функции из
 * ops (на плате это purrgo_gnss_power_on/off), время и состояние приёмника приходят параметрами, поэтому логика
 * проверяется host-тестом tests/host/test_gnss_duty.c. Основа решений - измерения теста GNSS_BBR
 * (docs/gnss-retention.md): EN = Low не стирает BBR, поэтому старт после включения горячий.
 *
 * Состояния:
 *   OFF     приёмник выключен, ждём срока следующего включения;
 *   SEARCH  включён, ждём фикс (таймаут: горячий PURRGO_GNSS_DUTY_FIX_TIMEOUT_S, холодный - _COLD_TIMEOUT_S);
 *   HOLD    фикс получен, ждём точность не хуже PURRGO_GNSS_DUTY_ACCEPT_HACC_M, но не дольше _HOLD_MS;
 *   CONT    приёмник не выключается (PSMCT): включается, если фикс несколько раз подряд приходил медленно
 *           (при долгом поиске дешевле не выключать, ток поиска ~13 мА против ~7 мА в PSMCT).
 *
 * Паузы. После принятого фикса период между началами включений PURRGO_GNSS_TRACKER_PERIOD_S (пауза = период минус
 * время работы, не меньше _MIN_PAUSE_S). После промаха (таймаут без фикса) пауза равна периоду, после второго
 * промаха подряд удваивается и так далее до _BACKOFF_MAX_S. Первый принятый фикс возвращает период. Таймаут при
 * промахах НЕ растёт: в горячем состоянии долгое ожидание означает плохое небо и только тратит ток. Исключение -
 * холодный старт (нет известного фикса, фикс старше _COLD_AGE_S или приёмник сообщил, что времени не знает):
 * один раз разрешается _COLD_TIMEOUT_S.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Параметры политики (мс). purrgo_gnss_duty_cfg_default() берёт их из PURRGO_GNSS_DUTY_* и PURRGO_GNSS_TRACKER_*. */
typedef struct {
    uint32_t period_ms;          /* период между началами включений */
    uint32_t fix_timeout_ms;     /* ожидание фикса при горячем старте */
    uint32_t cold_timeout_ms;    /* то же при холодном старте */
    uint32_t cold_age_ms;        /* возраст фикса, после которого старт считается холодным */
    uint32_t hold_ms;            /* ожидание хорошей точности после первого фикса (0 - принять сразу) */
    uint32_t accept_hacc_mm;     /* достаточная точность hAcc, мм */
    uint32_t min_pause_ms;       /* наименьшая пауза с выключенным приёмником */
    uint32_t backoff_max_ms;     /* предел паузы после промахов */
    uint32_t slow_fix_ms;        /* фикс дольше этого времени - медленный */
    uint32_t cont_retry_ms;      /* через столько непрерывной работы снова пробовать цикл */
    uint32_t cont_loss_ms;       /* столько без фикса - непрерывный режим кончается */
    uint8_t  min_fix_type;       /* 2 - 2D, 3 - 3D */
    uint8_t  slow_count;         /* медленных фиксов подряд до перехода в CONT; 0 - CONT выключен */
} purrgo_gnss_duty_cfg_t;

/* Действия над приёмником (ctx - пользовательский указатель). */
typedef struct {
    void (*power_on)(void *ctx);           /* EN = High, профиль PURRGO_GNSS_PROFILE_TRACKER_EN (если включён - перезапуск) */
    void (*power_off)(void *ctx);          /* EN = Low */
    void (*enter_continuous)(void *ctx);   /* приёмник остаётся включённым: профиль PURRGO_GNSS_PROFILE_TRACKER_CT */
} purrgo_gnss_duty_ops_t;

/* Что известно о решении приёмника на момент вызова (заполняет вызывающий из purrgo_gnss_solution_t). */
typedef struct {
    uint32_t fix_seq;    /* растёт на каждое новое решение (PVT или RMC) */
    bool valid;          /* решение с фиксом (gnssFixOK) */
    bool time_valid;     /* приёмник знает дату и время */
    uint8_t fix_type;    /* 2 - 2D, 3 - 3D, 1 - нет фикса, 0 - тип не передан (NMEA без GSA) */
    bool acc_valid;      /* hacc_mm достоверна */
    uint32_t hacc_mm;
} purrgo_gnss_duty_input_t;

typedef enum {
    PURRGO_GNSS_DUTY_STOPPED = 0,
    PURRGO_GNSS_DUTY_OFF,
    PURRGO_GNSS_DUTY_SEARCH,
    PURRGO_GNSS_DUTY_HOLD,
    PURRGO_GNSS_DUTY_CONT
} purrgo_gnss_duty_state_t;

typedef struct {
    purrgo_gnss_duty_cfg_t cfg;
    purrgo_gnss_duty_ops_t ops;
    void *ctx;

    purrgo_gnss_duty_state_t state;
    uint32_t t_on;               /* момент включения (SEARCH, HOLD) */
    uint32_t t_fix;              /* момент первого фикса (HOLD) */
    uint32_t t_next_start;       /* срок следующего включения (OFF) */
    uint32_t t_cont_start;       /* начало CONT */
    uint32_t t_cont_fix;         /* последний фикс в CONT */
    uint32_t seq_seen;           /* fix_seq, с которым сравниваем */
    uint32_t last_pause_ms;      /* последняя назначенная пауза (для срока действия фикса) */
    uint32_t last_fix_ms;        /* момент последнего принятого фикса */
    uint32_t last_ttf_ms;        /* время от включения до последнего принятого фикса */
    uint32_t cycles;             /* число включений */
    uint32_t misses_total;       /* число промахов */
    uint16_t misses;             /* промахов подряд */
    uint8_t  slow_run;           /* медленных фиксов подряд */
    bool have_fix;               /* когда-либо принимали фикс (или он был при входе) */
    bool long_timeout;           /* текущее включение - холодный старт */
    bool time_seen;              /* в этом включении уже была эпоха с известным временем */
} purrgo_gnss_duty_t;

/* Параметры по умолчанию из hardware_config.h. */
void purrgo_gnss_duty_cfg_default(purrgo_gnss_duty_cfg_t *cfg);

void purrgo_gnss_duty_init(purrgo_gnss_duty_t *d, const purrgo_gnss_duty_cfg_t *cfg,
                           const purrgo_gnss_duty_ops_t *ops, void *ctx);

/*
 * Начало работы (вход в трекер): сразу включает приёмник (power_on; если он уже включён с профилем навигации - это
 * перезапуск на 38400) и начинает поиск. recent_fix - в момент входа действующий фикс был (старт горячий).
 */
void purrgo_gnss_duty_start(purrgo_gnss_duty_t *d, uint32_t now_ms, const purrgo_gnss_duty_input_t *in,
                            bool recent_fix);

/* Конец работы: автомат останавливается, приёмник не трогается (платформа сама вернёт профиль навигации). */
void purrgo_gnss_duty_stop(purrgo_gnss_duty_t *d);

/* Раз в такт главного цикла. in - текущее решение приёмника. */
void purrgo_gnss_duty_tick(purrgo_gnss_duty_t *d, uint32_t now_ms, const purrgo_gnss_duty_input_t *in);

/* Через сколько мс автомату нужно действовать (0 - сейчас; UINT32_MAX - ничего не ждёт). Ограничивает сон МК. */
uint32_t purrgo_gnss_duty_wait_ms(const purrgo_gnss_duty_t *d, uint32_t now_ms);

/*
 * Срок, после которого молчание приёмника считается потерей фикса (purrgo_gnss_feed_check_timeout): пауза плюс
 * ожидание фикса плюс запас.
 */
uint32_t purrgo_gnss_duty_fix_timeout_ms(const purrgo_gnss_duty_t *d);

/*
 * Трекер платформы (один экземпляр, ops - purrgo_gnss_power_on/off и профили). Вне PURRGO_GNSS_TRACKER_MODE_EN
 * функции ничего не делают.
 */
void purrgo_gnss_tracker_start(uint32_t now_ms, const purrgo_gnss_duty_input_t *in, bool recent_fix);
void purrgo_gnss_tracker_stop(void);                    /* выход: приёмник перезапускается с профилем навигации */
void purrgo_gnss_tracker_tick(uint32_t now_ms, const purrgo_gnss_duty_input_t *in);
uint32_t purrgo_gnss_tracker_wait_ms(uint32_t now_ms);
uint32_t purrgo_gnss_tracker_fix_timeout_ms(void);
bool purrgo_gnss_tracker_active(void);

#ifdef __cplusplus
}
#endif

#endif /* PURRGO_GNSS_DUTY_H */
