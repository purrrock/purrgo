#ifndef PURRGO_UBLOX7_CONFIG_H
#define PURRGO_UBLOX7_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include "purrgo/gnss_config.h"

bool purrgo_ublox7_set_rate(
    const purrgo_gnss_transport_t *transport,
    uint16_t measurement_period_ms,
    uint16_t timeout_ms);

bool purrgo_ublox7_set_gnss_systems(
    const purrgo_gnss_transport_t *transport,
    uint32_t enabled_systems,
    uint16_t timeout_ms);

bool purrgo_ublox7_set_power_mode(
    const purrgo_gnss_transport_t *transport,
    purrgo_gnss_power_mode_t mode,
    uint16_t timeout_ms);

#endif
