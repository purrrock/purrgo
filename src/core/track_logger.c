#include "purrgo/track_logger.h"
#include "purrgo/geo.h"
#include "purrgo/config.h" // Подключаем доступ к app_config.timezone_offset_h
#include <string.h>

#include "purrgo/logger.h"



#define GPX_BUFFER_SIZE 512

static purrgo_file_t* active_file = NULL;
static track_logger_state_t current_state = LOGGER_STATE_IDLE;

static uint8_t current_track_day = 0;

// --- Состояние фильтра ---
static track_logger_mode_t current_mode = LOGGER_MODE_STANDARD;
static bool is_first_point = true;
static int32_t last_recorded_lat = 0;
static int32_t last_recorded_lon = 0;
static uint32_t last_recorded_time = 0;

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

// Легковесный конвертер в секунды с 2000 года (для расчета дельты времени)
// Оптимизировано для микроконтроллеров, работает в диапазоне 2000-2099 гг.
static uint32_t datetime_to_epoch(uint8_t year, uint8_t month, uint8_t day, uint8_t h, uint8_t m, uint8_t s) {
    static const uint16_t days_before_month[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    
    // Подсчет прошедших лет и високосных дней
    uint32_t days = year * 365 + (year + 3) / 4 + days_before_month[month - 1] + day - 1;
    
    // Корректировка, если текущий год високосный и месяц больше февраля
    if (year % 4 == 0 && month > 2) {
        days++;
    }
    
    return ((days * 24 + h) * 60 + m) * 60 + s;
}
// Обратный конвертер из секунд (с 2000 года) в локальную дату и время.
// Корректно обрабатывает високосные года до 2099 года (2100 не является високосным).
static void epoch_to_datetime(uint32_t epoch, uint8_t *year, uint8_t *month, uint8_t *day, 
                              uint8_t *h, uint8_t *m, uint8_t *s) {
    uint32_t time_of_day = epoch % 86400;
    uint32_t days = epoch / 86400;

    *h = time_of_day / 3600;
    *m = (time_of_day % 3600) / 60;
    *s = time_of_day % 60;

    uint32_t y = 0;
    while (1) {
        uint32_t days_in_year = (y % 4 == 0) ? 366 : 365;
        if (days >= days_in_year) {
            days -= days_in_year;
            y++;
        } else {
            break;
        }
    }
    *year = (uint8_t)y;

    uint8_t mo = 1;
    uint16_t days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (y % 4 == 0) days_in_month[2] = 29;

    while (days >= days_in_month[mo]) {
        days -= days_in_month[mo];
        mo++;
    }
    *month = mo;
    *day = (uint8_t)(days + 1);
}
// Вспомогательная функция для сброса данных на файловую систему
static void flush_buffer(void) {
    if (buffer_pos > 0 && active_file != NULL) {
        purrgo_fs_write(active_file, (const uint8_t*)write_buffer, buffer_pos);
        purrgo_fs_sync(active_file); // <-- Критически важно для выживания файла
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

    // 1. Получаем базовое UTC время в секундах
    uint32_t utc_epoch = datetime_to_epoch(first_fix->year, first_fix->month, first_fix->day, 
                                           first_fix->hours, first_fix->minutes, first_fix->seconds);
    
    // 2. Смещаем на локальный часовой пояс
	uint32_t local_epoch = utc_epoch + (app_config.tz_offset_minutes * 60);
    
    uint8_t l_year, l_month, l_day, l_hour, l_min, l_sec;
    epoch_to_datetime(local_epoch, &l_year, &l_month, &l_day, &l_hour, &l_min, &l_sec);

    // Запоминаем ЛОКАЛЬНЫЙ день старта для проверки смены суток
    current_track_day = l_day;

    // Имя файла формируется по локальному времени пользователя
    char filename[32];
    snprintf(filename, sizeof(filename), "%02d%02d%02d-%02d%02d%02d.gpx", 
             l_year, l_month, l_day, l_hour, l_min, l_sec);

    active_file = purrgo_fs_open(filename, FS_WRITE_CREATE);
    if (!active_file) {
        current_state = LOGGER_STATE_ERROR;
        return false;
    }

    buffer_pos = 0;
    is_first_point = true;
    
    write_to_buffer(GPX_HEADER);
    current_state = LOGGER_STATE_RECORDING;
    return true;
}

void purrgo_logger_add_point(const purrgo_gnss_solution_t* fix) {
    if (current_state != LOGGER_STATE_RECORDING || !fix || !fix->valid) return;

    uint32_t utc_epoch = datetime_to_epoch(fix->year, fix->month, fix->day, 
                                           fix->hours, fix->minutes, fix->seconds);
    
    uint32_t local_epoch = utc_epoch + (app_config.tz_offset_minutes * 60);
    
    uint8_t l_year, l_month, l_day, l_hour, l_min, l_sec;
    epoch_to_datetime(local_epoch, &l_year, &l_month, &l_day, &l_hour, &l_min, &l_sec);

    // Проверка смены суток происходит по ЛОКАЛЬНОМУ времени
    if (l_day != current_track_day) {
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
        purrgo_fs_close(active_file);
        active_file = NULL;
    }
    current_state = LOGGER_STATE_IDLE;
}