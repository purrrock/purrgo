#include "purrgo/gnss_adapter.h"
#include "minmea.h"

static purrgo_gnss_solution_t authoritative_solution = {0};

const purrgo_gnss_solution_t *purrgo_gnss_get_solution(void) {
    return &authoritative_solution;
}

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

void purrgo_gnss_process_nmea(const char *nmea_line, purrgo_gnss_solution_t *solution) {
    // Если указатель не передан, используем внутреннее авторитетное состояние
    purrgo_gnss_solution_t *target = solution ? solution : &authoritative_solution;

    // Validate checksum strictly before modifying the solution
    if (!minmea_check(nmea_line, true)) {
        return;
    }

    // Определение типа NMEA-сообщения
    switch (minmea_sentence_id(nmea_line, false)) {
        case MINMEA_SENTENCE_RMC: {
            struct minmea_sentence_rmc frame;
            if (minmea_parse_rmc(&frame, nmea_line)) {
                target->valid = frame.valid;
                if (frame.valid) {
                    target->lat_1e7 = convert_to_1e7(frame.latitude);
                    target->lon_1e7 = convert_to_1e7(frame.longitude);
                    
                    // Масштабирование скорости (узлы * 100)
                    if (frame.speed.scale != 0) {
                        target->speed_knots = (int32_t)(((int64_t)frame.speed.value * 100) / frame.speed.scale);
                    }

                    // Масштабирование курса (градусы * 100)
                    if (frame.course.scale != 0) {
                        target->course_valid = true;
                        target->course_deg_100 = (int32_t)(((int64_t)frame.course.value * 100) / frame.course.scale);
                    } else {
                        target->course_valid = false;
                    }
                    
                    target->hours = frame.time.hours;
                    target->minutes = frame.time.minutes;
                    target->seconds = frame.time.seconds;
                    target->day = frame.date.day;
                    target->month = frame.date.month;
                    target->year = frame.date.year;
                }
            }
            break;
        }
        case MINMEA_SENTENCE_GGA: {
            struct minmea_sentence_gga frame;
            if (minmea_parse_gga(&frame, nmea_line)) {
                target->satellites_tracked = frame.satellites_tracked;
                if (frame.altitude.scale != 0) {
                    target->alt_m = frame.altitude.value / frame.altitude.scale;
                }
                target->fix_quality = frame.fix_quality;
                if (frame.hdop.scale != 0) {
                    target->hdop_100 = (int32_t)(((int64_t)frame.hdop.value * 100) / frame.hdop.scale);
                }
            }
            break;
        }
        case MINMEA_SENTENCE_GSA: {
            struct minmea_sentence_gsa frame;
            if (minmea_parse_gsa(&frame, nmea_line)) {
                target->fix_type = frame.fix_type;
                if (frame.hdop.scale != 0) {
                    target->hdop_100 = (int32_t)(((int64_t)frame.hdop.value * 100) / frame.hdop.scale);
                }
                if (frame.vdop.scale != 0) {
                    target->vdop_100 = (int32_t)(((int64_t)frame.vdop.value * 100) / frame.vdop.scale);
                }
                if (frame.pdop.scale != 0) {
                    target->pdop_100 = (int32_t)(((int64_t)frame.pdop.value * 100) / frame.pdop.scale);
                }
            }
            break;
        }
        default:
            // Игнорируем $GPVTG, $GPGSV, $GPGLL и проприетарные $GPTXT от u-blox
            break;
    }
}

