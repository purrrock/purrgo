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
static size_t g_sent_len[8];
static int g_sent_count;

static void fake_tx(const uint8_t *data, size_t len)
{
    if (g_sent_count < 8 && len < sizeof g_sent[0]) {
        memcpy(g_sent[g_sent_count], data, len);
        g_sent[g_sent_count][len] = '\0';
        g_sent_len[g_sent_count] = len;
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

static void check_nmea_frames(void)
{
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
}

static void test_casic(void)
{
    g_sent_count = 0;
    purrgo_gnss_receiver_configure_casic(fake_tx);
    purrgo_gnss_receiver_configure_casic(NULL);     /* без функции отправки - ничего */

    CHECK(g_sent_count == 2);
    CHECK(strncmp(g_sent[0], "$PCAS02,1000*", 13) == 0);
    CHECK(strncmp(g_sent[1], "$PCAS03,", 8) == 0);
    check_nmea_frames();
    /* PCAS03: GGA, GSA и RMC включены, GSV выключен (как требует разбор в gnss.c). */
    CHECK(strncmp(g_sent[1], "$PCAS03,1,0,1,0,1,0,", 20) == 0);
}

static void ubx_checksum(const uint8_t *frame, size_t len, uint8_t *a, uint8_t *b)
{
    *a = 0;
    *b = 0;
    for (size_t i = 2; i + 2 < len; i++) {
        *a = (uint8_t)(*a + frame[i]);
        *b = (uint8_t)(*b + *a);
    }
}

static void test_ubx_frame(void)
{
    uint8_t out[64];
    const uint8_t payload[] = {0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x21, 0x30, 0xE8, 0x03};

    /* Эталон: UBX-CFG-VALSET, слой RAM, CFG-RATE-MEAS = 1000 мс. */
    static const uint8_t expect[] = {
        0xB5, 0x62, 0x06, 0x8A, 0x0A, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x21, 0x30, 0xE8, 0x03,
        0xD8, 0xC4
    };
    size_t n = purrgo_ubx_build_frame(out, sizeof out, 0x06, 0x8A, payload, sizeof payload);
    CHECK(n == sizeof expect);
    CHECK(memcmp(out, expect, sizeof expect) == 0);

    /* Граница размера и некорректные аргументы. */
    CHECK(purrgo_ubx_build_frame(out, 18, 0x06, 0x8A, payload, sizeof payload) == 18);
    CHECK(purrgo_ubx_build_frame(out, 17, 0x06, 0x8A, payload, sizeof payload) == 0);
    CHECK(purrgo_ubx_build_frame(NULL, 64, 0x06, 0x8A, payload, sizeof payload) == 0);
    CHECK(purrgo_ubx_build_frame(out, 64, 0x06, 0x8A, NULL, 4) == 0);

    /* Пустой payload допустим: B5 62 cls id 00 00 ck. */
    CHECK(purrgo_ubx_build_frame(out, sizeof out, 0x0A, 0x04, NULL, 0) == 8);
    CHECK(out[4] == 0 && out[5] == 0);

    /* VALSET с одним элементом даёт тот же кадр, что и эталон. */
    const purrgo_ubx_cfg_item_t item = { 0x30210001U, 1000U };
    n = purrgo_ubx_build_valset(out, sizeof out, 0x01, &item, 1);
    CHECK(n == sizeof expect);
    CHECK(memcmp(out, expect, sizeof expect) == 0);

    /* Некорректные параметры VALSET. */
    CHECK(purrgo_ubx_build_valset(out, sizeof out, 0x00, &item, 1) == 0);   /* нет слоя */
    CHECK(purrgo_ubx_build_valset(out, sizeof out, 0x01, &item, 0) == 0);
    CHECK(purrgo_ubx_build_valset(out, sizeof out, 0x01, NULL, 1) == 0);
    CHECK(purrgo_ubx_build_valset(out, 10, 0x01, &item, 1) == 0);           /* мало места */
    const purrgo_ubx_cfg_item_t bad = { 0x50210001U, 0U };                  /* 8 байт */
    CHECK(purrgo_ubx_build_valset(out, sizeof out, 0x01, &bad, 1) == 0);
}

static void test_ubx_configure(void)
{
    g_sent_count = 0;
    purrgo_gnss_receiver_configure_ubx(fake_tx);
    purrgo_gnss_receiver_configure_ubx(NULL);

    CHECK(g_sent_count == 1);

    const uint8_t *f = (const uint8_t *)g_sent[0];
    const size_t len = g_sent_len[0];

    CHECK(len >= 8);
    CHECK(f[0] == 0xB5 && f[1] == 0x62 && f[2] == 0x06 && f[3] == 0x8A);
    CHECK((size_t)(f[4] | (f[5] << 8)) + 8U == len);
    CHECK(f[6] == 0x00 && f[7] == (uint8_t)PURRGO_GNSS_UBX_LAYERS);

    uint8_t a, b;
    ubx_checksum(f, len, &a, &b);
    CHECK(f[len - 2] == a && f[len - 1] == b);

    /* Разбор пар ключ-значение: GGA, RMC, GSA включены; GLL, GSV, VTG выключены. */
    struct { uint32_t key; uint32_t value; int seen; } want[] = {
        { 0x30210001U, PURRGO_GNSS_UPDATE_PERIOD_MS, 0 },
        { 0x30210002U, 1U, 0 },
        { 0x209100BBU, 1U, 0 },   /* GGA */
        { 0x209100ACU, 1U, 0 },   /* RMC */
        { 0x209100C0U, 1U, 0 },   /* GSA */
        { 0x209100CAU, 0U, 0 },   /* GLL */
        { 0x209100C5U, 0U, 0 },   /* GSV */
        { 0x209100B1U, 0U, 0 },   /* VTG */
    };
    size_t pos = 10;               /* заголовок 6 + version/layers/reserved 4 */
    size_t pairs = 0;
    while (pos + 4U <= len - 2U) {
        uint32_t key = (uint32_t)f[pos] | ((uint32_t)f[pos + 1] << 8) |
                       ((uint32_t)f[pos + 2] << 16) | ((uint32_t)f[pos + 3] << 24);
        size_t vsize = (((key >> 28) & 7U) == 3U) ? 2U : 1U;
        uint32_t value = f[pos + 4];
        if (vsize == 2U) {
            value |= (uint32_t)f[pos + 5] << 8;
        }
        pos += 4U + vsize;
        pairs++;
        for (size_t i = 0; i < sizeof want / sizeof want[0]; i++) {
            if (want[i].key == key) {
                CHECK(want[i].value == value);
                want[i].seen++;
            }
        }
    }
    CHECK(pos == len - 2U);
    CHECK(pairs == sizeof want / sizeof want[0]);
    for (size_t i = 0; i < sizeof want / sizeof want[0]; i++) {
        CHECK(want[i].seen == 1);
    }
}

static void test_configure(void)
{
    g_sent_count = 0;
    purrgo_gnss_receiver_configure(fake_tx);
    purrgo_gnss_receiver_configure(NULL);       /* без функции отправки - ничего, без падения */

#if (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_UBX)
    CHECK(g_sent_count == 1);
    CHECK((uint8_t)g_sent[0][0] == 0xB5 && (uint8_t)g_sent[0][1] == 0x62);
#elif (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_CASIC)
    CHECK(g_sent_count == 2);
    CHECK(strncmp(g_sent[0], "$PCAS02,1000*", 13) == 0);
#else
    CHECK(g_sent_count == 0);
#endif
}

int main(void)
{
    test_frame();
    test_casic();
    test_ubx_frame();
    test_ubx_configure();
    test_configure();
    return test_finish("test_gnss_receiver");
}
