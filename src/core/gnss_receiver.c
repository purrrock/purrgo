#include "purrgo/gnss_receiver.h"
#include "purrgo/hardware_config.h"
#include "purrgo/logger.h"

#include <string.h>

size_t purrgo_nmea_build_frame(char *out, size_t out_size, const char *body)
{
    static const char hex[] = "0123456789ABCDEF";

    if (out == NULL || body == NULL) {
        return 0U;
    }

    size_t body_len = strlen(body);

    /* '$' + body + '*' + 2 цифры + CRLF + '\0' */
    if (body_len > out_size || out_size - body_len < 1U + 1U + 2U + 2U + 1U) {
        return 0U;
    }

    uint8_t checksum = 0U;
    size_t pos = 0U;

    out[pos++] = '$';

    for (size_t i = 0U; i < body_len; i++) {
        checksum ^= (uint8_t)body[i];
        out[pos++] = body[i];
    }

    out[pos++] = '*';
    out[pos++] = hex[checksum >> 4];
    out[pos++] = hex[checksum & 0x0FU];
    out[pos++] = '\r';
    out[pos++] = '\n';
    out[pos] = '\0';

    return pos;
}

static void send_command(purrgo_gnss_tx_fn tx, const char *body)
{
    char frame[96];
    size_t len = purrgo_nmea_build_frame(frame, sizeof frame, body);

    if (len == 0U) {
        PURRGO_LOG("GNSS: command too long: %s\r\n", body);
        return;
    }

    tx((const uint8_t *)frame, len);
}

/*
 * CASIC AT6558R (протокол CASIC Multimode Satellite Navigation Receiver, v4.2).
 *
 * PCAS02 - период навигационных данных, мс (1000 = 1 Гц).
 *
 * PCAS03 - какие предложения выдавать. Формат протокола - 18 полей:
 *   nGGA,nGLL,nGSA,nGSV,nRMC,nVTG,nZDA,nANT,nDHV,nLPS,res1,res2,nUTC,nGST,
 *   res3,res4,res5,nTIM
 * Значение n (0..9): выдавать каждое n-е решение, 0 - не выдавать, пустое поле -
 * оставить как было. Здесь заданы первые 13 полей (до nUTC включительно): GGA, GSA
 * и RMC каждое решение, остальное выключено; nGST и nTIM не затрагиваются.
 * Контрольная сумма вычисляется в purrgo_nmea_build_frame() (раньше была записана
 * вручную и не совпадала).
 */
#define GNSS_CASIC_PCAS02 "PCAS02,1000"
#define GNSS_CASIC_PCAS03 "PCAS03,1,0,1,0,1,0,0,0,0,0,,,0"

void purrgo_gnss_receiver_configure_casic(purrgo_gnss_tx_fn tx)
{
    if (tx == NULL) {
        return;
    }

    send_command(tx, GNSS_CASIC_PCAS02);
    send_command(tx, GNSS_CASIC_PCAS03);
}

/* ------------------------------------------------------------------------- */
/* u-blox UBX                                                                */
/* ------------------------------------------------------------------------- */

#define UBX_SYNC1            0xB5U
#define UBX_SYNC2            0x62U
#define UBX_CLASS_CFG        0x06U
#define UBX_ID_CFG_VALSET    0x8AU
#define UBX_VALSET_MAX_ITEMS 64U

size_t purrgo_ubx_build_frame(uint8_t *out, size_t out_size, uint8_t msg_class,
                              uint8_t msg_id, const uint8_t *payload, uint16_t len)
{
    const size_t total = (size_t)len + 8U;

    if (out == NULL || (len != 0U && payload == NULL) || out_size < total) {
        return 0U;
    }

    out[0] = UBX_SYNC1;
    out[1] = UBX_SYNC2;
    out[2] = msg_class;
    out[3] = msg_id;
    out[4] = (uint8_t)(len & 0xFFU);
    out[5] = (uint8_t)(len >> 8);
    if (len != 0U) {
        memcpy(&out[6], payload, len);
    }

    uint8_t ck_a = 0U;
    uint8_t ck_b = 0U;
    for (size_t i = 2U; i < 6U + (size_t)len; i++) {
        ck_a = (uint8_t)(ck_a + out[i]);
        ck_b = (uint8_t)(ck_b + ck_a);
    }
    out[6U + len] = ck_a;
    out[7U + len] = ck_b;

    return total;
}

/* Размер значения по ключу (биты 30..28); 0 - не поддерживается (8 байт и reserved). */
static size_t ubx_key_value_size(uint32_t key)
{
    switch ((key >> 28) & 0x7U) {
        case 1U: return 1U;
        case 2U: return 1U;
        case 3U: return 2U;
        case 4U: return 4U;
        default: return 0U;
    }
}

size_t purrgo_ubx_build_valset(uint8_t *out, size_t out_size, uint8_t layers,
                               const purrgo_ubx_cfg_item_t *items, size_t count)
{
    uint8_t payload[4U + UBX_VALSET_MAX_ITEMS * 8U];
    size_t pos = 0U;

    if (items == NULL || count == 0U || count > UBX_VALSET_MAX_ITEMS || layers == 0U) {
        return 0U;
    }

    payload[pos++] = 0x00U;    /* version */
    payload[pos++] = layers;
    payload[pos++] = 0x00U;    /* reserved */
    payload[pos++] = 0x00U;

    for (size_t i = 0U; i < count; i++) {
        const size_t vsize = ubx_key_value_size(items[i].key);

        if (vsize == 0U) {
            return 0U;
        }
        for (size_t b = 0U; b < 4U; b++) {
            payload[pos++] = (uint8_t)(items[i].key >> (8U * b));
        }
        for (size_t b = 0U; b < vsize; b++) {
            payload[pos++] = (uint8_t)(items[i].value >> (8U * b));
        }
    }

    return purrgo_ubx_build_frame(out, out_size, UBX_CLASS_CFG, UBX_ID_CFG_VALSET,
                                  payload, (uint16_t)pos);
}

/*
 * G10A F30 (u-blox M10, UBX-M10050-KB). Заводские настройки: 38400 8N1, 1 Гц,
 * NMEA 4.11, выдача GGA, GLL, GSA, GSV, RMC, VTG и TXT. Ключи - из u-blox M10 SPG
 * Interface Description (раздел Configuration defaults).
 *
 * Оставляем GGA, GSA и RMC (их разбирает gnss_adapter.c), GLL, GSV и VTG
 * выключаем: меньше трафика UART и работы разбора. Скорость UART не меняется.
 */
#define UBX_KEY_RATE_MEAS            0x30210001U  /* U2, мс */
#define UBX_KEY_RATE_NAV             0x30210002U  /* U2, циклов измерения на решение */
#define UBX_KEY_MSGOUT_GGA_UART1     0x209100BBU  /* U1 */
#define UBX_KEY_MSGOUT_GLL_UART1     0x209100CAU
#define UBX_KEY_MSGOUT_GSA_UART1     0x209100C0U
#define UBX_KEY_MSGOUT_GSV_UART1     0x209100C5U
#define UBX_KEY_MSGOUT_RMC_UART1     0x209100ACU
#define UBX_KEY_MSGOUT_VTG_UART1     0x209100B1U

void purrgo_gnss_receiver_configure_ubx(purrgo_gnss_tx_fn tx)
{
    static const purrgo_ubx_cfg_item_t items[] = {
        { UBX_KEY_RATE_MEAS,        (uint32_t)PURRGO_GNSS_UPDATE_PERIOD_MS },
        { UBX_KEY_RATE_NAV,         1U },
        { UBX_KEY_MSGOUT_GGA_UART1, 1U },
        { UBX_KEY_MSGOUT_RMC_UART1, 1U },
        { UBX_KEY_MSGOUT_GSA_UART1, 1U },
        { UBX_KEY_MSGOUT_GLL_UART1, 0U },
        { UBX_KEY_MSGOUT_GSV_UART1, 0U },
        { UBX_KEY_MSGOUT_VTG_UART1, 0U },
    };
    uint8_t frame[96];

    if (tx == NULL) {
        return;
    }

    size_t len = purrgo_ubx_build_valset(frame, sizeof frame,
                                         (uint8_t)PURRGO_GNSS_UBX_LAYERS,
                                         items, sizeof items / sizeof items[0]);
    if (len == 0U) {
        PURRGO_LOG("GNSS: UBX-CFG-VALSET build failed\r\n");
        return;
    }

    tx(frame, len);
}

void purrgo_gnss_receiver_configure(purrgo_gnss_tx_fn tx)
{
#if (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_UBX)
    purrgo_gnss_receiver_configure_ubx(tx);
#elif (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_CASIC)
    purrgo_gnss_receiver_configure_casic(tx);
#else
    (void)tx;
#endif
}
