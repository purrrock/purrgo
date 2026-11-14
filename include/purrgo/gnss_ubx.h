#ifndef PURRGO_GNSS_UBX_H
#define PURRGO_GNSS_UBX_H

/*
 * UBX-расширения приёмника u-blox M10 (интерфейс SPG 5.30, протокол 34.30):
 * разбор кадров из потока вперемешку с NMEA и декодеры сообщений
 *   UBX-NAV-PVT   (0x01 0x07)  время, позиция, скорость, оценки точности;
 *   UBX-NAV-DOP   (0x01 0x04)  HDOP/VDOP/PDOP (вместо NMEA GSA/GGA);
 *   UBX-NAV-ODO   (0x01 0x09)  дистанция с последнего сброса;
 *   UBX-SEC-SIG   (0x27 0x09)  глушение и спуфинг;
 *   UBX-MON-RF    (0x0A 0x38)  уровень помех, антенна;
 *   UBX-MON-VER   (0x0A 0x04)  версия прошивки (ответ на опрос);
 *   UBX-ACK-NAK   (0x05 0x00)  отказ приёмника (в лог).
 * Модуль не зависит от платформы: отправка команд идёт через purrgo_gnss_write()
 * (gnss_io.h). Собирается всегда; в приложение подключается при PURRGO_GNSS_UBX = 1.
 */

#include "purrgo/gnss_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Класс и идентификаторы сообщений. */
#define PURRGO_UBX_CLASS_ACK  0x05U
#define PURRGO_UBX_CLASS_NAV  0x01U
#define PURRGO_UBX_CLASS_MON  0x0AU
#define PURRGO_UBX_CLASS_SEC  0x27U
#define PURRGO_UBX_ID_ACK_NAK 0x00U
#define PURRGO_UBX_ID_ACK_ACK 0x01U
#define PURRGO_UBX_ID_NAV_DOP 0x04U
#define PURRGO_UBX_ID_NAV_PVT 0x07U
#define PURRGO_UBX_ID_NAV_ODO 0x09U
#define PURRGO_UBX_ID_NAV_RESETODO 0x10U
#define PURRGO_UBX_ID_MON_VER 0x04U
#define PURRGO_UBX_ID_MON_RF  0x38U
#define PURRGO_UBX_ID_SEC_SIG 0x09U

/* Длиннее - кадр пропускается (MON-VER с десятком extension ~ 340 байт). */
#define PURRGO_UBX_MAX_PAYLOAD 384U

/* Состояния SEC-SIG (значения полей протокола). */
#define PURRGO_UBX_JAM_UNKNOWN   0U
#define PURRGO_UBX_JAM_NONE      1U   /* глушения нет */
#define PURRGO_UBX_JAM_WARNING   2U   /* глушение обнаружено */
#define PURRGO_UBX_SPOOF_UNKNOWN  0U
#define PURRGO_UBX_SPOOF_NONE     1U
#define PURRGO_UBX_SPOOF_INDICATED 2U /* признаки спуфинга */
#define PURRGO_UBX_SPOOF_AFFIRMED  3U /* спуфинг подтверждён */

/* MON-RF antStatus. */
#define PURRGO_UBX_ANT_INIT      0U
#define PURRGO_UBX_ANT_DONTKNOW  1U
#define PURRGO_UBX_ANT_OK        2U
#define PURRGO_UBX_ANT_SHORT     3U
#define PURRGO_UBX_ANT_OPEN      4U

/* Ответы приёмника на UBX-CFG-VALSET (ACK/NAK) приходят в порядке отправки. */
#define PURRGO_UBX_CFG_MAX_TRACKED 16U

/* Данные считаются устаревшими, если сообщение не приходило дольше (мс). */
#define PURRGO_UBX_STALE_MS 3500U

/* ------------------------------------------------------------------------- */
/* Разбор кадров                                                             */
/* ------------------------------------------------------------------------- */

typedef enum {
    PURRGO_UBX_PARSER_IDLE = 0,
    PURRGO_UBX_PARSER_SYNC2,
    PURRGO_UBX_PARSER_CLASS,
    PURRGO_UBX_PARSER_ID,
    PURRGO_UBX_PARSER_LEN1,
    PURRGO_UBX_PARSER_LEN2,
    PURRGO_UBX_PARSER_PAYLOAD,
    PURRGO_UBX_PARSER_CK_A,
    PURRGO_UBX_PARSER_CK_B
} purrgo_ubx_parser_state_t;

typedef struct {
    purrgo_ubx_parser_state_t state;
    uint8_t msg_class;
    uint8_t msg_id;
    uint16_t length;
    uint16_t pos;
    uint8_t ck_a;
    uint8_t ck_b;
    uint8_t payload[PURRGO_UBX_MAX_PAYLOAD];
} purrgo_ubx_parser_t;

typedef enum {
    PURRGO_UBX_FEED_NOT_UBX = 0, /* байт не относится к UBX: его нужно отдать разборщику NMEA */
    PURRGO_UBX_FEED_CONSUMED,    /* байт часть кадра UBX (или битого кадра, который отброшен) */
    PURRGO_UBX_FEED_FRAME        /* кадр принят с верной контрольной суммой (см. parser->msg_*) */
} purrgo_ubx_feed_t;

void purrgo_ubx_parser_init(purrgo_ubx_parser_t *parser);

/* Идёт приём кадра (между B5 и последним байтом контрольной суммы). */
bool purrgo_ubx_parser_active(const purrgo_ubx_parser_t *parser);

/*
 * Один байт потока. Кадр начинается только с B5 62 в состоянии IDLE, поэтому
 * вызывающий подаёт сюда байты, пока NMEA-строка не начата (иначе B5 внутри
 * текста сорвал бы разбор). Кадр длиннее PURRGO_UBX_MAX_PAYLOAD пропускается:
 * разбор возвращается в IDLE, остаток кадра уходит в NMEA-разборщик и отбрасывается
 * им по контрольной сумме.
 */
purrgo_ubx_feed_t purrgo_ubx_parser_feed(purrgo_ubx_parser_t *parser, uint8_t byte);

/* ------------------------------------------------------------------------- */
/* Состояние, собранное из сообщений                                          */
/* ------------------------------------------------------------------------- */

typedef struct {
    /* UBX-NAV-PVT.flags.psmState (0 PSM выключен, 1 включается, 2 поиск, 3 слежение,
     * 4 слежение с оптимизацией мощности, 5 неактивен); psm_valid - PVT уже приходил. */
    bool psm_valid;
    uint8_t psm_state;

    /*
     * Сводка последнего UBX-NAV-PVT для монитора и диагностики (при выключенном NMEA других
     * источников состояния фикса нет). pvt_fix_ok - решение действительно (то же, что
     * solution.valid), pvt_fix_ok_count - число таких PVT с запуска (сверка с 1PPS).
     */
    bool pvt_valid;
    uint8_t pvt_fix_type;        /* fixType: 0 нет, 2 2D, 3 3D, 4 GNSS+DR, 5 только время */
    bool pvt_fix_ok;
    bool pvt_time_valid;         /* validTime: время известно (PPS уже мигает, позиции ещё может не быть) */
    uint8_t pvt_num_sv;
    uint32_t pvt_ms;             /* время приёма (purrgo_system_time_ms) */
    uint32_t pvt_count;
    uint32_t pvt_fix_ok_count;

    /* UBX-NAV-ODO */
    bool odo_valid;
    uint32_t odo_distance_m;     /* с последнего сброса модуля */
    uint32_t odo_total_m;
    uint32_t odo_std_m;
    uint32_t odo_ms;             /* время приёма (purrgo_system_time_ms) */

    /* UBX-SEC-SIG */
    bool sec_valid;
    bool jam_det_enabled;
    uint8_t jam_state;           /* PURRGO_UBX_JAM_* */
    bool spoof_det_enabled;
    uint8_t spoof_state;         /* PURRGO_UBX_SPOOF_* */
    uint32_t sec_ms;

    /* UBX-MON-RF (первый блок; у M10 один диапазон L1) */
    bool rf_valid;
    uint8_t ant_status;          /* PURRGO_UBX_ANT_* */
    uint8_t ant_power;           /* 0 OFF, 1 ON, 2 неизвестно */
    uint16_t noise_per_ms;
    uint16_t agc_cnt;            /* 0..8191 (100 %) */
    uint8_t cw_suppression;      /* 0 - нет CW-помехи, 255 - сильная */
    uint32_t rf_ms;

    /* UBX-MON-VER */
    bool ver_valid;
    char sw_version[31];
    char hw_version[11];
} purrgo_gnss_ubx_status_t;

/* Сбросить состояние (при старте приложения). Разборщик кадров не затрагивается. */
void purrgo_gnss_ubx_init(void);

const purrgo_gnss_ubx_status_t *purrgo_gnss_ubx_status(void);

/*
 * Обработать принятый кадр. Возвращает true, если это UBX-NAV-PVT и он применён к
 * solution (признак "приёмник жив", как RMC). now_ms - purrgo_system_time_ms().
 */
bool purrgo_gnss_ubx_process_frame(uint8_t msg_class, uint8_t msg_id, const uint8_t *payload,
                                   uint16_t length, purrgo_gnss_solution_t *solution,
                                   uint32_t now_ms);

/* Применить payload UBX-NAV-PVT (92 байта) к решению; false при неверной длине. */
bool purrgo_gnss_ubx_apply_pvt(const uint8_t *payload, uint16_t length,
                               purrgo_gnss_solution_t *solution);

/* ------------------------------------------------------------------------- */
/* Команды приёмнику и дистанция трека                                        */
/* ------------------------------------------------------------------------- */

/*
 * Перед отправкой серии UBX-CFG-VALSET: имена групп в порядке отправки (строки должны
 * жить дольше вызова, не более PURRGO_UBX_CFG_MAX_TRACKED). Ответы ACK/NAK на VALSET
 * сопоставляются группам по порядку, отказ пишется в лог с именем группы.
 */
void purrgo_gnss_ubx_cfg_begin(const char *const *names, uint8_t count);

/* Имя группы по номеру отправки (как в cfg_begin); NULL, если номер вне серии. */
const char *purrgo_gnss_ubx_cfg_name(uint8_t index);

/* Битовая маска групп (по индексу в names), на которые приёмник ответил NAK. */
uint32_t purrgo_gnss_ubx_cfg_nak_mask(void);

/* Сколько ответов (ACK или NAK) на группы получено с последнего cfg_begin(). */
uint8_t purrgo_gnss_ubx_cfg_replies(void);

/* Отправить UBX-MON-VER (опрос версии) через purrgo_gnss_write(). true - передано. */
bool purrgo_gnss_ubx_poll_version(void);

/*
 * Начало нового трека: обнулить дистанцию трека и отправить UBX-NAV-RESETODO.
 * Если команду отправить не удалось (GNSS выключен), дистанция всё равно отсчитывается
 * от текущего значения одометра модуля.
 */
void purrgo_gnss_ubx_reset_trip(uint32_t now_ms);

/*
 * Дистанция трека (м) по одометру модуля. false - данных нет или они устарели
 * (UBX-NAV-ODO не приходит).
 */
bool purrgo_gnss_ubx_trip_distance(uint32_t now_ms, uint32_t *distance_m);

/* Данные сообщения свежие (принято не позже PURRGO_UBX_STALE_MS назад). */
bool purrgo_gnss_ubx_fresh(uint32_t msg_ms, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* PURRGO_GNSS_UBX_H */
