#include "purrgo/track_logger.h"
#include <stdio.h>
#include <string.h>

// Кратность 512 байт оптимальна для прямой записи секторов на SD-карту.
#define GPX_BUFFER_SIZE 512

static purrgo_file_t* active_file = NULL;
static track_logger_state_t current_state = LOGGER_STATE_IDLE;

static char write_buffer[GPX_BUFFER_SIZE];
static size_t buffer_pos = 0;

// Стандартный заголовок файла GPX 1.1
static const char* GPX_HEADER = 
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<gpx version=\"1.1\" creator=\"PurrGo\">\n"
    "<trk><trkseg>\n";

// Закрывающие теги
static const char* GPX_FOOTER = 
    "</trkseg></trk>\n"
    "</gpx>\n";

// Вспомогательная функция для сброса данных на файловую систему
static void flush_buffer(void) {
    if (buffer_pos > 0 && active_file != NULL) {
        purrgo_fs_write(active_file, (const uint8_t*)write_buffer, buffer_pos);
        buffer_pos = 0;
    }
}

// Запись строки в буфер с проверкой переполнения
static void write_to_buffer(const char* str) {
    size_t len = strlen(str);
    size_t written = 0;

    while (written < len) {
        size_t space_left = GPX_BUFFER_SIZE - buffer_pos;
        size_t chunk = (len - written > space_left) ? space_left : (len - written);
        
        memcpy(&write_buffer[buffer_pos], &str[written], chunk);
        buffer_pos += chunk;
        written += chunk;

        // Если буфер заполнен до конца - сброс на накопитель
        if (buffer_pos == GPX_BUFFER_SIZE) {
            flush_buffer();
        }
    }
}

bool purrgo_logger_start(const purrgo_gnss_solution_t* first_fix) {
    if (current_state == LOGGER_STATE_RECORDING) return false;
    if (!first_fix || !first_fix->valid) return false;

    // Генерация имени файла формата YYMMDD-HHMMSS.gpx
    char filename[32];
    snprintf(filename, sizeof(filename), "%02d%02d%02d-%02d%02d%02d.gpx", 
             first_fix->year, first_fix->month, first_fix->day,
             first_fix->hours, first_fix->minutes, first_fix->seconds);

    active_file = purrgo_fs_open(filename, FS_WRITE_CREATE);
    if (!active_file) {
        current_state = LOGGER_STATE_ERROR;
        return false;
    }

    buffer_pos = 0;
    write_to_buffer(GPX_HEADER);
    current_state = LOGGER_STATE_RECORDING;
    
    return true;
}

void purrgo_logger_add_point(const purrgo_gnss_solution_t* fix) {
    if (current_state != LOGGER_STATE_RECORDING || !fix || !fix->valid) return;

    // Разбор целочисленных координат 1e7 для формирования строки (DDD.DDDDDDD)
    int32_t lat_abs = fix->lat_1e7 < 0 ? -fix->lat_1e7 : fix->lat_1e7;
    int32_t lon_abs = fix->lon_1e7 < 0 ? -fix->lon_1e7 : fix->lon_1e7;

    char point_str[128];
    // Использование целочисленной арифметики и модулей позволяет избежать тяжелого sprintf("%f")
    snprintf(point_str, sizeof(point_str), 
             "<trkpt lat=\"%s%d.%07d\" lon=\"%s%d.%07d\">\n"
             "  <ele>%d</ele>\n"
             "  <time>20%02d-%02d-%02dT%02d:%02d:%02dZ</time>\n"
             "</trkpt>\n",
             fix->lat_1e7 < 0 ? "-" : "", lat_abs / 10000000, lat_abs % 10000000,
             fix->lon_1e7 < 0 ? "-" : "", lon_abs / 10000000, lon_abs % 10000000,
             fix->alt_m,
             fix->year, fix->month, fix->day,
             fix->hours, fix->minutes, fix->seconds);

    write_to_buffer(point_str);
}

void purrgo_logger_stop(void) {
    if (current_state == LOGGER_STATE_RECORDING) {
        write_to_buffer(GPX_FOOTER);
        flush_buffer();
        purrgo_fs_close(active_file);
        active_file = NULL;
    }
    current_state = LOGGER_STATE_IDLE;
}