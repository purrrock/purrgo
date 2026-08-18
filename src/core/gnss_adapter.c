#include "purrgo/gnss_adapter.h"
#include "minmea.h"

// Вспомогательная функция для перевода формата minmea_float (значение, масштаб) 
// в микроградусы (1e7) с использованием исключительно целочисленной арифметики.
static int32_t convert_to_1e7(struct minmea_float f) {
    if (f.scale == 0) return 0;
    
    // NMEA передает координаты в формате DDDMM.MMMMM.
    // Библиотека minmea возвращает это как f.value = DDDMMMMMMM и f.scale.
    // Избегаем функции minmea_tocoord, так как она использует float и вызовет генерацию SoftFP на STM32.
    
    int32_t abs_value = f.value < 0 ? -f.value : f.value;
    
    // Выделение градусов (DDD) и минут с долями (MM.MMMMM)
    int32_t degrees = abs_value / (f.scale * 100);
    int32_t minutes_scaled = abs_value % (f.scale * 100);
    
    // Перевод минут в градусы: (minutes_scaled / scale) / 60
    // Для формата 1e7 умножаем на 10 000 000.
    // Используем 64-битную арифметику (int64_t) для предотвращения переполнения регистра.
    int64_t fractional_deg_1e7 = ((int64_t)minutes_scaled * 10000000LL) / ((int64_t)f.scale * 60LL);
    
    int32_t result = (degrees * 10000000) + (int32_t)fractional_deg_1e7;
    
    return f.value < 0 ? -result : result;
}

void purrgo_gnss_process_nmea(const char *nmea_line, purrgo_gnss_solution_t *solution) {
    // Определение типа NMEA-сообщения
    switch (minmea_sentence_id(nmea_line, false)) {
        case MINMEA_SENTENCE_RMC: {
            struct minmea_sentence_rmc frame;
            if (minmea_parse_rmc(&frame, nmea_line)) {
                solution->valid = frame.valid;
                if (frame.valid) {
                    solution->lat_1e7 = convert_to_1e7(frame.latitude);
                    solution->lon_1e7 = convert_to_1e7(frame.longitude);
                    
                    // Масштабирование скорости (узлы * 100)
                    if (frame.speed.scale != 0) {
                        solution->speed_knots = (frame.speed.value * 100) / frame.speed.scale;
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
                solution->satellites = frame.satellites_tracked;
                if (frame.altitude.scale != 0) {
                    solution->alt_m = frame.altitude.value / frame.altitude.scale;
                }
            }
            break;
        }
        default:
            // Игнорируем $GPVTG, $GPGSA, $GPGSV, $GPGLL и проприетарные $GPTXT от u-blox
            break;
    }
}

