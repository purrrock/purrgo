#ifndef PURRGO_HW_FAULT_H
#define PURRGO_HW_FAULT_H

/*
 * Аппаратные сбои: SD-карта, GNSS-модуль, E-Ink, АЦП батареи, системная периферия.
 *
 * purrgo_hw_fault() делает две вещи:
 *   1. пишет в лог (UART, а при SD LOG и в файл) строку "HW FAULT [<узел>]: <описание>" с
 *      приоритетом ERROR (PURRGO_LOGE), поэтому строка попадает в лог и в релизной сборке;
 *   2. один раз за "эпизод" сбоя узла просит звук PURRGO_SOUND_ERROR (purrgo/sound.h).
 *
 * Эпизод узла начинается с первого сообщения и кончается вызовом purrgo_hw_fault_clear(): узел
 * снова заработал (карта смонтирована, пошли данные GNSS, кадр ушёл на панель, АЦП измерил).
 * Пока эпизод идёт, повторные сообщения того же узла пишутся в лог, но не звучат: SD, которая
 * не монтируется, повторяет попытку раз в несколько секунд, а прибор не должен пищать каждый раз.
 * Сбой другого узла начинает свой эпизод и звучит отдельно.
 *
 * Звук играется не из purrgo_hw_fault(), а из purrgo_hw_fault_service() в главном цикле
 * (purrgo_app_tick()): сообщение может прийти из прерывания (ошибки приёма GNSS), а запуск
 * ШИМ излучателя оттуда небезопасен. Сообщения о сбоях при звуке OFF молчат, как и всё остальное.
 * Если запись остановлена по питанию (supply.c), сигнал разряда батареи уже прозвучал: сбои
 * узлов, вызванные просадкой питания, звука не дают (в лог пишутся).
 *
 * Функции безопасны для вызова из прерывания (кроме purrgo_hw_fault_service()).
 */

#include <stdbool.h>
#include "purrgo/purrgo_format.h"

typedef enum {
    PURRGO_HWF_SD = 0,    /* SD-карта: сбой обмена, не монтируется, SPI */
    PURRGO_HWF_GNSS,      /* GNSS-модуль: нет связи, нет данных, ошибки UART/DMA */
    PURRGO_HWF_DISPLAY,   /* E-Ink: сбой операции панели, таймаут BUSY */
    PURRGO_HWF_BATTERY,   /* измерение батареи: АЦП, опорное напряжение */
    PURRGO_HWF_SYSTEM,    /* прочая периферия: таймбейз LPTIM1, детектор питания PVD */
    PURRGO_HWF_COUNT
} purrgo_hw_t;

/* Имя узла для лога ("SD", "GNSS", "DISPLAY", "BATTERY", "SYSTEM"). */
const char* purrgo_hw_name(purrgo_hw_t hw);

/* Сообщить о сбое узла: строка в лог (ERROR) и, если эпизод новый, один сигнал. printf-формат. */
void purrgo_hw_fault(purrgo_hw_t hw, const char* format, ...) PURRGO_PRINTF_FORMAT(2, 3);

/* Узел снова исправен: эпизод закончен, следующий сбой снова прозвучит. Без сбоя - ничего не делает. */
void purrgo_hw_fault_clear(purrgo_hw_t hw);

/* Идёт ли эпизод сбоя узла (сообщение было, purrgo_hw_fault_clear() после него не вызывали). */
bool purrgo_hw_fault_active(purrgo_hw_t hw);

/* Главный цикл: сыграть накопленный сигнал (один на все новые эпизоды). */
void purrgo_hw_fault_service(void);

/* Служебное (для purrgo_hw_fault_service()): были ли новые эпизоды с прошлого вызова; сбрасывает признак. */
bool purrgo_hw_fault_take_pending(void);

/* Сбросить состояние всех узлов (при старте и в тестах). */
void purrgo_hw_fault_reset(void);

#endif /* PURRGO_HW_FAULT_H */
