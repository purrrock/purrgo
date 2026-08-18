#include "purrgo/ubx.h"

void purrgo_ubx_checksum(
    uint8_t msg_class, uint8_t msg_id, uint16_t payload_length,
    const uint8_t *payload, uint8_t *ck_a, uint8_t *ck_b)
{
    uint8_t a = 0;
    uint8_t b = 0;
    const uint8_t h[4] = {
        msg_class, msg_id,
        (uint8_t)(payload_length & 0xFFu),
        (uint8_t)(payload_length >> 8)
    };

    for (unsigned i = 0; i < 4u; ++i) {
        a = (uint8_t)(a + h[i]);
        b = (uint8_t)(b + a);
    }
    for (uint16_t i = 0; i < payload_length; ++i) {
        a = (uint8_t)(a + payload[i]);
        b = (uint8_t)(b + a);
    }
    *ck_a = a;
    *ck_b = b;
}

size_t purrgo_ubx_build(
    uint8_t msg_class, uint8_t msg_id,
    const uint8_t *payload, uint16_t payload_length,
    uint8_t *out, size_t out_capacity)
{
    const size_t n = 8u + (size_t)payload_length;
    if (out == NULL || out_capacity < n ||
        (payload_length != 0u && payload == NULL))
        return 0;

    out[0] = PURRGO_UBX_SYNC_CHAR_1;
    out[1] = PURRGO_UBX_SYNC_CHAR_2;
    out[2] = msg_class;
    out[3] = msg_id;
    out[4] = (uint8_t)(payload_length & 0xFFu);
    out[5] = (uint8_t)(payload_length >> 8);

    for (uint16_t i = 0; i < payload_length; ++i)
        out[6u + i] = payload[i];

    purrgo_ubx_checksum(msg_class, msg_id, payload_length, payload,
                        &out[6u + payload_length],
                        &out[7u + payload_length]);
    return n;
}
