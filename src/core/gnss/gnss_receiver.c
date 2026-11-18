#include "purrgo/gnss_receiver.h"
#include "purrgo/gnss_ubx.h"
#include "purrgo/hardware_config.h"
#include "purrgo/logger.h"
#include "purrgo/purrgo_format.h"

#include <stdarg.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* u-blox UBX                                                                */
/* ------------------------------------------------------------------------- */

#define UBX_SYNC1            0xB5U
#define UBX_SYNC2            0x62U
#define UBX_CLASS_CFG        0x06U
#define UBX_ID_CFG_VALSET    0x8AU
#define UBX_ID_CFG_VALDEL    0x8CU
#define UBX_VALDEL_MAX_KEYS  64U
#define UBX_CLASS_MON        0x0AU
#define UBX_ID_MON_VER       0x04U
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

size_t purrgo_ubx_build_valdel(uint8_t *out, size_t out_size, uint8_t layers,
                               const uint32_t *keys, size_t count)
{
    uint8_t payload[4U + 8U * 4U];
    size_t pos = 0U;

    /* Ключей для PM меньше десятка: буфер на 8 ключей, больше не нужно. */
    if (keys == NULL || count == 0U || count > 8U || count > UBX_VALDEL_MAX_KEYS ||
        (layers & 0x06U) == 0U || (layers & (uint8_t)~0x06U) != 0U) {
        return 0U;
    }

    payload[pos++] = 0x00U;    /* version */
    payload[pos++] = layers;
    payload[pos++] = 0x00U;    /* reserved */
    payload[pos++] = 0x00U;
    for (size_t i = 0U; i < count; i++) {
        for (size_t b = 0U; b < 4U; b++) {
            payload[pos++] = (uint8_t)(keys[i] >> (8U * b));
        }
    }
    return purrgo_ubx_build_frame(out, out_size, UBX_CLASS_CFG, UBX_ID_CFG_VALDEL,
                                  payload, (uint16_t)pos);
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
 * выключаем: меньше трафика UART и работы разбора. Скорость UART меняет отдельная
 * команда (purrgo_gnss_receiver_build_baud), её отправляет gnss_link.
 */
#define UBX_KEY_RATE_MEAS            0x30210001U  /* U2, мс */
#define UBX_KEY_RATE_NAV             0x30210002U  /* U2, циклов измерения на решение */
#define UBX_KEY_MSGOUT_GGA_UART1     0x209100BBU  /* U1 */
#define UBX_KEY_MSGOUT_GLL_UART1     0x209100CAU
#define UBX_KEY_MSGOUT_GSA_UART1     0x209100C0U
#define UBX_KEY_MSGOUT_GSV_UART1     0x209100C5U
#define UBX_KEY_MSGOUT_RMC_UART1     0x209100ACU
#define UBX_KEY_MSGOUT_VTG_UART1     0x209100B1U

/*
 * Расширения UBX (PURRGO_GNSS_UBX = 1, см. hardware_config.h). Каждая группа
 * отправляется отдельным UBX-CFG-VALSET: модуль применяет набор целиком или
 * отклоняет целиком (NAK), и отказ по одному ключу (например, SEC-SIG в SPG 5.10
 * или неподдерживаемая модель динамики) не должен отменять остальное. Сообщения
 * включаются по одному ключу на команду.
 */
#define UBX_KEY_MSGOUT_PVT_UART1     0x20910007U  /* U1: UBX-NAV-PVT */
#define UBX_KEY_MSGOUT_ODO_UART1     0x2091007FU  /* U1: UBX-NAV-ODO */
#define UBX_KEY_MSGOUT_MON_RF_UART1  0x2091035AU  /* U1: UBX-MON-RF */
#define UBX_KEY_MSGOUT_DOP_UART1     0x20910039U  /* U1: UBX-NAV-DOP (HDOP/VDOP/PDOP) */
#define UBX_KEY_MSGOUT_STATUS_UART1  0x2091001BU  /* U1: UBX-NAV-STATUS (spoofDetState) */
#define UBX_KEY_MSGOUT_SAT_UART1     0x20910016U  /* U1: UBX-NAV-SAT (C/N0 спутников) */
#define UBX_KEY_ITFM_ENABLE          0x1041000DU  /* L: монитор помех (jammingState в MON-RF); по умолчанию выключен */
#define UBX_KEY_MSGOUT_SEC_SIG_UART1 0x20910635U  /* U1: UBX-SEC-SIG */
#define UBX_KEY_ODO_USE_ODO          0x10220001U  /* L: одометр (по умолчанию выключен) */
#define UBX_KEY_ODO_PROFILE          0x20220005U  /* E1: 0 RUN, 1 CYCL, 2 SWIM, 3 CAR */
#define UBX_KEY_UART1OUTPROT_NMEA    0x10740002U  /* L: выдавать NMEA на UART1 (по умолчанию 1) */
#define UBX_KEY_NAVSPG_DYNMODEL      0x20110021U  /* E1: модель динамики */
#define UBX_KEY_TP_TP1_ENA           0x10050007U  /* L: таймпульс TP1 (1PPS); по умолчанию 1 */
#define UBX_KEY_TP_LEN_TP1           0x40050004U  /* U4, мкс: длина импульса до захвата времени (по умолчанию 0) */
#define UBX_KEY_TP_LEN_LOCK_TP1      0x40050005U  /* U4, мкс: длина импульса после захвата времени (по умолчанию 100000) */

/*
 * Энергосбережение (PURRGO_GNSS_PSM, см. hardware_config.h): группа CFG-PM,
 * UBX-M10 SPG 5.10 Interface Description, раздел 4.9.15.
 */
#define UBX_KEY_PM_OPERATEMODE       0x20D00001U  /* E1: 0 FULL, 1 PSMOO, 2 PSMCT */
#define UBX_KEY_PM_DONOTENTEROFF     0x10D00008U  /* L: не уходить в Inactive при потере фикса */

/* Набор систем (PURRGO_GNSS_SIGNALS, PSM): групповые ключи CFG-SIGNAL. */
#define UBX_KEY_SIGNAL_SBAS_ENA      0x10310020U  /* L */
#define UBX_KEY_SIGNAL_BDS_ENA       0x10310022U  /* L */
#define UBX_KEY_SIGNAL_GLO_ENA       0x10310025U  /* L */

/*
 * Параметры PSMOO для трекера (PURRGO_GNSS_TRACKER_PSMOO, см. hardware_config.h) и скорость UART.
 * Ключи: UBX-M10 SPG 5.10 Interface Description, группы CFG-PM и CFG-UART1.
 */
#define UBX_KEY_PM_POSUPDATEPERIOD   0x40D00002U  /* U4, с: период фиксов PSMOO (>= 5) */
#define UBX_KEY_PM_ACQPERIOD         0x40D00003U  /* U4, с: период повторного поиска после неудачи */
#define UBX_KEY_PM_ONTIME            0x30D00005U  /* U2, с: сколько оставаться в слежении после фикса */
#define UBX_KEY_UART1_BAUDRATE       0x40520001U  /* U4, бит/с */

#define UBX_PM_MODE_FULL  0U   /* CFG-PM-OPERATEMODE: нормальная работа */
#define UBX_PM_MODE_PSMOO 1U   /* ON/OFF */
#define UBX_PM_MODE_PSMCT 2U   /* циклическое слежение */

/*
 * Группа настройки: один UBX-CFG-VALSET. Имя нужно логу NAK (gnss_ubx.c приписывает ответ
 * группе по номеру), слой - маска RAM/BBR/Flash. Имена и кадры собираются в одном месте, поэтому
 * разойтись по числу и порядку не могут.
 */
typedef struct {
    const char *name;
    const purrgo_ubx_cfg_item_t *items;
    size_t count;
    uint8_t layers;
    bool del;   /* true: UBX-CFG-VALDEL по ключам items[i].key (значения не используются) */
} cfg_group_t;

#define CFG_MAX_GROUPS 24U

static void add_group(cfg_group_t *groups, size_t *n, const char *name,
                      const purrgo_ubx_cfg_item_t *items, size_t count, uint8_t layers)
{
    if (*n < CFG_MAX_GROUPS) {
        groups[*n].name = name;
        groups[*n].items = items;
        groups[*n].count = count;
        groups[*n].layers = layers;
        groups[*n].del = false;
        (*n)++;
    }
}

/*
 * Текстовая расшифровка отправленных команд для лога: "GNSS: TX NMEA/rate VALSET[RAM] RATE-MEAS=1000 ... (54 B)".
 * Имена ключей совпадают с UBX_KEY_* выше; неизвестный ключ печатается числом.
 */
typedef struct {
    uint32_t key;
    const char *name;
} key_name_t;

static const key_name_t k_key_names[] = {
    { UBX_KEY_RATE_MEAS,            "RATE-MEAS" },
    { UBX_KEY_RATE_NAV,             "RATE-NAV" },
    { UBX_KEY_MSGOUT_GGA_UART1,     "MSGOUT-GGA" },
    { UBX_KEY_MSGOUT_GLL_UART1,     "MSGOUT-GLL" },
    { UBX_KEY_MSGOUT_GSA_UART1,     "MSGOUT-GSA" },
    { UBX_KEY_MSGOUT_GSV_UART1,     "MSGOUT-GSV" },
    { UBX_KEY_MSGOUT_RMC_UART1,     "MSGOUT-RMC" },
    { UBX_KEY_MSGOUT_VTG_UART1,     "MSGOUT-VTG" },
    { UBX_KEY_MSGOUT_PVT_UART1,     "MSGOUT-NAV-PVT" },
    { UBX_KEY_MSGOUT_ODO_UART1,     "MSGOUT-NAV-ODO" },
    { UBX_KEY_MSGOUT_MON_RF_UART1,  "MSGOUT-MON-RF" },
    { UBX_KEY_MSGOUT_DOP_UART1,     "MSGOUT-NAV-DOP" },
    { UBX_KEY_MSGOUT_STATUS_UART1,  "MSGOUT-NAV-STATUS" },
    { UBX_KEY_MSGOUT_SAT_UART1,     "MSGOUT-NAV-SAT" },
    { UBX_KEY_MSGOUT_SEC_SIG_UART1, "MSGOUT-SEC-SIG" },
    { UBX_KEY_ITFM_ENABLE,          "ITFM-ENABLE" },
    { UBX_KEY_ODO_USE_ODO,          "ODO-USE" },
    { UBX_KEY_ODO_PROFILE,          "ODO-PROFILE" },
    { UBX_KEY_UART1OUTPROT_NMEA,    "UART1OUTPROT-NMEA" },
    { UBX_KEY_NAVSPG_DYNMODEL,      "DYNMODEL" },
    { UBX_KEY_TP_TP1_ENA,           "TP1-ENA" },
    { UBX_KEY_TP_LEN_TP1,           "TP-LEN" },
    { UBX_KEY_TP_LEN_LOCK_TP1,      "TP-LEN-LOCK" },
    { UBX_KEY_PM_OPERATEMODE,       "PM-OPERATEMODE" },
    { UBX_KEY_PM_DONOTENTEROFF,     "PM-DONOTENTEROFF" },
    { UBX_KEY_PM_POSUPDATEPERIOD,   "PM-POSUPDATEPERIOD" },
    { UBX_KEY_PM_ACQPERIOD,         "PM-ACQPERIOD" },
    { UBX_KEY_PM_ONTIME,            "PM-ONTIME" },
    { UBX_KEY_SIGNAL_SBAS_ENA,      "SIGNAL-SBAS" },
    { UBX_KEY_SIGNAL_BDS_ENA,       "SIGNAL-BDS" },
    { UBX_KEY_SIGNAL_GLO_ENA,       "SIGNAL-GLO" },
    { UBX_KEY_UART1_BAUDRATE,       "UART1-BAUDRATE" },
};

static const char *key_name(uint32_t key)
{
    for (size_t i = 0U; i < sizeof k_key_names / sizeof k_key_names[0]; i++) {
        if (k_key_names[i].key == key) {
            return k_key_names[i].name;
        }
    }
    return NULL;
}

/* Значение PM-OPERATEMODE словом, остальные - числом. */
static const char *pm_mode_name(uint32_t v)
{
    switch (v) {
        case UBX_PM_MODE_FULL:  return "FULL";
        case UBX_PM_MODE_PSMOO: return "PSMOO";
        case UBX_PM_MODE_PSMCT: return "PSMCT";
        default:                return NULL;
    }
}

/* Дописать в buf (размер size, текущая длина *pos) отформатированный текст; при нехватке места обрезает. */
static void log_append(char *buf, size_t size, size_t *pos, const char *fmt, ...) PURRGO_PRINTF_FORMAT(4, 5);

static void log_append(char *buf, size_t size, size_t *pos, const char *fmt, ...)
{
    va_list ap;

    if (*pos >= size - 1U) {
        return;
    }
    va_start(ap, fmt);
    const int n = purrgo_vsnprintf(buf + *pos, size - *pos, fmt, ap);
    va_end(ap);
    if (n > 0) {
        *pos += ((size_t)n < size - *pos) ? (size_t)n : (size - *pos - 1U);
    }
}

static void log_layers(char *buf, size_t size, size_t *pos, uint8_t layers)
{
    const char *sep = "";

    log_append(buf, size, pos, "[");
    if ((layers & 0x01U) != 0U) {
        log_append(buf, size, pos, "%sRAM", sep);
        sep = "+";
    }
    if ((layers & 0x02U) != 0U) {
        log_append(buf, size, pos, "%sBBR", sep);
        sep = "+";
    }
    if ((layers & 0x04U) != 0U) {
        log_append(buf, size, pos, "%sFlash", sep);
    }
    log_append(buf, size, pos, "]");
}

static void log_group(const cfg_group_t *g, size_t frame_len)
{
#if PURRGO_LOG_LEVEL >= PURRGO_LOG_LEVEL_INFO
    char line[200];
    size_t pos = 0U;

    log_append(line, sizeof line, &pos, "GNSS: TX %s %s", g->name, g->del ? "VALDEL" : "VALSET");
    log_layers(line, sizeof line, &pos, g->layers);
    for (size_t i = 0U; i < g->count; i++) {
        const char *name = key_name(g->items[i].key);

        if (name == NULL) {
            log_append(line, sizeof line, &pos, " 0x%08lX", (unsigned long)g->items[i].key);
        } else {
            log_append(line, sizeof line, &pos, " %s", name);
        }
        if (!g->del) {
            const char *vname = (g->items[i].key == UBX_KEY_PM_OPERATEMODE)
                                    ? pm_mode_name(g->items[i].value) : NULL;

            if (vname != NULL) {
                log_append(line, sizeof line, &pos, "=%s", vname);
            } else {
                log_append(line, sizeof line, &pos, "=%lu", (unsigned long)g->items[i].value);
            }
        }
    }
    PURRGO_LOGI("%s (%u B)\r\n", line, (unsigned)frame_len);
#else
    (void)g;
    (void)frame_len;
#endif
}

static void send_group(purrgo_gnss_tx_fn tx, const cfg_group_t *g)
{
    uint8_t frame[96];
    size_t len;

    if (g->del) {
        uint32_t keys[8];
        const size_t n = (g->count <= 8U) ? g->count : 8U;

        for (size_t i = 0U; i < n; i++) {
            keys[i] = g->items[i].key;
        }
        len = purrgo_ubx_build_valdel(frame, sizeof frame, g->layers, keys, n);
    } else {
        len = purrgo_ubx_build_valset(frame, sizeof frame, g->layers, g->items, g->count);
    }

    if (len == 0U) {
        PURRGO_LOGE("GNSS: UBX-CFG-VALSET build failed (%s)\r\n", g->name);
        return;
    }
    log_group(g, len);
    tx(frame, len);
}

void purrgo_gnss_receiver_configure_ubx_profile(purrgo_gnss_tx_fn tx, purrgo_gnss_profile_t profile,
                                                uint32_t uart_baud)
{
    /*
     * Профиль трекера в режиме PSMCT (PURRGO_GNSS_TRACKER_MODE_CT): те же кадры, что в навигации, но решения
     * реже (PURRGO_GNSS_TRACKER_PERIOD_S); настройка только в RAM.
     */
#if PURRGO_GNSS_TRACKER_PSMCT_ACTIVE
    const bool tracker_ct = (profile == PURRGO_GNSS_PROFILE_TRACKER);
#else
    const bool tracker_ct = false;
#endif
    (void)tracker_ct;
    uint32_t rate_ms = (uint32_t)PURRGO_GNSS_UPDATE_PERIOD_MS;

#if PURRGO_GNSS_TRACKER_PSMCT_ACTIVE
    if (tracker_ct) {
        rate_ms = (uint32_t)PURRGO_GNSS_TRACKER_PERIOD_S * 1000U;
    }
#endif
    const purrgo_ubx_cfg_item_t items[] = {
        { UBX_KEY_RATE_MEAS,        rate_ms },
        { UBX_KEY_RATE_NAV,         1U },
        { UBX_KEY_MSGOUT_GGA_UART1, 1U },
        { UBX_KEY_MSGOUT_RMC_UART1, 1U },
        { UBX_KEY_MSGOUT_GSA_UART1, 1U },
        { UBX_KEY_MSGOUT_GLL_UART1, 0U },
        { UBX_KEY_MSGOUT_GSV_UART1, 0U },
        { UBX_KEY_MSGOUT_VTG_UART1, 0U },
    };
    cfg_group_t groups[CFG_MAX_GROUPS];
    size_t n = 0U;

    if (tx == NULL) {
        return;
    }

    /*
     * Профиль трекера: PSMOO. В состоянии "off" RAM приёмника очищается, поэтому вся настройка
     * уходит ещё и в BBR (иначе после пробуждения пропали бы скорость UART, набор сообщений и сам
     * режим). В остальных случаях профиль - навигация (по умолчанию слой PURRGO_GNSS_UBX_LAYERS).
     */
#if PURRGO_GNSS_TRACKER_PSMOO_ACTIVE
    const bool tracker = (profile == PURRGO_GNSS_PROFILE_TRACKER);
    const bool after_tracker = (profile == PURRGO_GNSS_PROFILE_NAV_AFTER_TRACKER);
#else
    const bool tracker = false;
    const bool after_tracker = false;
    (void)uart_baud;
#endif
    (void)after_tracker;
    (void)profile;
    const uint8_t layers = tracker ? (uint8_t)PURRGO_GNSS_UBX_LAYERS_PERSIST
                                   : (uint8_t)PURRGO_GNSS_UBX_LAYERS;
#if PURRGO_GNSS_PSM_ANY
    /*
     * PM-ключи: в профиле трекера - RAM и BBR (PSMOO обязан пережить состояние "off"), в навигации - только
     * основной слой (PURRGO_GNSS_UBX_LAYERS). На железе PSMCT в BBR приёмник отклонил (NAK на OPERATEMODE = 2),
     * а в RAM принимает. Режим PSMOO из BBR после трекера убирает кадр "PM BBR clean" (UBX-CFG-VALDEL).
     */
    const uint8_t pm_layers = tracker ? (uint8_t)PURRGO_GNSS_UBX_LAYERS_PERSIST
                                      : (uint8_t)PURRGO_GNSS_UBX_LAYERS;
#endif

    /* 1. Частота решений и набор NMEA (как и без UBX-расширений). */
    add_group(groups, &n, "NMEA/rate", items, sizeof items / sizeof items[0], layers);

#if PURRGO_GNSS_SIGNAL_FRAME
    /*
     * Набор систем: отдельной командой, сразу за базовой. Приёмник перезапускает ГНСС
     * после смены набора, поэтому делаем это в начале настройки. В режиме
     * энергосбережения SBAS не принимается (manual рекомендует его выключить), а BeiDou
     * B1C не поддержан: оба выключаются (см. hardware_config.h).
     */
    static const purrgo_ubx_cfg_item_t signals[] = {
#if PURRGO_GNSS_SIG_OFF_SBAS
        { UBX_KEY_SIGNAL_SBAS_ENA, 0U },
#endif
#if PURRGO_GNSS_SIG_OFF_GLO
        { UBX_KEY_SIGNAL_GLO_ENA, 0U },
#endif
#if PURRGO_GNSS_SIG_OFF_BDS
        { UBX_KEY_SIGNAL_BDS_ENA, 0U },
#endif
    };
    add_group(groups, &n, "Signals", signals, sizeof signals / sizeof signals[0], layers);
#endif

#if PURRGO_GNSS_UBX
    /*
     * 2. Бинарные сообщения раз в эпоху (1 Гц при PURRGO_GNSS_UPDATE_PERIOD_MS = 1000).
     * Каждое - отдельным VALSET: если прошивка не знает ключа (SEC-SIG нет в SPG 5.10),
     * NAK получает только он, а NAV-PVT, NAV-ODO и MON-RF остаются включёнными.
     */
    static const purrgo_ubx_cfg_item_t msg_pvt = { UBX_KEY_MSGOUT_PVT_UART1,    1U };
    static const purrgo_ubx_cfg_item_t msg_odo = { UBX_KEY_MSGOUT_ODO_UART1,    1U };
    static const purrgo_ubx_cfg_item_t msg_rf  = { UBX_KEY_MSGOUT_MON_RF_UART1, 1U };
    /* HDOP/VDOP/PDOP: без NMEA (GGA/GSA) их источник - UBX-NAV-DOP. */
    static const purrgo_ubx_cfg_item_t msg_dop = { UBX_KEY_MSGOUT_DOP_UART1,    1U };
#if PURRGO_GNSS_NAV_STATUS
    static const purrgo_ubx_cfg_item_t msg_status = { UBX_KEY_MSGOUT_STATUS_UART1, 1U };
#endif
#if PURRGO_GNSS_NAV_SAT
    static const purrgo_ubx_cfg_item_t msg_sat = { UBX_KEY_MSGOUT_SAT_UART1, 1U };
#endif
#if PURRGO_GNSS_ITFM
    static const purrgo_ubx_cfg_item_t itfm = { UBX_KEY_ITFM_ENABLE, 1U };
#endif
    /* 3. Одометр: без USE_ODO UBX-NAV-ODO отдаёт нули. */
    static const purrgo_ubx_cfg_item_t odo[] = {
        { UBX_KEY_ODO_USE_ODO, 1U },
        { UBX_KEY_ODO_PROFILE, (uint32_t)PURRGO_GNSS_ODO_PROFILE },
    };
    /* 4. Модель динамики (модуль может отклонить неподдерживаемое значение). */
    static const purrgo_ubx_cfg_item_t dyn[] = {
        { UBX_KEY_NAVSPG_DYNMODEL, (uint32_t)PURRGO_GNSS_DYNMODEL },
    };

    add_group(groups, &n, "NAV-PVT", &msg_pvt, 1U, layers);
    add_group(groups, &n, "NAV-ODO", &msg_odo, 1U, layers);
    add_group(groups, &n, "MON-RF", &msg_rf, 1U, layers);
    add_group(groups, &n, "NAV-DOP", &msg_dop, 1U, layers);
#if PURRGO_GNSS_SEC_SIG
    static const purrgo_ubx_cfg_item_t msg_sig = { UBX_KEY_MSGOUT_SEC_SIG_UART1, 1U };
    add_group(groups, &n, "SEC-SIG", &msg_sig, 1U, layers);
#endif
    /* Замена SEC-SIG и контроля антенны в SPG 5.10 (см. hardware_config.h). */
#if PURRGO_GNSS_NAV_STATUS
    add_group(groups, &n, "NAV-STATUS", &msg_status, 1U, layers);
#endif
#if PURRGO_GNSS_NAV_SAT
    add_group(groups, &n, "NAV-SAT", &msg_sat, 1U, layers);
#endif
#if PURRGO_GNSS_ITFM
    add_group(groups, &n, "ITFM", &itfm, 1U, layers);
#endif
    add_group(groups, &n, "ODO enable/profile", odo, sizeof odo / sizeof odo[0], layers);
    add_group(groups, &n, "DYNMODEL", dyn, sizeof dyn / sizeof dyn[0], layers);
#if PURRGO_GNSS_TIMEPULSE == PURRGO_GNSS_TIMEPULSE_QUIET
    /*
     * 4a. Таймпульс (1PPS) остаётся включённым, но без импульсов (длина 0 и до, и после захвата
     * времени): прошивка 1PPS не использует, а светодиод PPS модуля тратит ток. Выключение через
     * TP1_ENA = 0 светодиод не гасит (вывод отпускается). Отдельный кадр: при NAK остальная
     * настройка не страдает.
     */
    static const purrgo_ubx_cfg_item_t tp_quiet[] = {
        { UBX_KEY_TP_TP1_ENA,      1U },
        { UBX_KEY_TP_LEN_TP1,      0U },
        { UBX_KEY_TP_LEN_LOCK_TP1, 0U },
    };

    add_group(groups, &n, "TIMEPULSE quiet", tp_quiet, sizeof tp_quiet / sizeof tp_quiet[0], layers);
#elif PURRGO_GNSS_TIMEPULSE == PURRGO_GNSS_TIMEPULSE_DISABLED
    /* 4a. Таймпульс выключен (CFG-TP-TP1_ENA = 0). */
    static const purrgo_ubx_cfg_item_t tp_off = { UBX_KEY_TP_TP1_ENA, 0U };

    add_group(groups, &n, "TIMEPULSE off", &tp_off, 1U, layers);
#endif
#endif

#if PURRGO_GNSS_TRACKER_PSMOO_ACTIVE
    /*
     * 4b. Скорость UART в слой BBR (только профиль трекера): после пробуждения из "off" RAM пуста, и
     * без этого модуль вернулся бы на заводскую скорость, а UART МК остался на рабочей. Значение то
     * же, что уже установлено (команда смены скорости применяется без перезапуска).
     */
    purrgo_ubx_cfg_item_t baud_item = { UBX_KEY_UART1_BAUDRATE, uart_baud };

    if (tracker && uart_baud != 0U) {
        add_group(groups, &n, "UART baud", &baud_item, 1U, layers);
    }
#endif

#if PURRGO_GNSS_NMEA_OFF
    /*
     * 5. NMEA выключается ПОСЛЕ определения скорости UART (gnss_link искал модуль по NMEA заводской
     * настройки) и после остальной настройки, до PSM (его кадры идут самыми последними). Отдельным
     * VALSET: ответ ACK/NAK приходит в UBX и при выключенном NMEA. При NAK останется сокращённый
     * NMEA (GGA, RMC, GSA из базового кадра) - ядро продолжит работать по запасному пути.
     * Повторная настройка (повтор через gnss_link) идемпотентна.
     */
    static const purrgo_ubx_cfg_item_t nmea_off = { UBX_KEY_UART1OUTPROT_NMEA, 0U };

    add_group(groups, &n, "NMEA off", &nmea_off, 1U, layers);
#endif

#if PURRGO_GNSS_TRACKER_PSMOO_ACTIVE
    if (after_tracker) {
        /* Параметры PSMOO, записанные профилем трекера в BBR (ключи те же, что в профиле трекера). */
        static const purrgo_ubx_cfg_item_t pm_bbr_keys[] = {
            { UBX_KEY_PM_OPERATEMODE, 0U },
            { UBX_KEY_PM_POSUPDATEPERIOD, 0U },
            { UBX_KEY_PM_ACQPERIOD, 0U },
            { UBX_KEY_PM_ONTIME, 0U },
            { UBX_KEY_PM_DONOTENTEROFF, 0U },
        };

        add_group(groups, &n, "PM BBR clean", pm_bbr_keys, sizeof pm_bbr_keys / sizeof pm_bbr_keys[0], 0x02U);
        groups[n - 1U].del = true;
    }
#endif

#if PURRGO_GNSS_PSM_ANY
    /*
     * 6. Режим энергосбережения - после всей остальной настройки, чтобы смена режима не мешала ей.
     * OPERATEMODE - последним ключом (для PSMOO обязательно: иначе приёмник мог бы уйти в "off", не
     * дождавшись остальных ключей). Все кадры - отдельные VALSET: отказ оставит приёмник в
     * прежнем режиме, остальная настройка не пострадает. PM-ключи идут в слои RAM и BBR.
     */
#if PURRGO_GNSS_TRACKER_PSMOO_ACTIVE
    if (tracker) {
        /*
         * PSMOO для трекера: период фиксов и поиска, слежение после фикса. DONOTENTEROFF = 0:
         * без сигнала приёмник засыпает и повторяет поиск через ACQPERIOD, а не жжёт ток непрерывным
         * поиском (в навигации он включён). Сетка периодов привязана к неделе GPS (GRIDOFFSET = 0).
         */
        static const purrgo_ubx_cfg_item_t psmoo_params[] = {
            { UBX_KEY_PM_POSUPDATEPERIOD, (uint32_t)PURRGO_GNSS_TRACKER_PERIOD_S },
            { UBX_KEY_PM_ACQPERIOD,       (uint32_t)PURRGO_GNSS_TRACKER_PERIOD_S },
            { UBX_KEY_PM_ONTIME,          (uint32_t)PURRGO_GNSS_TRACKER_ONTIME_S },
            { UBX_KEY_PM_DONOTENTEROFF,   0U },
        };
        static const purrgo_ubx_cfg_item_t psmoo_mode = { UBX_KEY_PM_OPERATEMODE, UBX_PM_MODE_PSMOO };

        add_group(groups, &n, "PSMOO params", psmoo_params,
                  sizeof psmoo_params / sizeof psmoo_params[0], pm_layers);
        add_group(groups, &n, "PSMOO mode", &psmoo_mode, 1U, pm_layers);
    } else
#endif
    {
        /*
         * Циклическое слежение (PSMCT): в навигации при PURRGO_GNSS_PSM, в профиле трекера - в режиме
         * PURRGO_GNSS_TRACKER_MODE_CT. DONOTENTEROFF = 1: при потере фикса поиск продолжается, а поток данных
         * не пропадает, приёмник не уходит в "off", настройка в слое RAM не стирается. Период решений PSMCT
         * задаёт CFG-RATE-MEAS из базового кадра.
         */
        bool psmct = tracker_ct;
#if PURRGO_GNSS_PSM_ACTIVE
        psmct = true;
#endif
        if (psmct) {
            static const purrgo_ubx_cfg_item_t psm_no_off = { UBX_KEY_PM_DONOTENTEROFF, 1U };
            static const purrgo_ubx_cfg_item_t psm_mode   = { UBX_KEY_PM_OPERATEMODE, UBX_PM_MODE_PSMCT };

            add_group(groups, &n, "PSM no-off", &psm_no_off, 1U, pm_layers);
            add_group(groups, &n, "PSM mode", &psm_mode, 1U, pm_layers);
        } else {
            /* PSMCT в навигации выключен, но в трекере режим энергосбережения возможен: возврат к полной мощности. */
            static const purrgo_ubx_cfg_item_t psm_full = { UBX_KEY_PM_OPERATEMODE, UBX_PM_MODE_FULL };

            add_group(groups, &n, "PSM mode", &psm_full, 1U, pm_layers);
        }
    }
#endif

#if PURRGO_GNSS_UBX
    /*
     * Имена групп в порядке отправки: ответы ACK/NAK на VALSET приходят по порядку, и
     * gnss_ubx.c по ним пишет в лог, какая именно группа отклонена. Вызывать до первой
     * отправки: ответ на первый кадр может прийти сразу.
     */
    static const char *group_names[CFG_MAX_GROUPS];

    for (size_t i = 0U; i < n; i++) {
        group_names[i] = groups[i].name;
    }
    purrgo_gnss_ubx_cfg_begin(group_names, (uint8_t)n);
#endif

    for (size_t i = 0U; i < n; i++) {
        send_group(tx, &groups[i]);
    }

#if PURRGO_GNSS_UBX
    /* 7. Опрос версии прошивки: ответ UBX-MON-VER разбирает gnss_ubx.c и пишет в лог. */
    uint8_t ver[8];
    const size_t ver_len = purrgo_ubx_build_frame(ver, sizeof ver, UBX_CLASS_MON, UBX_ID_MON_VER,
                                                  NULL, 0U);
    if (ver_len != 0U) {
        PURRGO_LOGI("GNSS: TX poll UBX-MON-VER (%u B)\r\n", (unsigned)ver_len);
        tx(ver, ver_len);
    }
#endif
}

void purrgo_gnss_receiver_configure_ubx(purrgo_gnss_tx_fn tx)
{
    purrgo_gnss_receiver_configure_ubx_profile(tx, PURRGO_GNSS_PROFILE_NAV, 0U);
}

/*
 * Смена скорости UART1 модуля: UBX-CFG-VALSET с ключом CFG-UART1-BAUDRATE
 * (U4, бит/с). Слой - PURRGO_GNSS_UBX_LAYERS (по умолчанию RAM: после
 * выключения по EN модуль снова стартует на заводской скорости).
 * Подтверждение (ACK) модуль может отправить уже на новой скорости, поэтому
 * успех смены определяется по NMEA на новой скорости (gnss_link), а не по ACK.
 */

size_t purrgo_gnss_receiver_build_baud(uint8_t *out, size_t out_size, uint32_t baud)
{
#if (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_UBX)
    const purrgo_ubx_cfg_item_t item = { UBX_KEY_UART1_BAUDRATE, baud };

    if (baud == 0U) {
        return 0U;
    }
    return purrgo_ubx_build_valset(out, out_size, (uint8_t)PURRGO_GNSS_UBX_LAYERS, &item, 1U);
#else
    (void)out;
    (void)out_size;
    (void)baud;
    return 0U;
#endif
}

void purrgo_gnss_receiver_configure(purrgo_gnss_tx_fn tx)
{
    purrgo_gnss_receiver_configure_profile(tx, PURRGO_GNSS_PROFILE_NAV, 0U);
}

void purrgo_gnss_receiver_configure_profile(purrgo_gnss_tx_fn tx, purrgo_gnss_profile_t profile,
                                            uint32_t uart_baud)
{
#if (PURRGO_HW_GNSS_RECEIVER == PURRGO_GNSS_RECEIVER_UBX)
    purrgo_gnss_receiver_configure_ubx_profile(tx, profile, uart_baud);
#else
    (void)tx;
    (void)profile;
    (void)uart_baud;
#endif
}
