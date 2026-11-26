#ifndef PURRGO_UTC_HINT_H
#define PURRGO_UTC_HINT_H

/*
 * Последнее известное время UTC (память о времени между включениями) и подсказка приёмнику.
 *
 * Устройство не знает времени, пока приёмник его не сообщит (RTC МК не используется). Модуль хранит:
 *   - "пол" (floor) - время, сохранённое в PURRGO.CFG (ключ LAST_UTC) при выключении и при каждой
 *     записи конфигурации; реальное время при следующем включении не может быть меньше;
 *   - время текущего сеанса - последнее время приёмника и момент его получения (purrgo_system_time_ms).
 * Из них считается оценка текущего времени (purrgo_utc_hint_estimate()). Это НЕ показание часов:
 * UI показывает только время, подтверждённое приёмником (solution.time_valid). Оценка нужна для:
 *   1) меток времени файлов FatFs (stm32/port/storage/fattime.c), пока приёмник времени не знает;
 *   2) проверки времени приёмника: время раньше "пола" (минус допуск) считается недостоверным
 *      (например, приёмник в заводском состоянии с ошибочной неделей GPS) - purrgo_utc_hint_filter();
 *   3) подсказки приёмнику UBX-MGA-INI-TIME_UTC и UBX-MGA-INI-POS_LLH, если после включения
 *      он не знает времени - purrgo_utc_hint_service().
 *
 * Время хранится в секундах от 01.01.2000 UTC (эпоха purrgo_time.h).
 */

#include <stdbool.h>
#include <stdint.h>

#include "purrgo/gnss_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Допуск проверки времени приёмника относительно "пола", с. */
#ifndef PURRGO_UTC_FLOOR_TOLERANCE_S
#define PURRGO_UTC_FLOOR_TOLERANCE_S 300U
#endif

/* Подсказка приёмнику вообще (0 - не отправлять). */
#ifndef PURRGO_UTC_HINT_ENABLE
#define PURRGO_UTC_HINT_ENABLE 1
#endif

/* Сколько приёмник должен непрерывно присылать PVT без времени, прежде чем отправить подсказку, мс. */
#ifndef PURRGO_UTC_HINT_GRACE_MS
#define PURRGO_UTC_HINT_GRACE_MS 4000U
#endif

/* Молчание приёмника дольше этого срока (перезапуск, питание) разрешает новую подсказку, мс. */
#ifndef PURRGO_UTC_HINT_SILENCE_RESET_MS
#define PURRGO_UTC_HINT_SILENCE_RESET_MS 10000U
#endif

/*
 * Подсказка по данным из PURRGO.CFG, когда в этом сеансе приёмник времени ещё не давал (холодное
 * включение). Сколько прошло с выключения, неизвестно, поэтому точность заявляется заведомо грубой:
 * время - максимум поля tAccS (65535 с, около 18 ч), позиция - PURRGO_UTC_HINT_BOOT_POS_ACC_M.
 * Если устройство было выключено дольше или перевезено дальше, подсказка окажется неточнее заявленного,
 * что по описанию u-blox может ухудшить поиск: тогда выставьте 0.
 */
#ifndef PURRGO_UTC_HINT_BOOT_TIME
#define PURRGO_UTC_HINT_BOOT_TIME 1
#endif
#ifndef PURRGO_UTC_HINT_BOOT_TACC_S
#define PURRGO_UTC_HINT_BOOT_TACC_S 65535U
#endif
#ifndef PURRGO_UTC_HINT_BOOT_POS
#define PURRGO_UTC_HINT_BOOT_POS 1
#endif
#ifndef PURRGO_UTC_HINT_BOOT_POS_ACC_M
#define PURRGO_UTC_HINT_BOOT_POS_ACC_M 500000U
#endif

/* Сброс состояния (тесты, purrgo_config_init() сбрасывает только "пол"). */
void purrgo_utc_hint_reset(void);

/* "Пол" из PURRGO.CFG (0 - неизвестен). Вызывает конфигурация при загрузке. */
void purrgo_utc_hint_set_floor(uint32_t epoch);
uint32_t purrgo_utc_hint_floor(void);

/*
 * Оценка текущего времени: время сеанса плюс прошедшее с его получения, иначе "пол". Это нижняя граница
 * (счёт идёт по целым секундам, момент отсчёта - последний вызов filter/service, не позже 1 с назад).
 * false - ничего не известно. Результат монотонен: не меньше "пола".
 */
bool purrgo_utc_hint_estimate(uint32_t *epoch_out);
bool purrgo_utc_hint_estimate_at(uint32_t now_ms, uint32_t *epoch_out);

/*
 * Вызывается для каждого принятого решения приёмника (gnss_feed): запоминает время и позицию, а время,
 * раньше известного (с допуском), объявляет недостоверным (time_valid = false, предупреждение в лог).
 */
void purrgo_utc_hint_filter(purrgo_gnss_solution_t *sol, uint32_t now_ms);

/*
 * Раз в секунду из главного цикла: если приёмник UBX работает (PVT приходит), но времени не знает
 * (validTime = 0) дольше PURRGO_UTC_HINT_GRACE_MS, отправляет ему UBX-MGA-INI-TIME_UTC и POS_LLH
 * (один раз за такой период). Без UBX или без известного времени - ничего не делает.
 */
void purrgo_utc_hint_service(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* PURRGO_UTC_HINT_H */
