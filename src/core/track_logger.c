#include "purrgo/purrgo_format.h"

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

/*
 * RAM Track Buffer
 *
 * This buffer exists separately from the GPX file because reading/parsing a GPX
 * file back from the SD card in real-time during map rendering would be too slow
 * and cause excessive SD card wear. The RAM buffer provides immediate, fast access
 * to the recent track geometry for live rendering on the screen.
 *
 * It is implemented as a circular buffer to bound memory usage predictably,
 * discarding the oldest points once full. It does NOT affect the GPX file recording,
 * which continues writing all points indefinitely.
 *
 * The maximum is 8192 points, which uses exactly 64 KiB of RAM (8192 * 8 bytes).
 * This is an acceptable and deterministic footprint for the target STM32 platform.
 */
static track_point_t ram_track_buffer[TRACK_RAM_MAX_POINTS];
static size_t ram_track_head = 0;
static size_t ram_track_count = 0;
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
    if (current_mode == LOGGER_MODE_OFF &&
        (current_state == LOGGER_STATE_RECORDING || current_state == LOGGER_STATE_ERROR)) {
        purrgo_logger_stop();
    }
}

track_logger_mode_t purrgo_logger_get_mode(void) {
    return current_mode;
}

#include "purrgo/purrgo_time.h"

/*
 * Переводит логгер в состояние ERROR после сбоя записи на SD.
 *
 * Файл закрывается (слот и блокировка FatFs освобождаются), но имя файла и
 * ещё не записанная часть write_buffer сохраняются: purrgo_logger_start()
 * попробует переоткрыть тот же файл в режиме дозаписи.
 */
static void enter_error_state(const char* reason) {
    PURRGO_LOG("Track logger error: %s\r\n", reason);
    if (active_file != NULL) {
        (void)purrgo_fs_close(active_file);
        active_file = NULL;
    }
    current_state = LOGGER_STATE_ERROR;
}

// Сброс накопленных данных на файловую систему.
// Возвращает false при ошибке записи; необработанные данные остаются в буфере.
static bool flush_buffer(void) {
    if (buffer_pos == 0) {
        return true;
    }
    if (active_file == NULL) {
        return false;
    }

    // Физическая запись накопленных данных (до 512 байт)
    uint32_t written = purrgo_fs_write(active_file, (const uint8_t*)write_buffer, (uint32_t)buffer_pos);
    if (written < buffer_pos) {
        // Часть данных могла дойти до файла: оставляем в буфере только хвост.
        memmove(write_buffer, write_buffer + written, buffer_pos - written);
        buffer_pos -= written;
        return false;
    }
    buffer_pos = 0;

    // Синхронизация FAT-таблицы при каждом сбросе буфера.
    // Гарантирует выживание файла при внезапном отключении аккумулятора.
    if (!purrgo_fs_sync(active_file)) {
        // Данные переданы FatFs, но на носитель не попали: файл нужно переоткрыть.
        return false;
    }
    return true;
}

// Запись строки в буфер: строка помещается целиком или не записывается вовсе
// (иначе при сбое в GPX остался бы обрывок тега).
static bool write_to_buffer(const char* str) {
    size_t len = strlen(str);
    if (len > GPX_BUFFER_SIZE) {
        return false;
    }

    if (len > GPX_BUFFER_SIZE - buffer_pos) {
        if (!flush_buffer()) {
            return false;
        }
    }

    memcpy(&write_buffer[buffer_pos], str, len);
    buffer_pos += len;
    return true;
}

// Дописывает футер, сбрасывает буфер и закрывает файл. Возвращает false при ошибке.
static bool finalize_active_file(void) {
    bool ok = write_to_buffer(GPX_FOOTER);
    if (ok) {
        ok = flush_buffer();
    }
    bool closed = purrgo_fs_close(active_file);
    active_file = NULL;
    return ok && closed;
}

bool purrgo_logger_start(const purrgo_gnss_solution_t* first_fix) {
    if (current_mode == LOGGER_MODE_OFF) return false;
    if (current_state == LOGGER_STATE_RECORDING) return false;
    if (!first_fix || !first_fix->valid) return false;

    /*
     * Восстановление после ошибки записи: пробуем дозаписать тот же файл,
     * чтобы сессия осталась одним GPX. Если открыть не удалось - файл
     * бросаем и ниже начинаем новый.
     */
    if (current_state == LOGGER_STATE_ERROR && s_active_filename[0] != '\0') {
        active_file = purrgo_fs_open(s_active_filename, FS_WRITE_APPEND);
        if (active_file) {
            PURRGO_LOG("Track logging resumed: %s\r\n", s_active_filename);
            current_state = LOGGER_STATE_RECORDING;
            return true;
        }
        PURRGO_LOG("Track logging: cannot resume %s, starting new file\r\n", s_active_filename);
        s_active_filename[0] = '\0';
        buffer_pos = 0;
    }

    purrgo_gnss_solution_t local_fix;
    if (!purrgo_time_apply_timezone(first_fix, &local_fix, app_config.tz_offset_minutes)) {
        return false;
    }

    // Запоминаем ЛОКАЛЬНЫЙ день старта для проверки смены суток
    current_track_day = local_fix.day;

// формируем имя файла трека
// Имя: секунды от 01.01.2026, в HEX.
// 8 символов + .gpx — формат FAT 8.3.
// Фиксированная ширина обеспечивает сортировку по имени.
uint32_t epoch = 0;
purrgo_time_datetime_to_epoch(local_fix.year, local_fix.month, local_fix.day,
                              local_fix.hours, local_fix.minutes, local_fix.seconds,
                              &epoch);

// Epoch purrgo_time начинается с 01.01.2000.
// 01.01.2026 = 820454400 секунд.
const uint32_t epoch_2026 = 820454400UL;
uint32_t track_id = epoch - epoch_2026;

char filename[64];
const char* prefix = purrgo_fs_get_tracks_path();

if (prefix[0] != '\0') {
    purrgo_snprintf(filename, sizeof(filename), "%s/%08X.gpx",
                    prefix, (unsigned int)track_id);
} else {
    purrgo_snprintf(filename, sizeof(filename), "%08X.gpx",
                    (unsigned int)track_id);
}
// закончили формирование имя файла трека

    active_file = purrgo_fs_open(filename, FS_WRITE_CREATE);
    if (!active_file) {
        current_state = LOGGER_STATE_ERROR;
        return false;
    }

    purrgo_snprintf(s_active_filename, sizeof(s_active_filename), "%.*s", (int)(sizeof(s_active_filename) - 1), filename);

    buffer_pos = 0;
    is_first_point = true;
    last_sync_time = 0;
    
    // Clear the RAM buffer for the new session
    ram_track_head = 0;
    ram_track_count = 0;

    if (!write_to_buffer(GPX_HEADER)) {
        // Заголовок (< размера буфера) в пустой буфер помещается всегда.
        enter_error_state("header");
        return false;
    }
    current_state = LOGGER_STATE_RECORDING;
    return true;
}

bool purrgo_logger_add_point(const purrgo_gnss_solution_t* fix) {
    if (current_mode == LOGGER_MODE_OFF) return false;
    if (current_state != LOGGER_STATE_RECORDING || !fix || !fix->valid) return false;

    uint32_t utc_epoch = 0;
    if (!purrgo_time_datetime_to_epoch(fix->year, fix->month, fix->day,
                                       fix->hours, fix->minutes, fix->seconds, &utc_epoch)) {
        return false;
    }
    
    purrgo_gnss_solution_t local_fix;
    if (!purrgo_time_apply_timezone(fix, &local_fix, app_config.tz_offset_minutes)) {
        return false;
    }

    // Проверка смены суток происходит по ЛОКАЛЬНОМУ времени
    if (local_fix.day != current_track_day) {
        purrgo_logger_stop();
        if (!purrgo_logger_start(fix)) {
            return false;
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

    if (!should_record) return false;


    // Запись точки в файл.
    // ВАЖНО: Внутри GPX файла (тег <time>) мы обязаны писать оригинальные данные fix (UTC время).
    int32_t lat_abs = fix->lat_1e7 < 0 ? -fix->lat_1e7 : fix->lat_1e7;
    int32_t lon_abs = fix->lon_1e7 < 0 ? -fix->lon_1e7 : fix->lon_1e7;

    char point_str[128];
    purrgo_snprintf(point_str, sizeof(point_str),
             "<trkpt lat=\"%s%d.%07d\" lon=\"%s%d.%07d\">\n"
             "  <ele>%d</ele>\n"
             "  <time>20%02d-%02d-%02dT%02d:%02d:%02dZ</time>\n"
             "</trkpt>\n",
             fix->lat_1e7 < 0 ? "-" : "", (int)(lat_abs / 10000000), (int)(lat_abs % 10000000),
             fix->lon_1e7 < 0 ? "-" : "", (int)(lon_abs / 10000000), (int)(lon_abs % 10000000),
             (int)fix->alt_m,
             fix->year, fix->month, fix->day,
             fix->hours, fix->minutes, fix->seconds);

    if (!write_to_buffer(point_str)) {
        // Сбой записи на SD: точка потеряна, логгер уходит в ERROR (повтор - из app_fsm).
        enter_error_state("write");
        return false;
    }

    last_recorded_lat = fix->lat_1e7;
    last_recorded_lon = fix->lon_1e7;
    last_recorded_time = utc_epoch; // Фильтр хранит историю в UTC
    is_first_point = false;

    // Also add to the RAM circular buffer
    ram_track_buffer[ram_track_head].lat_1e7 = fix->lat_1e7;
    ram_track_buffer[ram_track_head].lon_1e7 = fix->lon_1e7;
    ram_track_head = (ram_track_head + 1) % TRACK_RAM_MAX_POINTS;
    if (ram_track_count < TRACK_RAM_MAX_POINTS) {
        ram_track_count++;
    }

    return true;
}
void purrgo_logger_stop(void) {
    if (current_state == LOGGER_STATE_ERROR && s_active_filename[0] != '\0') {
        // Файл остался без футера после сбоя: одна попытка дозаписать его.
        active_file = purrgo_fs_open(s_active_filename, FS_WRITE_APPEND);
        if (active_file) {
            current_state = LOGGER_STATE_RECORDING;
        }
    }

    if (current_state == LOGGER_STATE_RECORDING) {
        if (finalize_active_file()) {
            PURRGO_LOG("Track logging stop\r\n");
        } else {
            PURRGO_LOG("Track logging stop: write error, file may be incomplete\r\n");
        }
    }
    buffer_pos = 0;
    s_active_filename[0] = '\0';
    current_state = LOGGER_STATE_IDLE;

    // Clear the RAM buffer
    ram_track_head = 0;
    ram_track_count = 0;
}

const char* purrgo_logger_get_active_filename(void) {
    if (s_active_filename[0] != '\0') {
        return s_active_filename;
    }
    return NULL;
}

track_logger_state_t purrgo_logger_get_state(void) {
    return current_state;
}

size_t purrgo_logger_get_track_point_count(void) {
    return ram_track_count;
}

bool purrgo_logger_get_track_point(size_t index, track_point_t *out_point) {
    if (out_point == NULL) {
        return false;
    }

    if (index >= ram_track_count) {
        return false;
    }

    // The oldest point is at (ram_track_head - ram_track_count) % TRACK_RAM_MAX_POINTS
    // The requested point is at oldest_point + index
    size_t start_index;
    if (ram_track_head >= ram_track_count) {
        start_index = ram_track_head - ram_track_count;
    } else {
        start_index = TRACK_RAM_MAX_POINTS - (ram_track_count - ram_track_head);
    }

    size_t target_index = (start_index + index) % TRACK_RAM_MAX_POINTS;
    *out_point = ram_track_buffer[target_index];

    return true;
}

bool purrgo_logger_get_last_two_points(
    track_point_t *previous,
    track_point_t *last
) {
    if (previous == NULL || last == NULL) {
        return false;
    }

    if (ram_track_count < 2) {
        return false;
    }

    // `ram_track_head` points to the next available slot for writing.
    // Therefore, the newest element is at index `ram_track_head - 1`
    // (with wraparound handling for circular buffer).
    size_t last_idx = (ram_track_head == 0) ? (TRACK_RAM_MAX_POINTS - 1) : (ram_track_head - 1);

    // The second newest element is at index `ram_track_head - 2`
    // (with wraparound handling).
    size_t prev_idx = (last_idx == 0) ? (TRACK_RAM_MAX_POINTS - 1) : (last_idx - 1);

    *last = ram_track_buffer[last_idx];
    *previous = ram_track_buffer[prev_idx];

    return true;
}
