#ifndef PURRGO_GNSS_ADAPTER_H
#define PURRGO_GNSS_ADAPTER_H

#include "purrgo/gnss_types.h"

// Обновляет состояние решения на основе входящей NMEA-строки
void purrgo_gnss_process_nmea(const char *nmea_line, purrgo_gnss_solution_t *solution);

// Получает указатель на текущее авторитетное GNSS-решение (управляется адаптером)
const purrgo_gnss_solution_t *purrgo_gnss_get_solution(void);

#endif // PURRGO_GNSS_ADAPTER_H
