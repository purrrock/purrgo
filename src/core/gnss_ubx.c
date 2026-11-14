#include "purrgo/gnss_ubx.h"

#include "purrgo/gnss_adapter.h"
#include "purrgo/gnss_io.h"
#include "purrgo/gnss_receiver.h"
#include "purrgo/logger.h"

#include <string.h>

/* ------------------------------------------------------------------------- */
/* Разбор кадров                                                             */
/* ------------------------------------------------------------------------- */

void purrgo_ubx_parser_init(purrgo_ubx_parser_t *parser)
{
    parser->state = PURRGO_UBX_PARSER_IDLE;
    parser->msg_class = 0U;
    parser->msg_id = 0U;
    parser->length = 0U;
    parser->pos = 0U;
    parser->ck_a = 0U;
    parser->ck_b = 0U;
}

bool purrgo_ubx_parser_active(const purrgo_ubx_parser_t *parser)
{
    return parser->state != PURRGO_UBX_PARSER_IDLE;
}

static void ck_add(purrgo_ubx_parser_t *p, uint8_t byte)
{
    p->ck_a = (uint8_t)(p->ck_a + byte);
    p->ck_b = (uint8_t)(p->ck_b + p->ck_a);
}

purrgo_ubx_feed_t purrgo_ubx_parser_feed(purrgo_ubx_parser_t *p, uint8_t byte)
{
    switch (p->state) {
    case PURRGO_UBX_PARSER_IDLE:
        if (byte == 0xB5U) {
            p->state = PURRGO_UBX_PARSER_SYNC2;
            return PURRGO_UBX_FEED_CONSUMED;
        }
        return PURRGO_UBX_FEED_NOT_UBX;

    case PURRGO_UBX_PARSER_SYNC2:
        if (byte == 0x62U) {
            p->state = PURRGO_UBX_PARSER_CLASS;
            p->ck_a = 0U;
            p->ck_b = 0U;
            return PURRGO_UBX_FEED_CONSUMED;
        }
        if (byte == 0xB5U) {
            return PURRGO_UBX_FEED_CONSUMED;     /* B5 B5 62: первый был мусором */
        }
        p->state = PURRGO_UBX_PARSER_IDLE;
        return PURRGO_UBX_FEED_NOT_UBX;

    case PURRGO_UBX_PARSER_CLASS:
        p->msg_class = byte;
        ck_add(p, byte);
        p->state = PURRGO_UBX_PARSER_ID;
        return PURRGO_UBX_FEED_CONSUMED;

    case PURRGO_UBX_PARSER_ID:
        p->msg_id = byte;
        ck_add(p, byte);
        p->state = PURRGO_UBX_PARSER_LEN1;
        return PURRGO_UBX_FEED_CONSUMED;

    case PURRGO_UBX_PARSER_LEN1:
        p->length = byte;
        ck_add(p, byte);
        p->state = PURRGO_UBX_PARSER_LEN2;
        return PURRGO_UBX_FEED_CONSUMED;

    case PURRGO_UBX_PARSER_LEN2:
        p->length = (uint16_t)(p->length | ((uint16_t)byte << 8));
        if (p->length > PURRGO_UBX_MAX_PAYLOAD) {
            p->state = PURRGO_UBX_PARSER_IDLE;   /* слишком длинный (или мусор): не ждём его конца */
            return PURRGO_UBX_FEED_NOT_UBX;
        }
        ck_add(p, byte);
        p->pos = 0U;
        p->state = (p->length == 0U) ? PURRGO_UBX_PARSER_CK_A : PURRGO_UBX_PARSER_PAYLOAD;
        return PURRGO_UBX_FEED_CONSUMED;

    case PURRGO_UBX_PARSER_PAYLOAD:
        p->payload[p->pos++] = byte;
        ck_add(p, byte);
        if (p->pos >= p->length) {
            p->state = PURRGO_UBX_PARSER_CK_A;
        }
        return PURRGO_UBX_FEED_CONSUMED;

    case PURRGO_UBX_PARSER_CK_A:
        if (byte == p->ck_a) {
            p->state = PURRGO_UBX_PARSER_CK_B;
        } else {
            p->state = PURRGO_UBX_PARSER_IDLE;
        }
        return PURRGO_UBX_FEED_CONSUMED;

    case PURRGO_UBX_PARSER_CK_B:
    default:
        p->state = PURRGO_UBX_PARSER_IDLE;
        return (byte == p->ck_b) ? PURRGO_UBX_FEED_FRAME : PURRGO_UBX_FEED_CONSUMED;
    }
}

/* ------------------------------------------------------------------------- */
/* Чтение полей (little-endian)                                              */
/* ------------------------------------------------------------------------- */

static uint16_t rd_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t rd_i32(const uint8_t *p)
{
    return (int32_t)rd_u32(p);
}

/* ------------------------------------------------------------------------- */
/* Состояние                                                                 */
/* ------------------------------------------------------------------------- */

#define PVT_LENGTH             92U
#define ODO_LENGTH             20U
#define RF_BLOCK_LENGTH        24U
#define STATUS_LENGTH          16U
#define SAT_HEADER_LENGTH      8U
#define SAT_BLOCK_LENGTH       12U
#define VER_LOG_INTERVAL_MS    10000U   /* повторные ответы на опрос в лог не дублируются */
#define ODO_RESET_HOLD_MS      1500U    /* столько кадров ODO после RESETODO считаются устаревшими */

/* Курс из PVT (headMot) надёжен только при движении и приличной оценке. */
#define COURSE_MIN_SPEED_MM_S  500
#define COURSE_MAX_HEADACC_1E5 4500000U /* 45 градусов */

static purrgo_gnss_ubx_status_t s_st;

/* Дистанция трека: значения одометра модуля. */
static bool s_odo_have;
static uint32_t s_odo_last;
static uint32_t s_odo_base;
static bool s_odo_pending_base;
static bool s_odo_hold;
static uint32_t s_odo_hold_until;

static bool s_ver_logged;
static uint32_t s_ver_logged_ms;

/* Сопоставление ответов ACK/NAK на VALSET группам настройки. */
static const char *const *s_cfg_names;
static uint8_t s_cfg_count;
static uint8_t s_cfg_replies;
static uint32_t s_cfg_nak_mask;

/* Периодическая диагностическая строка по PVT. */
#define PVT_LOG_INTERVAL_MS 5000U
static bool s_pvt_logged;
static uint32_t s_pvt_logged_ms;

/*
 * Диагностика MON-RF, NAV-STATUS и NAV-SAT (только PURRGO_LOGD, то есть в сборке Debug):
 * строка раз в PVT_LOG_INTERVAL_MS и сразу при смене состояния глушения/спуфинга - так на
 * железе видно, какие значения выдаёт именно эта прошивка модуля.
 */
static bool s_rf_logged;
static uint32_t s_rf_logged_ms;
static bool s_status_logged;
static uint32_t s_status_logged_ms;
static bool s_sat_logged;
static uint32_t s_sat_logged_ms;

static bool log_due(bool *logged, uint32_t *logged_ms, uint32_t now_ms)
{
    if (*logged && (uint32_t)(now_ms - *logged_ms) < PVT_LOG_INTERVAL_MS) {
        return false;
    }
    *logged = true;
    *logged_ms = now_ms;
    return true;
}

void purrgo_gnss_ubx_init(void)
{
    memset(&s_st, 0, sizeof s_st);
    s_odo_have = false;
    s_odo_last = 0U;
    s_odo_base = 0U;
    s_odo_pending_base = false;
    s_odo_hold = false;
    s_odo_hold_until = 0U;
    s_ver_logged = false;
    s_ver_logged_ms = 0U;
    s_cfg_names = NULL;
    s_cfg_count = 0U;
    s_cfg_replies = 0U;
    s_cfg_nak_mask = 0U;
    s_pvt_logged = false;
    s_pvt_logged_ms = 0U;
    s_rf_logged = false;
    s_rf_logged_ms = 0U;
    s_status_logged = false;
    s_status_logged_ms = 0U;
    s_sat_logged = false;
    s_sat_logged_ms = 0U;
}

void purrgo_gnss_ubx_cfg_begin(const char *const *names, uint8_t count)
{
    s_cfg_names = names;
    s_cfg_count = (count > PURRGO_UBX_CFG_MAX_TRACKED) ? (uint8_t)PURRGO_UBX_CFG_MAX_TRACKED : count;
    s_cfg_replies = 0U;
    s_cfg_nak_mask = 0U;
}

const char *purrgo_gnss_ubx_cfg_name(uint8_t index)
{
    return (s_cfg_names != NULL && index < s_cfg_count) ? s_cfg_names[index] : NULL;
}

uint32_t purrgo_gnss_ubx_cfg_nak_mask(void)
{
    return s_cfg_nak_mask;
}

uint8_t purrgo_gnss_ubx_cfg_replies(void)
{
    return s_cfg_replies;
}

/* Ответ приёмника на UBX-CFG-VALSET (класс 0x06, id 0x8A): ACK или NAK. */
static void process_cfg_reply(bool nak, const uint8_t *payload)
{
    if (payload[0] != 0x06U || payload[1] != 0x8AU) {
        if (nak) {
            PURRGO_LOGW("GNSS: module rejected UBX class 0x%02X id 0x%02X (NAK)\r\n",
                        (unsigned)payload[0], (unsigned)payload[1]);
        }
        return;
    }
    if (s_cfg_names != NULL && s_cfg_replies < s_cfg_count) {
        const uint8_t idx = s_cfg_replies++;

        if (nak) {
            s_cfg_nak_mask |= ((uint32_t)1U << idx);
            PURRGO_LOGW("GNSS: config \"%s\" rejected by module (NAK)\r\n", s_cfg_names[idx]);
        }
    } else if (nak) {
        PURRGO_LOGW("GNSS: module rejected UBX-CFG-VALSET (NAK)\r\n");
    }
}

const purrgo_gnss_ubx_status_t *purrgo_gnss_ubx_status(void)
{
    return &s_st;
}

bool purrgo_gnss_ubx_fresh(uint32_t msg_ms, uint32_t now_ms)
{
    /* Метка сообщения может быть новее now_ms (время такта взято до разбора байт): свежее. */
    const int32_t age = (int32_t)(now_ms - msg_ms);

    return age <= (int32_t)PURRGO_UBX_STALE_MS;
}

/* ------------------------------------------------------------------------- */
/* UBX-NAV-PVT                                                               */
/* ------------------------------------------------------------------------- */

bool purrgo_gnss_ubx_apply_pvt(const uint8_t *p, uint16_t length, purrgo_gnss_solution_t *sol)
{
    if (p == NULL || sol == NULL || length != PVT_LENGTH) {
        return false;
    }

    const uint8_t valid_flags = p[11];
    const uint8_t fix_type = p[20];
    const uint8_t flags = p[21];
    const uint16_t flags3 = rd_u16(&p[78]);
    s_st.psm_valid = true;
    s_st.psm_state = (uint8_t)((flags >> 2) & 0x07U);
    const bool fix_ok = ((flags & 0x01U) != 0U) && (fix_type >= 2U) && (fix_type <= 4U) &&
                        ((flags3 & 0x01U) == 0U);

    sol->valid = fix_ok;
    sol->fix_type = fix_ok ? ((fix_type == 2U) ? 2U : 3U) : 1U;
    sol->fix_quality = fix_ok ? (((flags & 0x02U) != 0U) ? 2U : 1U) : 0U;
    sol->satellites_tracked = p[23];
    sol->pdop_100 = (int32_t)rd_u16(&p[76]);

    if (!fix_ok) {
        sol->speed_knots = 0;
        sol->course_valid = false;
        sol->acc_valid = false;
        return true;
    }

    sol->lon_1e7 = rd_i32(&p[24]);
    sol->lat_1e7 = rd_i32(&p[28]);

    /* hMSL, мм -> м с округлением к ближайшему (в обе стороны симметрично). */
    const int32_t hmsl_mm = rd_i32(&p[36]);
    sol->alt_m = (hmsl_mm >= 0) ? (hmsl_mm + 500) / 1000 : -((-hmsl_mm + 500) / 1000);

    int32_t gspeed = rd_i32(&p[60]);
    if (gspeed < 0) {
        gspeed = 0;
    }
    /* мм/с -> сотые доли узла: x * 3600 / 1852 / 1000 * 100 = x * 360 / 1852. */
    sol->speed_knots = (int32_t)(((int64_t)gspeed * 360) / 1852);

    const int32_t head_mot = rd_i32(&p[64]);
    const uint32_t head_acc = rd_u32(&p[72]);
    if (gspeed >= COURSE_MIN_SPEED_MM_S && head_mot >= 0 && head_acc <= COURSE_MAX_HEADACC_1E5) {
        int32_t course = head_mot / 1000;          /* 1e-5 градуса -> сотые доли градуса */
        if (course >= 36000) {
            course -= 36000;
        }
        sol->course_deg_100 = course;
        sol->course_valid = true;
    } else {
        sol->course_valid = false;
    }

    if ((valid_flags & 0x01U) != 0U && (valid_flags & 0x02U) != 0U) {
        const uint16_t year = rd_u16(&p[4]);
        sol->year = (uint8_t)((year >= 2000U) ? (year - 2000U) : 0U);
        sol->month = p[6];
        sol->day = p[7];
        sol->hours = p[8];
        sol->minutes = p[9];
        sol->seconds = p[10];
    }

    const uint32_t hacc = rd_u32(&p[40]);
    sol->acc_valid = (hacc != 0xFFFFFFFFU);
    sol->hacc_mm = hacc;
    sol->vacc_mm = rd_u32(&p[44]);
    sol->sacc_mm_s = rd_u32(&p[68]);
    return true;
}

/* ------------------------------------------------------------------------- */
/* Остальные сообщения                                                       */
/* ------------------------------------------------------------------------- */

#define DOP_LENGTH 18U

/*
 * UBX-NAV-DOP: HDOP/VDOP/PDOP (масштаб 0,01 - как в решении). Единственный источник DOP, когда
 * NMEA выключен (PURRGO_GNSS_UBX_NMEA = 0); при включённом NMEA те же значения приходят из GGA/GSA.
 * Без решения приёмник отдаёт 99,99 - значение попадает в решение как есть.
 */
static void apply_dop(const uint8_t *p, uint16_t length, purrgo_gnss_solution_t *sol)
{
    if (sol == NULL || length != DOP_LENGTH) {
        return;
    }
    sol->pdop_100 = (int32_t)rd_u16(&p[6]);
    sol->vdop_100 = (int32_t)rd_u16(&p[10]);
    sol->hdop_100 = (int32_t)rd_u16(&p[12]);
}

static void process_odo(const uint8_t *p, uint16_t length, uint32_t now_ms)
{
    if (length != ODO_LENGTH || p[0] != 0U) {
        return;
    }

    if (s_odo_hold) {
        if ((int32_t)(now_ms - s_odo_hold_until) < 0) {
            return;     /* ещё может прийти значение до сброса */
        }
        s_odo_hold = false;
    }

    const uint32_t distance = rd_u32(&p[8]);

    if (s_odo_pending_base) {
        s_odo_base = distance;      /* сброс модуля не отправился: отсчёт от первого значения */
        s_odo_pending_base = false;
    } else if (s_odo_have && distance < s_odo_last) {
        s_odo_base = 0U;            /* счётчик модуля обнулился (RESETODO или холодный старт) */
    }
    s_odo_last = distance;
    s_odo_have = true;

    s_st.odo_valid = true;
    s_st.odo_distance_m = distance;
    s_st.odo_total_m = rd_u32(&p[12]);
    s_st.odo_std_m = rd_u32(&p[16]);
    s_st.odo_ms = now_ms;
}

static void process_sec_sig(const uint8_t *p, uint16_t length, uint32_t now_ms)
{
    if (length < 4U || p[0] != 2U) {      /* версия 2 - см. SPG 5.30 */
        return;
    }
    const uint8_t flags = p[1];

    s_st.jam_det_enabled = ((flags & 0x01U) != 0U);
    s_st.jam_state = (uint8_t)((flags >> 1) & 0x03U);
    s_st.spoof_det_enabled = ((flags & 0x08U) != 0U);
    s_st.spoof_state = (uint8_t)((flags >> 4) & 0x07U);
    s_st.sec_valid = true;
    s_st.sec_ms = now_ms;
}

static void process_mon_rf(const uint8_t *p, uint16_t length, uint32_t now_ms)
{
    if (length < 4U + RF_BLOCK_LENGTH || p[0] != 0U || p[1] < 1U) {
        return;
    }
    const uint8_t *b = &p[4];
    const uint8_t jam = (uint8_t)(b[1] & 0x03U);
    const bool jam_changed = s_st.rf_valid && jam != s_st.rf_jam_state;

    s_st.rf_jam_state = jam;
    s_st.ant_status = b[2];
    s_st.ant_power = b[3];
    s_st.noise_per_ms = rd_u16(&b[12]);
    s_st.agc_cnt = rd_u16(&b[14]);
    s_st.cw_suppression = b[16];
    s_st.rf_valid = true;
    s_st.rf_ms = now_ms;

    if (log_due(&s_rf_logged, &s_rf_logged_ms, now_ms) || jam_changed) {
        PURRGO_LOGD("GNSS: RF jam=%u ant=%u/%u noise=%u agc=%u cw=%u\r\n", (unsigned)jam,
                    (unsigned)b[2], (unsigned)b[3], (unsigned)s_st.noise_per_ms,
                    (unsigned)s_st.agc_cnt, (unsigned)s_st.cw_suppression);
    }
}

/*
 * UBX-NAV-STATUS (16 байт). flags2 (байт 7): биты 1..0 psmState, биты 4..3 spoofDetState
 * (0 неизвестно или выключено, 1 нет, 2 признаки, 3 несколько признаков).
 */
static void process_nav_status(const uint8_t *p, uint16_t length, uint32_t now_ms)
{
    if (length != STATUS_LENGTH) {
        return;
    }
    const uint8_t spoof = (uint8_t)((p[7] >> 3) & 0x03U);
    const bool spoof_changed = s_st.status_valid && spoof != s_st.status_spoof_state;

    s_st.status_spoof_state = spoof;
    s_st.status_valid = true;
    s_st.status_ms = now_ms;

    if (log_due(&s_status_logged, &s_status_logged_ms, now_ms) || spoof_changed) {
        PURRGO_LOGD("GNSS: STATUS gpsFix=%u flags=0x%02X psm=%u spoof=%u\r\n", (unsigned)p[4],
                    (unsigned)p[5], (unsigned)(p[7] & 0x03U), (unsigned)spoof);
    }
}

/*
 * UBX-NAV-SAT: 8 байт заголовка (iTOW, version = 1, numSvs) и по 12 байт на спутник
 * (gnssId, svId, cno, elev, azim, prRes, flags). Считаются спутники с svUsed (бит 3 flags)
 * и ненулевым cno: средний и максимальный C/N0, дБ-Гц.
 */
static void process_nav_sat(const uint8_t *p, uint16_t length, uint32_t now_ms)
{
    if (length < SAT_HEADER_LENGTH || p[4] != 1U) {
        return;
    }
    const uint8_t listed = p[5];

    if (length != (uint16_t)(SAT_HEADER_LENGTH + (uint16_t)listed * SAT_BLOCK_LENGTH)) {
        return;
    }

    uint32_t sum = 0U;
    uint8_t used = 0U;
    uint8_t cno_max = 0U;

    for (uint8_t i = 0U; i < listed; i++) {
        const uint8_t *sv = &p[SAT_HEADER_LENGTH + (uint16_t)i * SAT_BLOCK_LENGTH];
        const uint8_t cno = sv[2];

        if ((sv[8] & 0x08U) == 0U || cno == 0U) {      /* flags (X4, младший байт): svUsed */
            continue;
        }
        sum += cno;
        used++;
        if (cno > cno_max) {
            cno_max = cno;
        }
    }

    s_st.sat_listed = listed;
    s_st.sat_used = used;
    uint8_t cno_avg = 0U;
    if (used != 0U) {
        cno_avg = (uint8_t)((sum + used / 2U) / used);
    }
    s_st.sat_cno_avg = cno_avg;
    s_st.sat_cno_max = cno_max;
    s_st.sat_valid = true;
    s_st.sat_ms = now_ms;

    if (log_due(&s_sat_logged, &s_sat_logged_ms, now_ms)) {
        PURRGO_LOGD("GNSS: SAT listed=%u used=%u cno avg=%u max=%u dBHz\r\n", (unsigned)listed,
                    (unsigned)used, (unsigned)s_st.sat_cno_avg, (unsigned)cno_max);
    }
}

/* Поле фиксированной длины -> C-строка (в кадре строки заканчиваются нулём, но не обязаны). */
static void copy_field(char *dst, size_t dst_size, const uint8_t *src, size_t src_len)
{
    size_t n = 0U;

    while (n < src_len && n + 1U < dst_size && src[n] != 0U) {
        dst[n] = (char)src[n];
        n++;
    }
    dst[n] = '\0';
}

static void process_mon_ver(const uint8_t *p, uint16_t length, uint32_t now_ms)
{
    if (length < 40U) {
        return;
    }
    copy_field(s_st.sw_version, sizeof s_st.sw_version, &p[0], 30U);
    copy_field(s_st.hw_version, sizeof s_st.hw_version, &p[30], 10U);
    s_st.ver_valid = true;

    /* Ответы на повторные опросы (настройка отправляется несколько раз) не дублируем. */
    if (s_ver_logged && (uint32_t)(now_ms - s_ver_logged_ms) < VER_LOG_INTERVAL_MS) {
        return;
    }
    s_ver_logged = true;
    s_ver_logged_ms = now_ms;

    PURRGO_LOGI("GNSS: u-blox SW \"%s\" HW \"%s\"\r\n", s_st.sw_version, s_st.hw_version);

    for (uint16_t off = 40U; off + 30U <= length; off = (uint16_t)(off + 30U)) {
        char ext[31];

        copy_field(ext, sizeof ext, &p[off], 30U);
        if (ext[0] == '\0') {
            continue;
        }
        PURRGO_LOGI("GNSS:   %s\r\n", ext);

        /* Прошивка SPG 5.30 - протокол 34.xx; SPG 7.0x (F10) - 52.xx и другие ключи. */
        if (strncmp(ext, "PROTVER=", 8) == 0 && strncmp(&ext[8], "34.", 3) != 0) {
            PURRGO_LOGW("GNSS: protocol %s, UBX features are written for 34.xx (SPG 5.30)\r\n", &ext[8]);
        }
    }
}

bool purrgo_gnss_ubx_process_frame(uint8_t msg_class, uint8_t msg_id, const uint8_t *payload,
                                   uint16_t length, purrgo_gnss_solution_t *solution,
                                   uint32_t now_ms)
{
    if (payload == NULL) {
        return false;
    }

    if (msg_class == PURRGO_UBX_CLASS_NAV) {
        if (msg_id == PURRGO_UBX_ID_NAV_PVT) {
            if (!purrgo_gnss_ubx_apply_pvt(payload, length, solution)) {
                return false;
            }
            /* Раз в несколько секунд: видно, есть ли время без позиции (PPS мигает уже по времени). */
            if (!s_pvt_logged || (uint32_t)(now_ms - s_pvt_logged_ms) >= PVT_LOG_INTERVAL_MS) {
                s_pvt_logged = true;
                s_pvt_logged_ms = now_ms;
                PURRGO_LOGI("GNSS: PVT fixType=%u fixOK=%u sv=%u validTime=%u hAcc=%lu mm pDOP=%u\r\n",
                            (unsigned)payload[20], (unsigned)(payload[21] & 1U), (unsigned)payload[23],
                            (unsigned)((payload[11] >> 1) & 1U), (unsigned long)rd_u32(&payload[40]),
                            (unsigned)rd_u16(&payload[76]));
            }
            s_st.pvt_valid = true;
            s_st.pvt_fix_type = payload[20];
            s_st.pvt_fix_ok = solution->valid;
            s_st.pvt_time_valid = ((payload[11] >> 1) & 1U) != 0U;
            s_st.pvt_num_sv = payload[23];
            s_st.pvt_ms = now_ms;
            s_st.pvt_count++;
            if (solution->valid) {
                s_st.pvt_fix_ok_count++;
            }
            purrgo_gnss_adapter_pvt_applied();
            return true;
        }
        if (msg_id == PURRGO_UBX_ID_NAV_ODO) {
            process_odo(payload, length, now_ms);
        } else if (msg_id == PURRGO_UBX_ID_NAV_DOP) {
            apply_dop(payload, length, solution);
        } else if (msg_id == PURRGO_UBX_ID_NAV_STATUS) {
            process_nav_status(payload, length, now_ms);
        } else if (msg_id == PURRGO_UBX_ID_NAV_SAT) {
            process_nav_sat(payload, length, now_ms);
        }
    } else if (msg_class == PURRGO_UBX_CLASS_SEC && msg_id == PURRGO_UBX_ID_SEC_SIG) {
        process_sec_sig(payload, length, now_ms);
    } else if (msg_class == PURRGO_UBX_CLASS_MON) {
        if (msg_id == PURRGO_UBX_ID_MON_RF) {
            process_mon_rf(payload, length, now_ms);
        } else if (msg_id == PURRGO_UBX_ID_MON_VER) {
            process_mon_ver(payload, length, now_ms);
        }
    } else if (msg_class == PURRGO_UBX_CLASS_ACK && length == 2U &&
               (msg_id == PURRGO_UBX_ID_ACK_NAK || msg_id == PURRGO_UBX_ID_ACK_ACK)) {
        process_cfg_reply(msg_id == PURRGO_UBX_ID_ACK_NAK, payload);
    }
    return false;
}

/* ------------------------------------------------------------------------- */
/* Команды и дистанция трека                                                 */
/* ------------------------------------------------------------------------- */

bool purrgo_gnss_ubx_poll_version(void)
{
    uint8_t frame[8];
    const size_t n = purrgo_ubx_build_frame(frame, sizeof frame, PURRGO_UBX_CLASS_MON,
                                            PURRGO_UBX_ID_MON_VER, NULL, 0U);

    return n != 0U && purrgo_gnss_write(frame, (uint16_t)n);
}

void purrgo_gnss_ubx_reset_trip(uint32_t now_ms)
{
    uint8_t frame[8];
    const size_t n = purrgo_ubx_build_frame(frame, sizeof frame, PURRGO_UBX_CLASS_NAV,
                                            PURRGO_UBX_ID_NAV_RESETODO, NULL, 0U);
    const bool sent = (n != 0U) && purrgo_gnss_write(frame, (uint16_t)n);

    if (s_odo_have) {
        s_odo_base = s_odo_last;
        s_odo_pending_base = false;
    } else {
        s_odo_base = 0U;
        s_odo_pending_base = !sent;
    }
    s_odo_hold = sent;
    s_odo_hold_until = now_ms + ODO_RESET_HOLD_MS;

    if (!sent) {
        PURRGO_LOGW("GNSS: UBX-NAV-RESETODO not sent, trip distance counts from now\r\n");
    }
}

bool purrgo_gnss_ubx_trip_distance(uint32_t now_ms, uint32_t *distance_m)
{
    if (!s_odo_have || !s_st.odo_valid || !purrgo_gnss_ubx_fresh(s_st.odo_ms, now_ms)) {
        return false;
    }
    if (distance_m != NULL) {
        *distance_m = (s_odo_last >= s_odo_base) ? (s_odo_last - s_odo_base) : s_odo_last;
    }
    return true;
}
