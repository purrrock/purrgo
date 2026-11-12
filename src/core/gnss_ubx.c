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
}

void purrgo_gnss_ubx_cfg_begin(const char *const *names, uint8_t count)
{
    s_cfg_names = names;
    s_cfg_count = (count > PURRGO_UBX_CFG_MAX_TRACKED) ? (uint8_t)PURRGO_UBX_CFG_MAX_TRACKED : count;
    s_cfg_replies = 0U;
    s_cfg_nak_mask = 0U;
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

    s_st.ant_status = b[2];
    s_st.ant_power = b[3];
    s_st.noise_per_ms = rd_u16(&b[12]);
    s_st.agc_cnt = rd_u16(&b[14]);
    s_st.cw_suppression = b[16];
    s_st.rf_valid = true;
    s_st.rf_ms = now_ms;
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
            purrgo_gnss_adapter_pvt_applied();
            return true;
        }
        if (msg_id == PURRGO_UBX_ID_NAV_ODO) {
            process_odo(payload, length, now_ms);
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
