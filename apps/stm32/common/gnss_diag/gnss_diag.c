/*
 * Диагностика связи с GNSS: разбор NMEA и счётчики. См. gnss_diag.h.
 */

#include "gnss_diag.h"

#include <string.h>

enum
{
    PS_IDLE = 0,
    PS_BODY,
    PS_CS_HI,
    PS_CS_LO
};

void gnss_diag_init(gnss_diag_t *d)
{
    if (d != NULL)
    {
        memset(d, 0, sizeof *d);
    }
}

static int hex_value(uint8_t c)
{
    if ((c >= (uint8_t)'0') && (c <= (uint8_t)'9'))
    {
        return (int)(c - (uint8_t)'0');
    }
    if ((c >= (uint8_t)'A') && (c <= (uint8_t)'F'))
    {
        return (int)(c - (uint8_t)'A') + 10;
    }
    if ((c >= (uint8_t)'a') && (c <= (uint8_t)'f'))
    {
        return (int)(c - (uint8_t)'a') + 10;
    }
    return -1;
}

/* Найти поле idx (0 - идентификатор предложения). Возвращает false, если поля нет. */
static bool find_field(const gnss_diag_t *d, unsigned idx, unsigned *start, unsigned *len)
{
    unsigned field = 0U;
    unsigned begin = 0U;

    for (unsigned i = 0U; i <= d->body_len; i++)
    {
        if ((i == d->body_len) || (d->body[i] == ','))
        {
            if (field == idx)
            {
                *start = begin;
                *len = i - begin;
                return true;
            }
            field++;
            begin = i + 1U;
        }
    }
    return false;
}

static uint8_t parse_u8(const gnss_diag_t *d, unsigned idx)
{
    unsigned start;
    unsigned len;
    unsigned value = 0U;

    if (!find_field(d, idx, &start, &len))
    {
        return 0U;
    }
    for (unsigned i = 0U; i < len; i++)
    {
        const char c = d->body[start + i];

        if ((c < '0') || (c > '9'))
        {
            break;
        }
        value = (value * 10U) + (unsigned)(c - '0');
        if (value > 255U)
        {
            return 255U;
        }
    }
    return (uint8_t)value;
}

static unsigned talker_index(char t1)
{
    switch (t1)
    {
    case 'P': return 0U;   /* GPS */
    case 'L': return 1U;   /* GLONASS */
    case 'A': return 2U;   /* Galileo */
    case 'B':              /* BeiDou: BD / GB */
    case 'D': return 3U;
    case 'Q': return 4U;   /* QZSS */
    default:  return 5U;   /* GN и прочие */
    }
}

static void handle_sentence(gnss_diag_t *d)
{
    /* Тело: 2 символа talker + 3 символа типа + запятая, например "GNGGA,...". */
    if ((d->body_len < 6U) || (d->body[5] != ','))
    {
        d->seen_mask |= GNSS_DIAG_SEEN_OTHER;
        return;
    }

    const char t1 = d->body[1];
    const char *type = &d->body[2];

    if (memcmp(type, "GGA", 3U) == 0)
    {
        d->seen_mask |= GNSS_DIAG_SEEN_GGA;
        d->gga_seen = true;
        d->gga_quality = parse_u8(d, 6U);
        d->gga_sats_used = parse_u8(d, 7U);
    }
    else if (memcmp(type, "RMC", 3U) == 0)
    {
        unsigned start;
        unsigned len;

        d->seen_mask |= GNSS_DIAG_SEEN_RMC;
        d->rmc_seen = true;
        d->rmc_valid = find_field(d, 2U, &start, &len) && (len >= 1U) && (d->body[start] == 'A');
    }
    else if (memcmp(type, "GSV", 3U) == 0)
    {
        d->seen_mask |= GNSS_DIAG_SEEN_GSV;
        d->gsv_seen = true;
        d->gsv_in_view[talker_index(t1)] = parse_u8(d, 3U);
    }
    else if (memcmp(type, "GSA", 3U) == 0)
    {
        d->seen_mask |= GNSS_DIAG_SEEN_GSA;
    }
    else if (memcmp(type, "VTG", 3U) == 0)
    {
        d->seen_mask |= GNSS_DIAG_SEEN_VTG;
    }
    else if (memcmp(type, "GLL", 3U) == 0)
    {
        d->seen_mask |= GNSS_DIAG_SEEN_GLL;
    }
    else
    {
        d->seen_mask |= GNSS_DIAG_SEEN_OTHER;
    }
}

void gnss_diag_feed(gnss_diag_t *d, uint8_t byte)
{
    if (d == NULL)
    {
        return;
    }

    d->bytes++;
    if (((byte >= 0x20U) && (byte <= 0x7EU)) || (byte == (uint8_t)'\r') || (byte == (uint8_t)'\n'))
    {
        d->printable++;
    }

    /* '$' всегда начинает новое предложение. */
    if (byte == (uint8_t)'$')
    {
        if ((d->parse_state != PS_IDLE))
        {
            d->sentences_bad++;      /* предыдущее предложение оборвано */
        }
        d->parse_state = PS_BODY;
        d->xor_acc = 0U;
        d->body_len = 0U;
        d->body_overflow = false;
        return;
    }

    switch (d->parse_state)
    {
    case PS_BODY:
        if (byte == (uint8_t)'*')
        {
            d->parse_state = PS_CS_HI;
        }
        else if ((byte == (uint8_t)'\r') || (byte == (uint8_t)'\n'))
        {
            d->sentences_bad++;
            d->parse_state = PS_IDLE;
        }
        else
        {
            d->xor_acc ^= byte;
            if (d->body_len < (GNSS_DIAG_BODY_MAX - 1U))
            {
                d->body[d->body_len] = (char)byte;
                d->body_len++;
            }
            else
            {
                d->body_overflow = true;
            }
        }
        break;

    case PS_CS_HI:
    {
        const int v = hex_value(byte);

        if (v < 0)
        {
            d->sentences_bad++;
            d->parse_state = PS_IDLE;
        }
        else
        {
            d->cs_hi = (uint8_t)v;
            d->parse_state = PS_CS_LO;
        }
        break;
    }

    case PS_CS_LO:
    {
        const int v = hex_value(byte);

        d->parse_state = PS_IDLE;
        if ((v >= 0) && !d->body_overflow &&
            ((uint8_t)((d->cs_hi << 4) | (uint8_t)v) == d->xor_acc))
        {
            d->sentences_ok++;
            handle_sentence(d);
        }
        else
        {
            d->sentences_bad++;
        }
        break;
    }

    case PS_IDLE:
    default:
        break;
    }
}

void gnss_diag_uart_error(gnss_diag_t *d, uint32_t err_bits)
{
    if (d == NULL)
    {
        return;
    }
    if ((err_bits & GNSS_DIAG_UART_ERR_PE) != 0U)
    {
        d->err_pe++;
    }
    if ((err_bits & GNSS_DIAG_UART_ERR_NE) != 0U)
    {
        d->err_ne++;
    }
    if ((err_bits & GNSS_DIAG_UART_ERR_FE) != 0U)
    {
        d->err_fe++;
    }
    if ((err_bits & GNSS_DIAG_UART_ERR_ORE) != 0U)
    {
        d->err_ore++;
    }
    if ((err_bits & GNSS_DIAG_UART_ERR_DMA) != 0U)
    {
        d->err_dma++;
    }
}

uint16_t gnss_diag_sats_in_view(const gnss_diag_t *d)
{
    uint16_t sum = 0U;

    if (d == NULL)
    {
        return 0U;
    }
    for (unsigned i = 0U; i < GNSS_DIAG_TALKERS; i++)
    {
        sum = (uint16_t)(sum + d->gsv_in_view[i]);
    }
    return sum;
}

gnss_diag_state_t gnss_diag_state(const gnss_diag_t *d)
{
    if ((d == NULL) || (d->bytes == 0U))
    {
        return GNSS_DIAG_NO_DATA;
    }
    if (d->sentences_ok == 0U)
    {
        return GNSS_DIAG_NOT_NMEA;
    }
    if ((d->gga_seen && (d->gga_quality > 0U)) || (d->rmc_seen && d->rmc_valid))
    {
        return GNSS_DIAG_FIX;
    }
    return GNSS_DIAG_NMEA_NO_FIX;
}
