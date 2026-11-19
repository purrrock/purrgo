#ifndef PURRGO_SOUND_H
#define PURRGO_SOUND_H

/*
 * Звуковые сигналы (пьезоизлучатель). Ядро только просит сигнал по смыслу события, а
 * как и чем он звучит, решает платформа (STM32: stm32/port/sound/sound_stm32.c, два
 * комплементарных канала TIM16; PC: строка в лог).
 *
 *   PURRGO_SOUND_ERROR   - длинный громкий: запись остановлена из-за низкого питания
 *                          (supply.c) или запись трека оборвалась (сбой SD);
 *   PURRGO_SOUND_ARRIVED - двойной громкий: прибытие к цели навигации (waypoint.c);
 *   PURRGO_SOUND_INFO    - короткий: прочие нужды (первый фикс GNSS после включения);
 *   PURRGO_SOUND_CLICK   - тихий щелчок на нажатие кнопки (app_fsm.c).
 *
 * Звук включается в меню (пункт SOUND, ключ SOUND в PURRGO.CFG), по умолчанию выключен:
 * пока выключен, purrgo_sound_play() ничего не делает, в том числе для ERROR.
 *
 * Сигналы неблокирующие: purrgo_sound_play() возвращается сразу, сигнал доигрывает платформа.
 */

#include <stdbool.h>

typedef enum {
    PURRGO_SOUND_NONE = 0,
    PURRGO_SOUND_CLICK,     /* тихий щелчок */
    PURRGO_SOUND_INFO,      /* короткий */
    PURRGO_SOUND_ARRIVED,   /* двойной громкий */
    PURRGO_SOUND_ERROR,     /* длинный громкий */
    PURRGO_SOUND_COUNT
} purrgo_sound_t;

/*
 * Сыграть сигнал, если звук включён в настройках (app_config.sound_on). Порядок
 * значений в purrgo_sound_t - приоритет: более важный сигнал прерывает менее важный,
 * менее важный во время более важного отбрасывается.
 */
void purrgo_sound_play(purrgo_sound_t sound);

/* Название сигнала для лога ("ERROR", "ARRIVED", ...). */
const char* purrgo_sound_name(purrgo_sound_t sound);

/*
 * Платформенный хук: начать сигнал и вернуться сразу. Политику (включён ли звук) и
 * приоритеты проверяет вызывающий purrgo_sound_play()/платформа; хук вызывается только
 * при включённом звуке.
 */
void purrgo_platform_sound_play(purrgo_sound_t sound);

#endif /* PURRGO_SOUND_H */
