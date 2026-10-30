#include "purrgo/battery.h"

/*
 * ЗАГЛУШКА мониторинга батареи (см. purrgo/battery.h).
 * Значение для проверки отображения в строке состояния и записи в трек;
 * можно переопределить при сборке: -DPURRGO_BATTERY_STUB_PERCENT=55,
 * -1 - показывать "неизвестно".
 */
#ifndef PURRGO_BATTERY_STUB_PERCENT
#define PURRGO_BATTERY_STUB_PERCENT 100
#endif

#ifndef PURRGO_BATTERY_STUB_MV
#define PURRGO_BATTERY_STUB_MV 0   /* 0 - напряжение не измеряется */
#endif

bool purrgo_battery_read(purrgo_battery_t* out) {
    if (!out) {
        return false;
    }
    out->percent = (int8_t)PURRGO_BATTERY_STUB_PERCENT;
    out->millivolts = (uint16_t)PURRGO_BATTERY_STUB_MV;
    return out->percent >= 0;
}
