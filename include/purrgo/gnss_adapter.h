#ifndef PURRGO_GNSS_ADAPTER_H
#define PURRGO_GNSS_ADAPTER_H

#include "purrgo/gnss_types.h"

// Обновляет состояние решения на основе входящей NMEA-строки
void purrgo_gnss_process_nmea(const char *nmea_line, purrgo_gnss_solution_t *solution);

#endif // PURRGO_GNSS_ADAPTER_H
