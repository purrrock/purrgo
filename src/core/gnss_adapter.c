#include "purrgo/gnss_adapter.h"
#include "minmea.h"
#include <string.h>

// Вспомогательная функция для перевода формата minmea_float (значение, масштаб) 
// в микроградусы (1e7) с использованием исключительно целочисленной арифметики.
static int32_t convert_to_1e7(struct minmea_float f)
{
    if (f.scale == 0) {
        return 0;
    }

    /*
     * NMEA coordinates are represented as DDDMM.MMMMM.
     *
     * minmea stores the value as an integer plus a decimal scale.
     *
     * We intentionally do not use minmea_tocoord(), because the
     * navigation core must remain free of floating-point arithmetic.
     */

    /*
     * Widen before negation.
     *
     * Negating INT32_MIN in int32_t is undefined behavior because
     * +2147483648 cannot be represented by int32_t.
     */
    int64_t abs_value = f.value < 0
                      ? -(int64_t)f.value
                      : (int64_t)f.value;

    /*
     * Split DDDMM.MMMMM into:
     *
     *     degrees
     *     minutes_scaled
     *
     * The multiplication is explicitly widened to int64_t.
     */
    int64_t degrees =
        abs_value / ((int64_t)f.scale * 100LL);

    int64_t minutes_scaled =
        abs_value % ((int64_t)f.scale * 100LL);

    /*
     * Convert minutes to degrees * 1e7:
     *
     *     minutes / 60 * 10,000,000
     *
     * Keep the entire calculation in int64_t.
     */
    int64_t fractional_deg_1e7 =
        (minutes_scaled * 10000000LL) /
        ((int64_t)f.scale * 60LL);

    int64_t result =
        degrees * 10000000LL + fractional_deg_1e7;

    return (int32_t)(f.value < 0 ? -result : result);
}

/*
 * GSA приходит отдельно от каждой навигационной системы (GP/GL/GA/BD/QZ), а у
 * "GN" - по одной фразе на систему с идентификатором системы в последнем поле
 * (NMEA 4.1). Если применять каждую фразу к решению напрямую, GSA системы без
 * спутников в решении ("нет фикса", DOP 99.9) затирает результат остальных.
 * Поэтому GSA запоминается по системам, а в решение попадает сводка:
 * наилучший fix_type среди недавно обновлённых систем и DOP из этой системы.
 */
enum {
    GSA_SYS_GPS = 0,
    GSA_SYS_GLONASS,
    GSA_SYS_GALILEO,
    GSA_SYS_BEIDOU,
    GSA_SYS_QZSS,
    GSA_SYS_COMBINED,   /* GN без идентификатора системы */
    GSA_SYS_OTHER,
    GSA_SYS_COUNT
};

/* Запись устарела, если система не присылала GSA столько эпох (фраз RMC). */
#define GSA_STALE_EPOCHS 3U

typedef struct {
    bool used;
    uint32_t epoch;
    uint8_t fix_type;
    int32_t hdop_100;   /* -1 - в фразе не было */
    int32_t vdop_100;
    int32_t pdop_100;
} gsa_slot_t;

static gsa_slot_t s_gsa[GSA_SYS_COUNT];
static uint32_t s_gsa_epoch = 0;

void purrgo_gnss_adapter_reset(void)
{
    memset(s_gsa, 0, sizeof(s_gsa));
    s_gsa_epoch = 0;
}

static int32_t minmea_to_100(struct minmea_float f)
{
    if (f.scale == 0) {
        return -1;
    }
    return (int32_t)(((int64_t)f.value * 100) / f.scale);
}

/* Идентификатор системы из последнего поля GSA (0 - поля нет). */
static int gsa_system_id(const char *line)
{
    const char *star = strchr(line, '*');
    if (star == NULL) {
        return 0;
    }
    const char *p = star;
    while (p > line && *(p - 1) != ',') {
        p--;
    }
    if (p == line || p == star) {
        return 0;
    }
    return (p[0] >= '1' && p[0] <= '9' && p + 1 == star) ? (p[0] - '0') : 0;
}

static int gsa_slot_index(const char *line)
{
    char talker[3] = {0};
    if (!minmea_talker_id(talker, line)) {
        return GSA_SYS_OTHER;
    }
    if (talker[0] == 'G' && talker[1] == 'N') {
        switch (gsa_system_id(line)) {
            case 1: return GSA_SYS_GPS;
            case 2: return GSA_SYS_GLONASS;
            case 3: return GSA_SYS_GALILEO;
            case 4: return GSA_SYS_BEIDOU;
            case 5: return GSA_SYS_QZSS;
            default: return GSA_SYS_COMBINED;
        }
    }
    if (talker[0] == 'G' && talker[1] == 'P') return GSA_SYS_GPS;
    if (talker[0] == 'G' && talker[1] == 'L') return GSA_SYS_GLONASS;
    if (talker[0] == 'G' && talker[1] == 'A') return GSA_SYS_GALILEO;
    if ((talker[0] == 'B' && talker[1] == 'D') || (talker[0] == 'G' && talker[1] == 'B')) return GSA_SYS_BEIDOU;
    if (talker[0] == 'Q' && talker[1] == 'Z') return GSA_SYS_QZSS;
    return GSA_SYS_OTHER;
}

/* Сводка по системам -> решение. Ничего не меняет, пока GSA ни разу не приходила. */
static void gsa_merge_into(purrgo_gnss_solution_t *solution)
{
    int best = -1;
    bool any_used = false;

    for (int i = 0; i < GSA_SYS_COUNT; i++) {
        if (!s_gsa[i].used) {
            continue;
        }
        any_used = true;
        if (s_gsa_epoch - s_gsa[i].epoch >= GSA_STALE_EPOCHS) {
            continue;
        }
        if (best < 0 ||
            s_gsa[i].fix_type > s_gsa[best].fix_type ||
            (s_gsa[i].fix_type == s_gsa[best].fix_type &&
             s_gsa[i].hdop_100 >= 0 &&
             (s_gsa[best].hdop_100 < 0 || s_gsa[i].hdop_100 < s_gsa[best].hdop_100))) {
            best = i;
        }
    }

    if (!any_used) {
        return;
    }
    if (best < 0) {
        solution->fix_type = 1;     /* все системы замолчали */
        return;
    }

    solution->fix_type = s_gsa[best].fix_type;
    if (s_gsa[best].hdop_100 >= 0) solution->hdop_100 = s_gsa[best].hdop_100;
    if (s_gsa[best].vdop_100 >= 0) solution->vdop_100 = s_gsa[best].vdop_100;
    if (s_gsa[best].pdop_100 >= 0) solution->pdop_100 = s_gsa[best].pdop_100;
}

bool purrgo_gnss_process_nmea(const char *nmea_line, purrgo_gnss_solution_t *solution) {
    bool rmc_parsed = false;

    // Validate checksum strictly before modifying the solution
    if (!minmea_check(nmea_line, true)) {
        return false;
    }

    // Определение типа NMEA-сообщения
    switch (minmea_sentence_id(nmea_line, false)) {
        case MINMEA_SENTENCE_RMC: {
            struct minmea_sentence_rmc frame;
            if (minmea_parse_rmc(&frame, nmea_line)) {
                rmc_parsed = true;
                s_gsa_epoch++;      /* новая эпоха: устаревшие GSA отбрасываются */
                gsa_merge_into(solution);
                solution->valid = frame.valid;
                if (frame.valid) {
                    solution->lat_1e7 = convert_to_1e7(frame.latitude);
                    solution->lon_1e7 = convert_to_1e7(frame.longitude);
                    
                    // Масштабирование скорости (узлы * 100)
                    if (frame.speed.scale != 0) {
                        solution->speed_knots = (int32_t)(((int64_t)frame.speed.value * 100) / frame.speed.scale);
                    }

                    // Масштабирование курса (градусы * 100)
                    if (frame.course.scale != 0) {
                        solution->course_valid = true;
                        solution->course_deg_100 = (int32_t)(((int64_t)frame.course.value * 100) / frame.course.scale);
                    } else {
                        solution->course_valid = false;
                    }
                    
                    solution->hours = frame.time.hours;
                    solution->minutes = frame.time.minutes;
                    solution->seconds = frame.time.seconds;
                    solution->day = frame.date.day;
                    solution->month = frame.date.month;
                    solution->year = frame.date.year;
                }
            }
            break;
        }
        case MINMEA_SENTENCE_GGA: {
            struct minmea_sentence_gga frame;
            if (minmea_parse_gga(&frame, nmea_line)) {
                solution->satellites_tracked = frame.satellites_tracked;
                if (frame.altitude.scale != 0) {
                    solution->alt_m = frame.altitude.value / frame.altitude.scale;
                }
                solution->fix_quality = frame.fix_quality;
                if (frame.hdop.scale != 0) {
                    solution->hdop_100 = (int32_t)(((int64_t)frame.hdop.value * 100) / frame.hdop.scale);
                }
            }
            break;
        }
        case MINMEA_SENTENCE_GSA: {
            struct minmea_sentence_gsa frame;
            if (minmea_parse_gsa(&frame, nmea_line)) {
                gsa_slot_t *slot = &s_gsa[gsa_slot_index(nmea_line)];
                slot->used = true;
                slot->epoch = s_gsa_epoch;
                slot->fix_type = (uint8_t)frame.fix_type;
                slot->hdop_100 = minmea_to_100(frame.hdop);
                slot->vdop_100 = minmea_to_100(frame.vdop);
                slot->pdop_100 = minmea_to_100(frame.pdop);
                gsa_merge_into(solution);
            }
            break;
        }
        default:
            // Игнорируем $GPVTG, $GPGSV, $GPGLL и проприетарные $GPTXT от u-blox
            break;
    }

    return rmc_parsed;
}

