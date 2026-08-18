#ifndef PURRGO_GNSS_CONFIG_H
#define PURRGO_GNSS_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PURRGO_GNSS_POWER_CONTINUOUS = 0,
    PURRGO_GNSS_POWER_SAVE = 1
} purrgo_gnss_power_mode_t;

typedef enum {
    PURRGO_GNSS_SYSTEM_GPS     = (1u << 0),
    PURRGO_GNSS_SYSTEM_GLONASS = (1u << 1),
    PURRGO_GNSS_SYSTEM_GALILEO = (1u << 2),
    PURRGO_GNSS_SYSTEM_BEIDOU  = (1u << 3),
    PURRGO_GNSS_SYSTEM_QZSS    = (1u << 4),
    PURRGO_GNSS_SYSTEM_SBAS    = (1u << 5)
} purrgo_gnss_system_t;

typedef struct {
    uint16_t measurement_period_ms;
    uint32_t enabled_systems;
    purrgo_gnss_power_mode_t power_mode;
} purrgo_gnss_config_t;

typedef struct {
    bool (*write)(const uint8_t *data, uint16_t length, void *context);
    void *context;
} purrgo_gnss_transport_t;

#endif
