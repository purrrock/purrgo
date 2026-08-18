#ifndef PURRGO_UBX_H
#define PURRGO_UBX_H

#include <stddef.h>
#include <stdint.h>

#define PURRGO_UBX_SYNC_CHAR_1 0xB5u
#define PURRGO_UBX_SYNC_CHAR_2 0x62u

void purrgo_ubx_checksum(
    uint8_t msg_class, uint8_t msg_id, uint16_t payload_length,
    const uint8_t *payload, uint8_t *ck_a, uint8_t *ck_b);

size_t purrgo_ubx_build(
    uint8_t msg_class, uint8_t msg_id,
    const uint8_t *payload, uint16_t payload_length,
    uint8_t *out, size_t out_capacity);

#endif
