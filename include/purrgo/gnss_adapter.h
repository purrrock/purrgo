#ifndef PURRGO_GNSS_ADAPTER_H
#define PURRGO_GNSS_ADAPTER_H

#include "purrgo/gnss_types.h"

// Обновляет состояние решения на основе входящей NMEA-строки.
// Возвращает true, если разобрана корректная (с верной контрольной суммой)
// фраза RMC - признак того, что приёмник жив и отдаёт навигационные данные
// (в том числе со статусом 'V'). Используется для таймаута фикса.
bool purrgo_gnss_process_nmea(const char *nmea_line, purrgo_gnss_solution_t *solution);

#endif // PURRGO_GNSS_ADAPTER_H
