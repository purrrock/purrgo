#ifndef PURRGO_GNSS_PPS_H
#define PURRGO_GNSS_PPS_H

/*
 * Статистика импульсов 1PPS приёмника GNSS: число фронтов и интервалы между
 * ними. Нужна для проверки (TODO, P0 "Проверить 1PPS"): частота 1 Гц, импульсы
 * только при фиксе, число фронтов совпадает с числом RMC со статусом A.
 *
 * Модуль не зависит от HAL и ядра PurrGO, проверяется host-тестом
 * (tests/host/test_gnss_pps.c). Время передаёт вызывающий код (мс, HAL_GetTick()).
 *
 * Потоки: gnss_pps_edge() вызывается из прерывания EXTI, gnss_pps_snapshot() -
 * из основного цикла. Согласованность снимка обеспечивает счётчик seq
 * (seqlock): запись из прерывания не блокируется, чтение повторяется, если
 * прерывание изменило данные во время копирования.
 */

#include <stdbool.h>
#include <stdint.h>

/* Интервал между фронтами считается нормальным в пределах 1000 +- этого значения, мс. */
#define GNSS_PPS_TOLERANCE_MS 50U

/* Интервал больше этого значения означает пропуск импульсов (около 1,5 с), а не дрожание. */
#define GNSS_PPS_GAP_MS 1500U

typedef struct
{
    uint32_t edges;          /* всего фронтов с момента gnss_pps_init() */
    uint32_t intervals;      /* измеренных интервалов (edges - 1, если фронты были) */
    uint32_t ok;             /* интервалов в пределах допуска */
    uint32_t too_short;      /* короче 1000 - допуск (дребезг, ложные фронты) */
    uint32_t gaps;           /* интервалов >= GNSS_PPS_GAP_MS (были пропущены импульсы) */
    uint32_t missed;         /* оценка числа пропущенных импульсов по таким интервалам */
    uint32_t drift;          /* остальные интервалы вне допуска (дрожание или неточная секунда) */
    uint32_t min_ms;         /* наименьший измеренный интервал (0, если интервалов нет) */
    uint32_t max_ms;         /* наибольший измеренный интервал */
    uint32_t last_edge_ms;   /* время последнего фронта */
} gnss_pps_stats_t;

typedef struct
{
    volatile uint32_t seq;   /* нечётное - идёт запись */
    gnss_pps_stats_t s;
} gnss_pps_t;

void gnss_pps_init(gnss_pps_t *p);

/** @brief Учесть фронт 1PPS (можно из прерывания). */
void gnss_pps_edge(gnss_pps_t *p, uint32_t now_ms);

/** @brief Согласованная копия статистики. */
void gnss_pps_snapshot(const gnss_pps_t *p, gnss_pps_stats_t *out);

/**
 * @brief Были ли фронты в последние max_age_ms (для "импульсы только при фиксе").
 * false, если фронтов не было вовсе.
 */
bool gnss_pps_active(const gnss_pps_stats_t *s, uint32_t now_ms, uint32_t max_age_ms);

#endif /* PURRGO_GNSS_PPS_H */
