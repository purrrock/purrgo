#ifndef PURRGO_UTC_HINT_PRIV_H
#define PURRGO_UTC_HINT_PRIV_H

/* Внутренний обмен между utc_clock.c (состояние) и utc_hint.c (отправка подсказки приёмнику). */

#include <stdbool.h>
#include <stdint.h>

/*
 * Время, полученное от приёмника в этом сеансе, продолженное до now_ms; since_s_out - сколько секунд назад
 * оно получено. false - в сеансе времени не было.
 */
bool purrgo_utc_hint_priv_session_time(uint32_t now_ms, uint32_t *epoch_out, uint32_t *since_s_out);

/* Последняя действительная позиция приёмника и сколько секунд назад она была. */
bool purrgo_utc_hint_priv_position(uint32_t now_ms, int32_t *lat, int32_t *lon, int32_t *alt_cm,
                                   uint32_t *elapsed_s_out);

void purrgo_utc_hint_priv_touch(uint32_t now_ms);

/* Сброс состояния часов (purrgo_utc_hint_reset() в utc_hint.c сбрасывает и его, и отправку). */
void purrgo_utc_hint_clock_reset(void);

#endif
