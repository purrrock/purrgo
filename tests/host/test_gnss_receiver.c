/*
 * Настройка приёмника GNSS: формат UBX-кадра, контрольная сумма Fletcher-8,
 * UBX-CFG-VALSET и стартовая конфигурация G10A F30.
 */
#include <string.h>
#include "purrgo/gnss_receiver.h"
#include "purrgo/hardware_config.h"
#include "test_common.h"

/*
 * Кадры настройки: базовая + (набор систем) + (PSM: два кадра) + при PURRGO_GNSS_UBX по VALSET
 * на PVT, ODO, MON-RF, NAV-DOP, (SEC-SIG), одометр, модель динамики, (выключение NMEA) и опрос
 * MON-VER.
 */
#define EXPECTED_FRAMES (1 + PURRGO_GNSS_SIGNAL_FRAME + PURRGO_GNSS_PSM_FRAMES + \
                         (PURRGO_GNSS_UBX ? (7 + PURRGO_GNSS_SEC_SIG + PURRGO_GNSS_NMEA_OFF_FRAMES) : 0))

static char g_sent[16][128];
static size_t g_sent_len[16];
static int g_sent_count;

static void fake_tx(const uint8_t *data, size_t len)
{
    if (g_sent_count < 16 && len < sizeof g_sent[0]) {
        memcpy(g_sent[g_sent_count], data, len);
        g_sent[g_sent_count][len] = '\0';
        g_sent_len[g_sent_count] = len;
        g_sent_count++;
    }
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

    /* Базовая настройка + при PURRGO_GNSS_UBX: по VALSET на каждое сообщение (PVT, ODO, MON-RF, при
     * PURRGO_GNSS_SEC_SIG ещё SEC-SIG), одометр, модель динамики и опрос MON-VER. */
    CHECK(g_sent_count == EXPECTED_FRAMES);

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

/* Разбор VALSET-кадра в пары ключ-значение; возвращает число пар. */
static size_t parse_valset(const uint8_t *f, size_t len, uint32_t *keys, uint32_t *values, size_t max)
{
    size_t pos = 10;
    size_t n = 0;

    while (pos + 4U <= len - 2U && n < max) {
        uint32_t key = (uint32_t)f[pos] | ((uint32_t)f[pos + 1] << 8) |
                       ((uint32_t)f[pos + 2] << 16) | ((uint32_t)f[pos + 3] << 24);
        size_t vsize = (((key >> 28) & 7U) == 3U) ? 2U : (((key >> 28) & 7U) == 4U) ? 4U : 1U;
        uint32_t value = 0;
        for (size_t b = 0; b < vsize; b++) {
            value |= (uint32_t)f[pos + 4U + b] << (8U * b);
        }
        keys[n] = key;
        values[n] = value;
        n++;
        pos += 4U + vsize;
    }
    return n;
}

/* Ищет среди отправленных кадров VALSET, содержащий ключ; возвращает значение или -1. */
static int64_t find_key(uint32_t key, size_t *frame_index, size_t *pairs_in_frame)
{
    for (int i = 0; i < g_sent_count; i++) {
        const uint8_t *f = (const uint8_t *)g_sent[i];
        if (g_sent_len[i] < 12 || f[2] != 0x06 || f[3] != 0x8A) {
            continue;
        }
        uint32_t keys[16], values[16];
        size_t n = parse_valset(f, g_sent_len[i], keys, values, 16);
        for (size_t k = 0; k < n; k++) {
            if (keys[k] == key) {
                *frame_index = (size_t)i;
                *pairs_in_frame = n;
                return (int64_t)values[k];
            }
        }
    }
    return -1;
}

static void test_power_save(void)
{
    size_t frame = 0, pairs = 0, f2 = 0, p2 = 0;

    g_sent_count = 0;
    purrgo_gnss_receiver_configure_ubx(fake_tx);

    /* PSM: двумя отдельными кадрами (NAK называет точный шаг), OPERATEMODE - последним. */
    const int64_t mode = find_key(0x20D00001U, &frame, &pairs);
    const int64_t no_off = find_key(0x10D00008U, &f2, &p2);
#if PURRGO_GNSS_PSM_ACTIVE
    CHECK(mode == PURRGO_GNSS_PSM && pairs == 1);
    CHECK(no_off == 1 && p2 == 1 && f2 + 1 == frame);
    CHECK(find_key(0x10D0000AU, &f2, &p2) == -1);                  /* UPDATEEPH по умолчанию уже включён */
    CHECK(find_key(0x30210001U, &f2, &p2) >= 0 && f2 != frame);    /* частота решений - в другом кадре */
#else
    CHECK(mode == -1 && no_off == -1);
#endif

    /*
     * Набор систем одним кадром: при PSM выключаются SBAS и BeiDou (в PSM не работают),
     * GLONASS - только при PURRGO_GNSS_SIG_GPS_GAL. GPS и Galileo не трогаются.
     */
    const int64_t sbas = find_key(0x10310020U, &frame, &pairs);
    const int64_t bds = find_key(0x10310022U, &frame, &pairs);
    const int64_t glo = find_key(0x10310025U, &frame, &pairs);
    CHECK(sbas == (PURRGO_GNSS_SIG_OFF_SBAS ? 0 : -1));
    CHECK(bds == (PURRGO_GNSS_SIG_OFF_BDS ? 0 : -1));
    CHECK(glo == (PURRGO_GNSS_SIG_OFF_GLO ? 0 : -1));
    CHECK(find_key(0x1031001FU, &f2, &p2) == -1);
    CHECK(find_key(0x10310021U, &f2, &p2) == -1);
#if PURRGO_GNSS_SIGNAL_FRAME
    {
        size_t sig_frame = 0, np = 0;
        const int64_t any = PURRGO_GNSS_SIG_OFF_BDS ? find_key(0x10310022U, &sig_frame, &np)
                                                    : find_key(0x10310025U, &sig_frame, &np);
        CHECK(any == 0);
        CHECK(np == PURRGO_GNSS_SIGNAL_COUNT);
        CHECK(sig_frame == 1);                                      /* сразу за базовой настройкой */
    }
#endif
}

static void test_configure(void)
{
    g_sent_count = 0;
    purrgo_gnss_receiver_configure(fake_tx);
    purrgo_gnss_receiver_configure(NULL);       /* без функции отправки - ничего, без падения */

#if (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_UBX)
    CHECK(g_sent_count == EXPECTED_FRAMES);
    CHECK((uint8_t)g_sent[0][0] == 0xB5 && (uint8_t)g_sent[0][1] == 0x62);
#else
    CHECK(g_sent_count == 0);
#endif
}

static void test_build_baud(void)
{
    uint8_t out[32];

#if (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_UBX)
    /* Эталоны: UBX-CFG-VALSET, слой RAM (PURRGO_GNSS_UBX_LAYERS = 0x01), CFG-UART1-BAUDRATE
     * (ключ 0x40520001, U4). Контрольная сумма посчитана независимо (Fletcher-8). */
    static const uint8_t expect_115200[] = {
        0xB5, 0x62, 0x06, 0x8A, 0x0C, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00,
        0x52, 0x40, 0x00, 0xC2, 0x01, 0x00, 0xF3, 0xA5
    };
    static const uint8_t expect_38400[] = {
        0xB5, 0x62, 0x06, 0x8A, 0x0C, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00,
        0x52, 0x40, 0x00, 0x96, 0x00, 0x00, 0xC6, 0x1F
    };

    CHECK(PURRGO_GNSS_UBX_LAYERS == 0x01);
    size_t n = purrgo_gnss_receiver_build_baud(out, sizeof out, 115200U);
    CHECK(n == sizeof expect_115200);
    CHECK(memcmp(out, expect_115200, sizeof expect_115200) == 0);

    n = purrgo_gnss_receiver_build_baud(out, sizeof out, 38400U);
    CHECK(n == sizeof expect_38400);
    CHECK(memcmp(out, expect_38400, sizeof expect_38400) == 0);

    /* Граница размера и некорректные аргументы. */
    CHECK(purrgo_gnss_receiver_build_baud(out, 20, 115200U) == 20);
    CHECK(purrgo_gnss_receiver_build_baud(out, 19, 115200U) == 0);
    CHECK(purrgo_gnss_receiver_build_baud(NULL, 32, 115200U) == 0);
    CHECK(purrgo_gnss_receiver_build_baud(out, sizeof out, 0U) == 0);
#else
    CHECK(purrgo_gnss_receiver_build_baud(out, sizeof out, 115200U) == 0);
#endif
}

int main(void)
{
    test_build_baud();
    test_ubx_frame();
    test_ubx_configure();
    test_power_save();
    test_configure();
    return test_finish("test_gnss_receiver");
}
