#ifndef PURRGO_SOUND_H
#define PURRGO_SOUND_H

/*
 * Звуковые сигналы (пьезоизлучатель). Ядро только просит сигнал по смыслу события, а
 * как и чем он звучит, решает платформа (STM32: stm32/port/sound/sound_stm32.c, два
 * комплементарных канала TIM16; PC: строка в лог).
 *
 *   PURRGO_SOUND_ERROR   - длинный громкий: критический разряд батарей, запись трека и
 *                          запись на SD остановлены (supply.c), либо сбой железа - SD, GNSS,
 *                          E-Ink, АЦП батареи (hw_fault.h, один раз за эпизод сбоя). Остановка
 *                          записи трека пользователем (переход в читалку, выключение) и
 *                          программный сбой записи сигнала не дают;
 *   PURRGO_SOUND_ARRIVED - двойной громкий: прибытие к цели навигации (waypoint.c);
 *   PURRGO_SOUND_INFO    - короткий: прочие нужды (первый фикс GNSS после включения,
 *                          проверочный сигнал при выборе громкости в меню);
 *   PURRGO_SOUND_CLICK   - тихий щелчок на нажатие кнопки (app_fsm.c).
 *
 * Громкость выбирается в меню (пункт SOUND: OFF / LOW / HIGH, ключ SOUND=0|1|2 в
 * PURRGO.CFG), по умолчанию OFF. При OFF прибор полностью беззвучный: purrgo_sound_play()
 * ничего не делает ни для одного сигнала, в том числе для ERROR и щелчка кнопок.
 *
 * Сигналы неблокирующие: purrgo_sound_play() возвращается сразу, сигнал доигрывает платформа
 * (исключение - щелчок CLICK: он длится миллисекунды и играется синхронно, чтобы не
 * зависнуть, если после нажатия сразу начнётся долгая блокирующая операция).
 * Выключение и любые долгие блокирующие места, где главный цикл не крутится, должны сначала
 * вызвать purrgo_sound_stop().
 */

#include <stdbool.h>

typedef enum {
    PURRGO_SOUND_NONE = 0,
    PURRGO_SOUND_CLICK,     /* тихий щелчок */
    PURRGO_SOUND_INFO,      /* короткий */
    PURRGO_SOUND_ARRIVED,   /* двойной громкий */
    PURRGO_SOUND_ERROR,     /* длинный громкий (критический разряд батарей) */
    PURRGO_SOUND_COUNT
} purrgo_sound_t;

/* Громкость звука (пункт SOUND меню). Значения хранятся в PURRGO.CFG, порядок менять нельзя. */
typedef enum {
    PURRGO_SOUND_LEVEL_OFF = 0,   /* полная тишина */
    PURRGO_SOUND_LEVEL_LOW,       /* тише */
    PURRGO_SOUND_LEVEL_HIGH,      /* громче */
    PURRGO_SOUND_LEVEL_COUNT
} purrgo_sound_level_t;

/*
 * Сыграть сигнал на громкости app_config.sound_level (OFF - ничего). Порядок значений в
 * purrgo_sound_t - приоритет: более важный сигнал прерывает менее важный, менее важный
 * во время более важного отбрасывается.
 */
void purrgo_sound_play(purrgo_sound_t sound);

/* Немедленно оборвать звучащий сигнал (выключение прибора, выбор OFF в меню). */
void purrgo_sound_stop(void);

/* Название сигнала для лога ("ERROR", "ARRIVED", ...). */
const char* purrgo_sound_name(purrgo_sound_t sound);

/* Название громкости для меню и лога ("OFF", "LOW", "HIGH"). */
const char* purrgo_sound_level_name(purrgo_sound_level_t level);

/*
 * Платформенные хуки. Политику (включён ли звук) проверяет ядро, но платформа тоже не должна
 * играть при level == PURRGO_SOUND_LEVEL_OFF.
 *
 * purrgo_platform_sound_play(): начать сигнал и вернуться сразу.
 * purrgo_platform_sound_stop(): оборвать звучащий сигнал, выводы излучателя в Low.
 */
void purrgo_platform_sound_play(purrgo_sound_t sound, purrgo_sound_level_t level);
void purrgo_platform_sound_stop(void);

#endif /* PURRGO_SOUND_H */
