#include "purrgo/purrgo_format.h"

#include "purrgo/track_logger.h"
#include "purrgo/geo.h"
#include "purrgo/battery.h"
#include "purrgo/config.h" // Подключаем доступ к app_config.timezone_offset_h
#include "purrgo/hardware_config.h"
#include <string.h>

#include "purrgo/logger.h"



/* Размер буфера записи. Одна точка с расширениями - до ~500 байт, заголовок GPX - ~600. */
#define GPX_BUFFER_SIZE 1024

/*
 * Диагностический режим (сборка с -DPURRGO_TRACK_DIAG=1, CMake: -DPURRGO_TRACK_DIAG=ON):
 * в файл пишется КАЖДАЯ валидная эпоха GNSS (1 точка в секунду), а в <extensions>
 * каждой точки стоит pg:rej - битовая маска фильтров (LOGGER_REJ_*), которые в
 * обычной сборке эту точку отбросили бы (0 - обычная сборка точку записала бы).
 * Состояние фильтров ведётся как в обычной сборке, поэтому по такому файлу можно
 * подобрать пороги и проверить прореживание. Экранный трек (RAM) не меняется.
 * Объём записи ~0,9 МБ в час. Для штатной работы выключено.
 */
#ifndef PURRGO_TRACK_DIAG
#define PURRGO_TRACK_DIAG 1
#endif

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

/*
 * Синхронизация FAT (f_sync) - не на каждый сброс буфера (~каждые 4 точки), а
 * раз в LOGGER_SYNC_INTERVAL_S секунд или LOGGER_SYNC_POINTS точек. При внезапной
 * потере питания теряется не больше этого окна; при штатной остановке файл
 * закрывается (f_close синхронизирует).
 */
#define LOGGER_SYNC_INTERVAL_S 60U
#define LOGGER_SYNC_POINTS     50U

static uint32_t last_sync_time = 0;      /* UTC epoch последней синхронизации, 0 - ещё не было */
static uint32_t points_since_sync = 0;

/*
 * Предыдущая записанная точка: вместе с last_recorded_* задаёт направление
 * последнего отрезка трека (фильтр незначительных изменений, см.
 * deviates_from_last_segment). prev_valid == false, если отрезка ещё нет или он
 * короче минимального шага (направление по шуму GNSS ничего не значит).
 */
static bool     prev_valid = false;
static int32_t  prev_lat = 0;
static int32_t  prev_lon = 0;
/* Наибольшее продвижение вдоль последнего отрезка с момента его записи (плоские единицы 1e-7 градуса). */
static int64_t  max_along = 0;

/*
 * Цепочка подряд отброшенных выбросов (скачок быстрее LOGGER_MAX_SPEED_MPS от
 * последней записанной точки). Если опорная точка сама оказалась ошибочной
 * (например, первый фикс после холодного старта), все хорошие точки выглядели бы
 * выбросами. Поэтому LOGGER_OUTLIER_RESET_COUNT подряд отброшенных точек, которые
 * согласуются друг с другом (каждая не дальше LOGGER_MAX_SPEED_MPS * dt от
 * предыдущей отброшенной), считаются новым положением: последняя из них
 * записывается и становится опорной.
 */
static uint8_t  outlier_count = 0;
static int32_t  outlier_lat = 0;
static int32_t  outlier_lon = 0;
static uint32_t outlier_time = 0;

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
 * The size is TRACK_RAM_MAX_POINTS (see track_logger.h): 1024 points use
 * exactly 8 KiB of RAM (1024 * 8 bytes), a deterministic footprint for the
 * target STM32 platform.
 */
static track_point_t ram_track_buffer[TRACK_RAM_MAX_POINTS];
static size_t ram_track_head = 0;
static size_t ram_track_count = 0;
/* Сколько точек всего попало в RAM-трек (монотонно, при очистке буфера не сбрасывается). */
static uint32_t ram_track_serial = 0;
/*
 * Заголовок файла GPX 1.1. Пространства имён:
 *   (по умолчанию) GPX 1.1 - обязательно для Garmin (BaseCamp, Connect) и большинства программ;
 *   gpxtpx - Garmin TrackPointExtension v2 (скорость, курс; в v1 их нет);
 *   pg     - собственные теги PurrGO (hacc, bat, vbat, rej).
 */
static const char* GPX_HEADER =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<gpx version=\"1.1\" creator=\"PurrGo\" "
    "xmlns=\"http://www.topografix.com/GPX/1/1\" "
    "xmlns:gpxtpx=\"http://www.garmin.com/xmlschemas/TrackPointExtension/v2\" "
    "xmlns:pg=\"https://github.com/purrrock/purrgo\" "
    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
    "xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 "
    "http://www.topografix.com/GPX/1/1/gpx.xsd "
    "http://www.garmin.com/xmlschemas/TrackPointExtension/v2 "
    "http://www8.garmin.com/xmlschemas/TrackPointExtensionv2.xsd\">\n"
    "<trk><trkseg>\n";

// Закрывающие теги
static const char* GPX_FOOTER = 
    "</trkseg></trk>\n"
    "</gpx>\n";

/* Начало нового трека (новый файл): приложение сбрасывает дистанцию трека и т.п. */
static purrgo_logger_new_track_cb_t s_new_track_cb = NULL;

void purrgo_logger_set_new_track_callback(purrgo_logger_new_track_cb_t cb) {
    s_new_track_cb = cb;
}

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
    PURRGO_LOGE("Track logger error: %s\r\n", reason);
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

    // Передача накопленных данных (до 512 байт) во FatFs. На носитель FAT и размер
    // файла попадут при ближайшем sync_active_file() или закрытии файла.
    uint32_t written = purrgo_fs_write(active_file, (const uint8_t*)write_buffer, (uint32_t)buffer_pos);
    if (written < buffer_pos) {
        // Часть данных могла дойти до файла: оставляем в буфере только хвост.
        memmove(write_buffer, write_buffer + written, buffer_pos - written);
        buffer_pos -= written;
        return false;
    }
    buffer_pos = 0;
    return true;
}

// Сброс буфера и синхронизация FAT-таблицы (защита от внезапного отключения питания).
static bool sync_active_file(void) {
    if (active_file == NULL) {
        return false;
    }
    if (!flush_buffer()) {
        return false;
    }
    // Данные переданы FatFs, но на носитель не попали: файл нужно переоткрыть.
    return purrgo_fs_sync(active_file);
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

#define LOGGER_NAME_MAX_ATTEMPTS 8U

/*
 * Фильтры качества и шума. Параметр, равный 0 (приёмник его не присылает: нет
 * GSA/GGA), считается неизвестным и точку не отклоняет.
 */
#define LOGGER_MIN_FIX_TYPE        3U      /* только 3D-фикс (GSA) */
#define LOGGER_MAX_HDOP_100        500     /* HDOP > 5.0 - точку не пишем (если нет hAcc) */
#define LOGGER_MAX_HACC_MM         ((uint32_t)PURRGO_GNSS_TRACK_MAX_HACC_M * 1000U)  /* hAcc выше - не пишем */
#define LOGGER_MIN_SATELLITES      4U      /* меньше 4 спутников в решении - не пишем */
#define LOGGER_STATIONARY_KNOTS_100 50     /* < 0.5 уз - стоим, дрейф координат - шум */
#define LOGGER_MAX_SPEED_MPS       500U    /* быстрее - выброс (скачок координат) */
#define LOGGER_OUTLIER_RESET_COUNT 5U      /* столько согласованных выбросов подряд - сброс опорной точки */

/*
 * Прореживание: точка пишется, только если трек в этом месте изменился заметно.
 * Критерии (любой из них; timeout - в любом случае):
 *   - отклонение точки от продолжения последнего записанного отрезка (прямой
 *     prev -> last) >= deviation_m: трек повернул, ускорение вдоль прямой не
 *     считается изменением;
 *   - разворот (точка позади последней вдоль направления отрезка);
 *   - расстояние от последней точки >= max_chord_m (предел длины прямой хорды,
 *     иначе на шоссе точка раз в несколько километров);
 *   - прошло timeout_s секунд (точка со стоянки: время и напряжение батареи).
 * Ближе min_step_m от последней записанной точки точка не пишется никогда
 * (кроме timeout): это шум приёмника.
 */
typedef struct {
    uint32_t min_step_m;
    uint32_t deviation_m;
    uint32_t max_chord_m;
    uint32_t timeout_s;
} logger_filter_params_t;

static const logger_filter_params_t FILTER_STANDARD   = {  5U,  5U,  250U,  300U };
static const logger_filter_params_t FILTER_EXPEDITION = { 10U, 10U, 1000U,  900U };

static const logger_filter_params_t* filter_params(void) {
    return (current_mode == LOGGER_MODE_EXPEDITION) ? &FILTER_EXPEDITION : &FILTER_STANDARD;
}

/* Причины отбраковки точки (pg:rej в диагностическом режиме). */
#define LOGGER_REJ_FIX_TYPE   0x01U   /* fix_type < 3 */
#define LOGGER_REJ_HDOP       0x02U   /* HDOP > 5.0 */
#define LOGGER_REJ_SATS       0x04U   /* меньше 4 спутников в решении */
#define LOGGER_REJ_STATIONARY 0x08U   /* стоим: скорость < 0.5 уз */
#define LOGGER_REJ_OUTLIER    0x10U   /* скачок быстрее LOGGER_MAX_SPEED_MPS */
#define LOGGER_REJ_DECIMATED  0x20U   /* трек не изменился заметно (прореживание) */
#define LOGGER_REJ_HACC       0x40U   /* hAcc (UBX-NAV-PVT) выше PURRGO_GNSS_TRACK_MAX_HACC_M */

static uint8_t quality_reject_mask(const purrgo_gnss_solution_t* fix) {
    uint8_t mask = 0U;
    if (fix->fix_type != 0 && fix->fix_type < LOGGER_MIN_FIX_TYPE) {
        mask |= LOGGER_REJ_FIX_TYPE;
    }
    if (fix->acc_valid) {
        /* Прямая оценка погрешности точнее HDOP: HDOP не учитывает качество сигнала. */
        if (fix->hacc_mm > LOGGER_MAX_HACC_MM) {
            mask |= LOGGER_REJ_HACC;
        }
    } else if (fix->hdop_100 > LOGGER_MAX_HDOP_100) {
        mask |= LOGGER_REJ_HDOP;
    }
    if (fix->satellites_tracked != 0 && fix->satellites_tracked < LOGGER_MIN_SATELLITES) {
        mask |= LOGGER_REJ_SATS;
    }
    return mask;
}

static uint64_t isqrt_u64(uint64_t n) {
    uint64_t res = 0;
    uint64_t bit = 1ULL << 62;
    while (bit > n) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

/* Смещение to относительно from в плоских единицах 1e-7 градуса (долгота - с поправкой на cos широты). */
static void local_offset(int32_t from_lat, int32_t from_lon, int32_t cos_10k,
                         int32_t to_lat, int32_t to_lon, int64_t* dx, int64_t* dy) {
    int64_t dlon = purrgo_geo_wrap_lon((int64_t)to_lon - (int64_t)from_lon);
    *dx = dlon * cos_10k / 10000;
    *dy = (int64_t)to_lat - (int64_t)from_lat;
}

/*
 * true - точка (lat, lon) заметно отличается от продолжения последнего отрезка
 * prev -> last: она дальше deviation_m от его прямой (поворот) либо вернулась назад
 * вдоль отрезка больше чем на deviation_m от наибольшего продвижения (разворот:
 * без этого прогулка "туда и обратно" по одной линии осталась бы без точки
 * разворота). Целочисленно: |cross| >= deviation * |u|, где u - вектор отрезка,
 * v - вектор от last до точки. Поправка на cos широты берётся в last: для
 * хорд до десятков километров ошибка пренебрежима. Неправдоподобно большие
 * векторы (больше ~12 000 км) считаются изменением. Побочный эффект: обновляет
 * max_along (вызывать для каждой проверяемой точки).
 */
static bool deviates_from_last_segment(int32_t lat, int32_t lon, uint32_t deviation_m) {
    const int64_t limit = 1LL << 30;
    int32_t cos_10k = purrgo_geo_cos_10k(last_recorded_lat);
    int64_t ux, uy, vx, vy;

    local_offset(prev_lat, prev_lon, cos_10k, last_recorded_lat, last_recorded_lon, &ux, &uy);
    local_offset(last_recorded_lat, last_recorded_lon, cos_10k, lat, lon, &vx, &vy);

    if (ux > limit || ux < -limit || uy > limit || uy < -limit ||
        vx > limit || vx < -limit || vy > limit || vy < -limit) {
        return true;
    }

    int64_t ulen = (int64_t)isqrt_u64((uint64_t)(ux * ux + uy * uy));
    if (ulen == 0) {
        return true;
    }

    const int64_t dev_units = (int64_t)deviation_m * PURRGO_1E7_PER_METER;
    int64_t along = (ux * vx + uy * vy) / ulen;
    if (along > max_along) {
        max_along = along;
    }
    if (along < max_along - dev_units) {
        return true;    /* разворот */
    }

    int64_t cross = ux * vy - uy * vx;
    if (cross < 0) {
        cross = -cross;
    }
    return cross >= dev_units * ulen;
}

/*
 * Учитывает отброшенный выброс. true - цепочка достигла LOGGER_OUTLIER_RESET_COUNT:
 * опорную точку нужно заменить текущей (счётчик сброшен).
 */
static bool outlier_note(const purrgo_gnss_solution_t* fix, uint32_t utc_epoch) {
    bool consistent = false;

    if (outlier_count != 0U) {
        uint32_t dist = purrgo_geo_distance_m(outlier_lat, outlier_lon, fix->lat_1e7, fix->lon_1e7);
        uint32_t dt = (utc_epoch >= outlier_time) ? (utc_epoch - outlier_time) : 0U;
        uint64_t dt_eff = (dt != 0U) ? dt : 1U;
        consistent = ((uint64_t)dist <= (uint64_t)LOGGER_MAX_SPEED_MPS * dt_eff);
    }

    outlier_count = consistent ? (uint8_t)(outlier_count + 1U) : 1U;
    outlier_lat = fix->lat_1e7;
    outlier_lon = fix->lon_1e7;
    outlier_time = utc_epoch;

    if (outlier_count >= LOGGER_OUTLIER_RESET_COUNT) {
        outlier_count = 0U;
        return true;
    }
    return false;
}

static void make_track_filename(char* out, size_t out_size, uint32_t track_id) {
    const char* prefix = purrgo_fs_get_tracks_path();

    if (prefix[0] != '\0') {
        purrgo_snprintf(out, out_size, "%s/%08X.gpx", prefix, (unsigned int)track_id);
    } else {
        purrgo_snprintf(out, out_size, "%08X.gpx", (unsigned int)track_id);
    }
}

static void logger_stop_impl(bool clear_ram);

/*
 * keep_ram: не очищать RAM-буфер трека (смена суток, переход на новый файл после
 * сбоя): уже нарисованный на экране трек не должен пропадать.
 */
static bool logger_start_impl(const purrgo_gnss_solution_t* first_fix, bool keep_ram) {
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
        PURRGO_LOGW("Track logging: cannot resume %s, starting new file\r\n", s_active_filename);
        s_active_filename[0] = '\0';
        buffer_pos = 0;
        keep_ram = true;    /* сессия продолжается в новом файле */
    }

    purrgo_gnss_solution_t local_fix;
    if (!purrgo_time_apply_timezone(first_fix, &local_fix, app_config.tz_offset_minutes)) {
        return false;
    }

    // Запоминаем ЛОКАЛЬНЫЙ день старта для проверки смены суток
    current_track_day = local_fix.day;

    /*
     * Имя файла: UTC-секунды от 01.01.2000 (эпоха purrgo_time) в HEX.
     * 8 символов + .gpx - формат FAT 8.3. Фиксированная ширина обеспечивает
     * сортировку по имени (до 2136 года). Не зависит от часового пояса и
     * не может переполниться при любой допустимой дате. Имена прежнего формата
     * (секунды от 01.01.2026 по местному времени) числово меньше и сортируются
     * раньше новых.
     */
    uint32_t track_id = 0;
    if (!purrgo_time_datetime_to_epoch(first_fix->year, first_fix->month, first_fix->day,
                                       first_fix->hours, first_fix->minutes, first_fix->seconds,
                                       &track_id)) {
        return false;
    }

    /*
     * Не затираем существующий файл (FS_WRITE_CREATE = FA_CREATE_ALWAYS):
     * при совпадении имени (перезапуск в ту же секунду) берём следующий номер.
     */
    char filename[64];
    bool name_found = false;
    for (uint32_t attempt = 0; attempt < LOGGER_NAME_MAX_ATTEMPTS; attempt++, track_id++) {
        make_track_filename(filename, sizeof(filename), track_id);
        /* purrgo_fs_exists(): без открытия файла, поэтому нет ложного "open error: 4" в логе */
        if (!purrgo_fs_exists(filename)) {
            name_found = true;
            break;
        }
    }
    if (!name_found) {
        PURRGO_LOGE("Track logger: no free file name\r\n");
        current_state = LOGGER_STATE_ERROR;
        return false;
    }

    active_file = purrgo_fs_open(filename, FS_WRITE_CREATE);
    if (!active_file) {
        current_state = LOGGER_STATE_ERROR;
        return false;
    }

    purrgo_snprintf(s_active_filename, sizeof(s_active_filename), "%.*s", (int)(sizeof(s_active_filename) - 1), filename);

    buffer_pos = 0;
    is_first_point = true;
    last_sync_time = 0;
    points_since_sync = 0;
    prev_valid = false;
    max_along = 0;
    outlier_count = 0;

    // Clear the RAM buffer for the new session
    if (!keep_ram) {
        ram_track_head = 0;
        ram_track_count = 0;
    }

    if (!write_to_buffer(GPX_HEADER)) {
        // Заголовок (< размера буфера) в пустой буфер помещается всегда.
        enter_error_state("header");
        return false;
    }
    current_state = LOGGER_STATE_RECORDING;
    if (s_new_track_cb != NULL) {
        s_new_track_cb();   /* новый файл; продолжение после сбоя записи сюда не доходит */
    }
    return true;
}

bool purrgo_logger_start(const purrgo_gnss_solution_t* first_fix) {
    return logger_start_impl(first_fix, false);
}

/*
 * Периодическая синхронизация FAT. Точка уже в буфере (и, для записанной, в
 * RAM-треке), поэтому при сбое она не теряется: после дозаписи файла уйдёт на SD.
 */
static void sync_if_due(uint32_t utc_epoch) {
    if (last_sync_time == 0) {
        last_sync_time = utc_epoch;
    }
    points_since_sync++;
    if (points_since_sync >= LOGGER_SYNC_POINTS ||
        (utc_epoch >= last_sync_time && utc_epoch - last_sync_time >= LOGGER_SYNC_INTERVAL_S)) {
        if (sync_active_file()) {
            points_since_sync = 0;
            last_sync_time = utc_epoch;
        } else {
            enter_error_state("sync");
        }
    }
}

/* Значение <fix> GPX 1.1 по данным GSA/GGA; NULL - неизвестно (приёмник не присылает). */
static const char* gpx_fix_name(const purrgo_gnss_solution_t* fix) {
    switch (fix->fix_type) {
        case 1:  return "none";
        case 2:  return "2d";
        case 3:  return (fix->fix_quality == 2U) ? "dgps" : "3d";
        default: return NULL;
    }
}

/*
 * Формирует <trkpt> и кладёт его в буфер записи. Порядок дочерних тегов - по схеме
 * GPX 1.1: ele, time, fix, sat, hdop, extensions. Параметры, равные 0 (приёмник их
 * не присылает), не пишутся.
 *   <fix>, <sat>, <hdop> - из GSA/GGA: нужны, чтобы сверять фильтры качества;
 *   gpxtpx:speed, gpxtpx:course - скорость (м/с) и курс (градусы) в формате Garmin
 *            TrackPointExtension v2;
 *   pg:hacc - горизонтальная точность hAcc, метры (UBX-NAV-PVT; без PVT не пишется);
 *   pg:bat, pg:vbat - заряд батареи (для анализа энергопотребления);
 *   pg:sacc, pg:rej - только в диагностической сборке, см. PURRGO_TRACK_DIAG.
 * ВАЖНО: внутри GPX (тег <time>) пишутся оригинальные данные fix (UTC время).
 * Возвращает false при сбое записи (логгер уходит в ERROR, повтор - из app_fsm).
 */
static bool buffer_point(const purrgo_gnss_solution_t* fix, uint8_t rej) {
    int32_t lat_abs = fix->lat_1e7 < 0 ? -fix->lat_1e7 : fix->lat_1e7;
    int32_t lon_abs = fix->lon_1e7 < 0 ? -fix->lon_1e7 : fix->lon_1e7;
    (void)rej;

    char opt[96] = "";
    size_t opt_len = 0U;
    const char* fix_name = gpx_fix_name(fix);
    if (fix_name != NULL) {
        opt_len += (size_t)purrgo_snprintf(opt + opt_len, sizeof(opt) - opt_len,
                                           "  <fix>%s</fix>\n", fix_name);
    }
    if (fix->satellites_tracked != 0U) {
        opt_len += (size_t)purrgo_snprintf(opt + opt_len, sizeof(opt) - opt_len,
                                           "  <sat>%u</sat>\n", (unsigned int)fix->satellites_tracked);
    }
    if (fix->hdop_100 > 0) {
        int32_t hdop = fix->hdop_100 > 9999 ? 9999 : fix->hdop_100;
        opt_len += (size_t)purrgo_snprintf(opt + opt_len, sizeof(opt) - opt_len,
                                           "  <hdop>%d.%02d</hdop>\n", (int)(hdop / 100), (int)(hdop % 100));
    }
    if (opt_len >= sizeof(opt)) {
        return false;   /* не должно случаться: размер буфера рассчитан на максимум */
    }

    /*
     * <extensions>: gpxtpx:speed - скорость над землёй, м/с (Garmin TrackPointExtension v2),
     * gpxtpx:course - курс, градусы (только при надёжном курсе); дальше собственные теги.
     */
    char ext[320];
    int ext_len = purrgo_snprintf(ext, sizeof(ext), "  <extensions><gpxtpx:TrackPointExtension>");
    int32_t spd_knots = fix->speed_knots < 0 ? 0 : (fix->speed_knots > 9999999 ? 9999999 : fix->speed_knots);
    uint32_t mps_100 = (uint32_t)(((int64_t)spd_knots * 1852) / 3600);   /* сотые доли узла -> сотые доли м/с */
    ext_len += purrgo_snprintf(ext + ext_len, sizeof(ext) - (size_t)ext_len,
                               "<gpxtpx:speed>%u.%02u</gpxtpx:speed>",
                               (unsigned int)(mps_100 / 100U), (unsigned int)(mps_100 % 100U));
    if (fix->course_valid && fix->course_deg_100 >= 0) {
        uint32_t crs = (uint32_t)fix->course_deg_100 % 36000U;
        ext_len += purrgo_snprintf(ext + ext_len, sizeof(ext) - (size_t)ext_len,
                                   "<gpxtpx:course>%u.%02u</gpxtpx:course>",
                                   (unsigned int)(crs / 100U), (unsigned int)(crs % 100U));
    }
    ext_len += purrgo_snprintf(ext + ext_len, sizeof(ext) - (size_t)ext_len, "</gpxtpx:TrackPointExtension>");
    if (fix->acc_valid) {
        uint32_t hacc = fix->hacc_mm > 9999999U ? 9999999U : fix->hacc_mm;   /* мм -> м, 2 знака */
        ext_len += purrgo_snprintf(ext + ext_len, sizeof(ext) - (size_t)ext_len,
                                   "<pg:hacc>%u.%02u</pg:hacc>",
                                   (unsigned int)(hacc / 1000U), (unsigned int)((hacc % 1000U) / 10U));
    }
    purrgo_battery_t bat;
    if (purrgo_battery_read(&bat) && bat.percent >= 0) {
        ext_len += purrgo_snprintf(ext + ext_len, sizeof(ext) - (size_t)ext_len,
                                   "<pg:bat>%d</pg:bat>", (int)bat.percent);
        if (bat.millivolts != 0) {
            ext_len += purrgo_snprintf(ext + ext_len, sizeof(ext) - (size_t)ext_len,
                                       "<pg:vbat>%u</pg:vbat>", (unsigned int)bat.millivolts);
        }
    }
#if PURRGO_TRACK_DIAG
    if (fix->acc_valid) {
        /* sAcc UBX-NAV-PVT, мм/с -> м/с: для сверки порога стоянки (скорость на месте сравнивается с sAcc). */
        uint32_t sacc = fix->sacc_mm_s > 9999999U ? 9999999U : fix->sacc_mm_s;
        ext_len += purrgo_snprintf(ext + ext_len, sizeof(ext) - (size_t)ext_len,
                                   "<pg:sacc>%u.%03u</pg:sacc>",
                                   (unsigned int)(sacc / 1000U), (unsigned int)(sacc % 1000U));
    }
    ext_len += purrgo_snprintf(ext + ext_len, sizeof(ext) - (size_t)ext_len, "<pg:rej>%u</pg:rej>", (unsigned int)rej);
#endif
    ext_len += purrgo_snprintf(ext + ext_len, sizeof(ext) - (size_t)ext_len, "</extensions>\n");
    if (ext_len < 0 || (size_t)ext_len >= sizeof(ext)) {
        return false;
    }

    char point_str[640];
    int n = purrgo_snprintf(point_str, sizeof(point_str),
             "<trkpt lat=\"%s%d.%07d\" lon=\"%s%d.%07d\">\n"
             "  <ele>%d</ele>\n"
             "  <time>20%02d-%02d-%02dT%02d:%02d:%02dZ</time>\n"
             "%s"
             "%s"
             "</trkpt>\n",
             fix->lat_1e7 < 0 ? "-" : "", (int)(lat_abs / 10000000), (int)(lat_abs % 10000000),
             fix->lon_1e7 < 0 ? "-" : "", (int)(lon_abs / 10000000), (int)(lon_abs % 10000000),
             (int)fix->alt_m,
             fix->year, fix->month, fix->day,
             fix->hours, fix->minutes, fix->seconds,
             opt, ext);
    if (n < 0 || (size_t)n >= sizeof(point_str)) {
        return false;
    }

    if (!write_to_buffer(point_str)) {
        enter_error_state("write");
        return false;
    }
    return true;
}

/*
 * Точка отбракована фильтром. Обычная сборка: ничего не пишет. Диагностическая:
 * пишет её в GPX с маской причин (в RAM-трек и в состояние фильтров не попадает).
 * Всегда возвращает false - как purrgo_logger_add_point() для незаписанной точки.
 */
static bool reject_point(const purrgo_gnss_solution_t* fix, uint8_t rej) {
#if PURRGO_TRACK_DIAG
    uint32_t utc_epoch = 0;
    if (current_state == LOGGER_STATE_RECORDING &&
        purrgo_time_datetime_to_epoch(fix->year, fix->month, fix->day,
                                      fix->hours, fix->minutes, fix->seconds, &utc_epoch) &&
        buffer_point(fix, rej)) {
        sync_if_due(utc_epoch);
    }
#else
    (void)fix;
    (void)rej;
#endif
    return false;
}

bool purrgo_logger_add_point(const purrgo_gnss_solution_t* fix) {
    if (current_mode == LOGGER_MODE_OFF) return false;
    if (current_state != LOGGER_STATE_RECORDING || !fix || !fix->valid) return false;
    uint8_t quality_rej = quality_reject_mask(fix);
    if (quality_rej != 0U) return reject_point(fix, quality_rej);

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
        logger_stop_impl(false);
        if (!logger_start_impl(fix, true)) {
            return false;
        }
    }

    bool should_record = false;
    bool reference_reset = false;   /* точка записывается как новая опорная: направление отрезка неизвестно */
    uint32_t step_m = 0;            /* расстояние от последней записанной точки */

    // Логика фильтрации (Decimation)
    if (is_first_point) {
        should_record = true;
        reference_reset = true;
    } else {
        const logger_filter_params_t* fp = filter_params();
        uint32_t dist_m = purrgo_geo_distance_m(last_recorded_lat, last_recorded_lon, fix->lat_1e7, fix->lon_1e7);
        uint32_t dt_s = (utc_epoch >= last_recorded_time) ? (utc_epoch - last_recorded_time) : 0;

        bool timeout = (dt_s >= fp->timeout_s);
        step_m = dist_m;

        // Выброс: скачок координат быстрее LOGGER_MAX_SPEED_MPS (dt < 1 с считаем за 1 с).
        uint64_t dt_eff = (dt_s != 0) ? dt_s : 1;
        if ((uint64_t)dist_m > (uint64_t)LOGGER_MAX_SPEED_MPS * dt_eff) {
            // Точка отбрасывается, пока цепочка выбросов не наберёт LOGGER_OUTLIER_RESET_COUNT:
            // тогда опорная точка считается ошибочной, и эта точка записывается как новая.
            if (!outlier_note(fix, utc_epoch)) {
                return reject_point(fix, LOGGER_REJ_OUTLIER);
            }
            should_record = true;
            reference_reset = true;
        } else {
            outlier_count = 0;

            // Стоим на месте: смещение в пределах шума GNSS не пишем, остаётся точка по таймауту.
            if (!timeout && fix->speed_knots < LOGGER_STATIONARY_KNOTS_100) {
                return reject_point(fix, LOGGER_REJ_STATIONARY);
            }

            if (timeout) {
                should_record = true;
            } else if (dist_m >= fp->min_step_m) {
                // Направления ещё нет - берём первую точку за пределами шума; дальше только
                // заметное изменение: поворот/разворот, предельная хорда.
                should_record = !prev_valid ||
                                dist_m >= fp->max_chord_m ||
                                deviates_from_last_segment(fix->lat_1e7, fix->lon_1e7, fp->deviation_m);
            }
        }
    }

    if (!should_record) return reject_point(fix, LOGGER_REJ_DECIMATED);

    // Запись точки в файл.
    if (!buffer_point(fix, 0U)) {
        // Сбой записи на SD: точка потеряна, логгер уходит в ERROR (повтор - из app_fsm).
        return false;
    }

    // Направление нового отрезка: от прежней последней точки к этой, если он длиннее шума.
    if (reference_reset || step_m < filter_params()->min_step_m) {
        prev_valid = false;
    } else {
        prev_valid = true;
        prev_lat = last_recorded_lat;
        prev_lon = last_recorded_lon;
    }
    last_recorded_lat = fix->lat_1e7;
    last_recorded_lon = fix->lon_1e7;
    last_recorded_time = utc_epoch; // Фильтр хранит историю в UTC
    is_first_point = false;
    max_along = 0;
    // Also add to the RAM circular buffer
    ram_track_buffer[ram_track_head].lat_1e7 = fix->lat_1e7;
    ram_track_buffer[ram_track_head].lon_1e7 = fix->lon_1e7;
    ram_track_head = (ram_track_head + 1) % TRACK_RAM_MAX_POINTS;
    if (ram_track_count < TRACK_RAM_MAX_POINTS) {
        ram_track_count++;
    }
    ram_track_serial++;

    sync_if_due(utc_epoch);

    return true;
}

static void logger_stop_impl(bool clear_ram) {
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
            PURRGO_LOGE("Track logging stop: write error, file may be incomplete\r\n");
        }
    }
    buffer_pos = 0;
    s_active_filename[0] = '\0';
    current_state = LOGGER_STATE_IDLE;

    // Clear the RAM buffer
    if (clear_ram) {
        ram_track_head = 0;
        ram_track_count = 0;
    }
}

void purrgo_logger_stop(void) {
    logger_stop_impl(true);
}

void purrgo_logger_suspend(void) {
    logger_stop_impl(false);
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

uint32_t purrgo_logger_get_track_serial(void) {
    return ram_track_serial;
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

void purrgo_logger_reset(void) {
    if (active_file != NULL) {
        (void)purrgo_fs_close(active_file);
        active_file = NULL;
    }
    current_state = LOGGER_STATE_IDLE;
    current_mode = LOGGER_MODE_STANDARD;
    s_active_filename[0] = '\0';
    current_track_day = 0;
    is_first_point = true;
    last_recorded_lat = 0;
    last_recorded_lon = 0;
    last_recorded_time = 0;
    last_sync_time = 0;
    points_since_sync = 0;
    prev_valid = false;
    prev_lat = 0;
    prev_lon = 0;
    max_along = 0;
    outlier_count = 0;
    outlier_lat = 0;
    outlier_lon = 0;
    outlier_time = 0;
    buffer_pos = 0;
    ram_track_head = 0;
    ram_track_count = 0;
}
