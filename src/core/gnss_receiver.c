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

#if (PURRGO_HW_GNSS_RECEIVER != PURRGO_GNSS_RECEIVER_NONE)

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

#endif

#if (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_CASIC)

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

void purrgo_gnss_receiver_configure(purrgo_gnss_tx_fn tx)
{
    if (tx == NULL) {
        return;
    }

    send_command(tx, GNSS_CASIC_PCAS02);
    send_command(tx, GNSS_CASIC_PCAS03);
}

#else /* PURRGO_GNSS_RECEIVER_NONE */

void purrgo_gnss_receiver_configure(purrgo_gnss_tx_fn tx)
{
    (void)tx;
}

#endif
