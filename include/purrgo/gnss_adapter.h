#ifndef PURRGO_GNSS_ADAPTER_H
#define PURRGO_GNSS_ADAPTER_H

#include "purrgo/gnss_types.h"

// Обновляет состояние решения на основе входящей NMEA-строки
void purrgo_gnss_process_nmea(const char *nmea_line, purrgo_gnss_solution_t *solution);

// Задает глобальный указатель на текущее GNSS-решение для доступа из других модулей (например, FatFs)
void purrgo_gnss_set_active_solution(const purrgo_gnss_solution_t *solution);

// Получает глобальный указатель на текущее GNSS-решение (может вернуть NULL)
const purrgo_gnss_solution_t* purrgo_gnss_get_active_solution(void);

#endif // PURRGO_GNSS_ADAPTER_H
