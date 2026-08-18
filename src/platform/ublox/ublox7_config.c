#include "ublox7_config.h"
#include "purrgo/ubx.h"

#define UBX_CLASS_CFG 0x06u
#define UBX_ID_CFG_GNSS 0x3Eu
#define UBX_ID_CFG_RATE 0x08u
#define UBX_ID_CFG_RXM  0x11u

bool purrgo_ublox7_set_rate(
    const purrgo_gnss_transport_t *transport,
    uint16_t measurement_period_ms, uint16_t timeout_ms)
{
    (void)timeout_ms;
    if (transport == NULL || transport->write == NULL)
        return false;

    /* u-blox 7 Protocol 14: measRate=ms, navRate=1, timeRef=0 (UTC). */
    const uint8_t payload[6] = {
        (uint8_t)measurement_period_ms,
        (uint8_t)(measurement_period_ms >> 8),
        0x01u, 0x00u,
        0x00u, 0x00u
    };
    uint8_t frame[14];
    const size_t n = purrgo_ubx_build(
        UBX_CLASS_CFG, UBX_ID_CFG_RATE, payload, sizeof(payload),
        frame, sizeof(frame));

    return n != 0u &&
           transport->write(frame, (uint16_t)n, transport->context);
}

bool purrgo_ublox7_set_gnss_systems(
    const purrgo_gnss_transport_t *transport,
    uint32_t enabled_systems, uint16_t timeout_ms)
{
    /*
     * Deliberately not implemented yet.
     * CFG-GNSS contains receiver/channel allocation fields. We must first
     * poll and parse the receiver's actual configuration instead of
     * guessing channel counts.
     */
    (void)transport;
    (void)enabled_systems;
    (void)timeout_ms;
    return false;
}

bool purrgo_ublox7_set_power_mode(
    const purrgo_gnss_transport_t *transport,
    purrgo_gnss_power_mode_t mode, uint16_t timeout_ms)
{
    (void)timeout_ms;
    if (transport == NULL || transport->write == NULL)
        return false;
    if (mode != PURRGO_GNSS_POWER_CONTINUOUS &&
        mode != PURRGO_GNSS_POWER_SAVE)
        return false;

    /* u-blox 7 Protocol 14 CFG-RXM: reserved1=8, lpMode 0/1. */
    const uint8_t payload[2] = { 0x08u, (uint8_t)mode };
    uint8_t frame[10];
    const size_t n = purrgo_ubx_build(
        UBX_CLASS_CFG, UBX_ID_CFG_RXM, payload, sizeof(payload),
        frame, sizeof(frame));

    return n != 0u &&
           transport->write(frame, (uint16_t)n, transport->context);
}
