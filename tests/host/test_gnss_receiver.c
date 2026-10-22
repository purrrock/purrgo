/*
 * Настройка приёмника GNSS (патч 0023): формат NMEA-кадра, контрольная сумма и
 * последовательность команд CASIC. Контрольная сумма примера из документации
 * CASIC ($PCAS03,1,1,1,1,1,1,1,1,0,0,,,1,1,,,,1*33) - эталон.
 */
#include <string.h>
#include "purrgo/gnss_receiver.h"
#include "purrgo/hardware_config.h"
#include "test_common.h"

static char g_sent[8][128];
static int g_sent_count;

static void fake_tx(const uint8_t *data, size_t len)
{
    if (g_sent_count < 8 && len < sizeof g_sent[0]) {
        memcpy(g_sent[g_sent_count], data, len);
        g_sent[g_sent_count][len] = '\0';
        g_sent_count++;
    }
}

static unsigned xor_of(const char *s)
{
    unsigned c = 0;
    for (; *s; s++) { c ^= (unsigned char)*s; }
    return c;
}

static void test_frame(void)
{
    char out[96];
    size_t n = purrgo_nmea_build_frame(out, sizeof out, "PCAS03,1,1,1,1,1,1,1,1,0,0,,,1,1,,,,1");

    CHECK(n == strlen(out));
    CHECK(strcmp(out, "$PCAS03,1,1,1,1,1,1,1,1,0,0,,,1,1,,,,1*33\r\n") == 0);

    n = purrgo_nmea_build_frame(out, sizeof out, "PCAS02,1000");
    char expect[64];
    snprintf(expect, sizeof expect, "$PCAS02,1000*%02X\r\n", xor_of("PCAS02,1000"));
    CHECK(strcmp(out, expect) == 0 && n == strlen(expect));

    /* Граница размера: '$' + тело + '*HH' + CRLF + '\0'. */
    const char *body = "ABC";
    size_t need = 1 + 3 + 3 + 2 + 1;
    CHECK(purrgo_nmea_build_frame(out, need, body) == need - 1);
    CHECK(purrgo_nmea_build_frame(out, need - 1, body) == 0);
    CHECK(purrgo_nmea_build_frame(out, 0, body) == 0);
    CHECK(purrgo_nmea_build_frame(NULL, 16, body) == 0);
    CHECK(purrgo_nmea_build_frame(out, sizeof out, NULL) == 0);

    /* Пустое тело допустимо. */
    CHECK(purrgo_nmea_build_frame(out, sizeof out, "") == 6);
    CHECK(strcmp(out, "$*00\r\n") == 0);
}

static void test_configure(void)
{
    g_sent_count = 0;
    purrgo_gnss_receiver_configure(fake_tx);
    purrgo_gnss_receiver_configure(NULL);       /* без функции отправки - ничего, без падения */

#if (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_CASIC)
    CHECK(g_sent_count == 2);
    CHECK(strncmp(g_sent[0], "$PCAS02,1000*", 13) == 0);
    CHECK(strncmp(g_sent[1], "$PCAS03,", 8) == 0);

    /* Все кадры корректны: '$', контрольная сумма совпадает, оканчиваются CRLF. */
    for (int i = 0; i < g_sent_count; i++) {
        const char *f = g_sent[i];
        size_t len = strlen(f);
        const char *star = strchr(f, '*');
        CHECK(f[0] == '$' && star != NULL && len >= 6 && strcmp(f + len - 2, "\r\n") == 0);
        if (star != NULL) {
            char body[96];
            size_t bl = (size_t)(star - f) - 1;
            memcpy(body, f + 1, bl);
            body[bl] = '\0';
            unsigned got = 0;
            sscanf(star + 1, "%2X", &got);
            CHECK(got == xor_of(body));
        }
    }
    /* PCAS03: GGA, GSA и RMC включены, GSV выключен (как требует разбор в gnss.c). */
    CHECK(strncmp(g_sent[1], "$PCAS03,1,0,1,0,1,0,", 20) == 0);
#else
    CHECK(g_sent_count == 0);
#endif
}

int main(void)
{
    test_frame();
    test_configure();
    return test_finish("test_gnss_receiver");
}
