#include "purrgo/track_logger.h"
#include "purrgo/geo.h"
#include "purrgo/config.h" // Подключаем доступ к app_config.timezone_offset_h
#include <string.h>

#include "purrgo/logger.h"



#define GPX_BUFFER_SIZE 512

static purrgo_file_t* active_file = NULL;
static track_logger_state_t current_state = LOGGER_STATE_IDLE;
static char s_active_filename[64] = {0};

static uint8_t current_track_day = 0;

// --- Состояние фильтра ---
static track_logger_mode_t current_mode = LOGGER_MODE_STANDARD;
static bool is_first_point = true;
static int32_t last_recorded_lat = 0;
static int32_t last_recorded_lon = 0;
static uint32_t last_recorded_time = 0;

static uint32_t last_sync_time = 0;

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

// Установка режима логгера
void purrgo_logger_set_mode(track_logger_mode_t mode) {
    current_mode = mode;
}

#include "purrgo/purrgo_time.h"

// Вспомогательная функция для сброса данных на файловую систему
static void flush_buffer(void) {
    if (buffer_pos > 0 && active_file != NULL) {
        // Физическая запись накопленных данных (до 512 байт)
        purrgo_fs_write(active_file, (const uint8_t*)write_buffer, buffer_pos);
        
        // Обязательная синхронизация FAT-таблицы при каждом сбросе буфера.
        // Гарантирует выживание файла при внезапном отключении аккумулятора.
        purrgo_fs_sync(active_file); 
        
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

    purrgo_gnss_solution_t local_fix;
    if (!purrgo_time_apply_timezone(first_fix, &local_fix, app_config.tz_offset_minutes)) {
        return false;
    }

    // Запоминаем ЛОКАЛЬНЫЙ день старта для проверки смены суток
    current_track_day = local_fix.day;

    // Имя файла формируется по локальному времени пользователя
    char filename[32];
    snprintf(filename, sizeof(filename), "%02d%02d%02d-%02d%02d%02d.gpx", 
             local_fix.year, local_fix.month, local_fix.day,
             local_fix.hours, local_fix.minutes, local_fix.seconds);

    active_file = purrgo_fs_open(filename, FS_WRITE_CREATE);
    if (!active_file) {
        current_state = LOGGER_STATE_ERROR;
        return false;
    }

    strncpy(s_active_filename, filename, sizeof(s_active_filename) - 1);
    s_active_filename[sizeof(s_active_filename) - 1] = '\0';

    buffer_pos = 0;
    is_first_point = true;
    last_sync_time = 0;
    
    write_to_buffer(GPX_HEADER);
    current_state = LOGGER_STATE_RECORDING;
    return true;
}

void purrgo_logger_add_point(const purrgo_gnss_solution_t* fix) {
    if (current_state != LOGGER_STATE_RECORDING || !fix || !fix->valid) return;

    uint32_t utc_epoch = 0;
    if (!purrgo_time_datetime_to_epoch(fix->year, fix->month, fix->day,
                                       fix->hours, fix->minutes, fix->seconds, &utc_epoch)) {
        return;
    }
    
    purrgo_gnss_solution_t local_fix;
    if (!purrgo_time_apply_timezone(fix, &local_fix, app_config.tz_offset_minutes)) {
        return;
    }

    // Проверка смены суток происходит по ЛОКАЛЬНОМУ времени
    if (local_fix.day != current_track_day) {
        purrgo_logger_stop();
        if (!purrgo_logger_start(fix)) {
            return; 
        }
    }

    bool should_record = false;

    // Логика фильтрации (Decimation)
    if (is_first_point) {
        should_record = true;
    } else {
        uint32_t dist_m = purrgo_geo_distance_m(last_recorded_lat, last_recorded_lon, fix->lat_1e7, fix->lon_1e7);
        uint32_t dt_s = (utc_epoch >= last_recorded_time) ? (utc_epoch - last_recorded_time) : 0; 

        uint32_t target_dist = (current_mode == LOGGER_MODE_EXPEDITION) ? 100 : 5;
        uint32_t target_time = (current_mode == LOGGER_MODE_EXPEDITION) ? 900 : 300;

        if (dist_m >= target_dist || dt_s >= target_time) {
            should_record = true;
        }
    }

    if (!should_record) return;

    last_recorded_lat = fix->lat_1e7;
    last_recorded_lon = fix->lon_1e7;
    last_recorded_time = utc_epoch; // Фильтр хранит историю в UTC
    is_first_point = false;

    // Запись точки в файл.
    // ВАЖНО: Внутри GPX файла (тег <time>) мы обязаны писать оригинальные данные fix (UTC время).
    int32_t lat_abs = fix->lat_1e7 < 0 ? -fix->lat_1e7 : fix->lat_1e7;
    int32_t lon_abs = fix->lon_1e7 < 0 ? -fix->lon_1e7 : fix->lon_1e7;

    char point_str[128];
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
        if (active_file != NULL) {
            purrgo_fs_sync(active_file); // Ensure final data is written before closing
            purrgo_fs_close(active_file);
            active_file = NULL;
        }
    }
    s_active_filename[0] = '\0';
    current_state = LOGGER_STATE_IDLE;
}

const char* purrgo_logger_get_active_filename(void) {
    if (s_active_filename[0] != '\0') {
        return s_active_filename;
    }
    return NULL;
}
