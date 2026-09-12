#include "purrgo/config.h"
#include "purrgo/fs_hal.h"
#include "purrgo/logger.h"
#include "purrgo/purrgo_format.h"
#include <string.h>


#define CONFIG_MAX_SIZE 512


purrgo_config_t app_config;


/*
 * Внутренний парсер целочисленных значений.
 *
 * Реализован вместо atoi(), чтобы контролировать тип результата
 * и обработку переполнения.
 */
static int32_t parse_int32(const char* str)
{
    int64_t res = 0;
    int32_t sign = 1;

    /*
     * Пропуск пробелов.
     */
    while (*str == ' ' || *str == '\t') {
        str++;
    }

    /*
     * Определение знака.
     */
    if (*str == '-') {
        sign = -1;
        str++;
    }
    else if (*str == '+') {
        str++;
    }

    /*
     * Чтение числовой части с накоплением в int64_t.
     */
    while (*str >= '0' && *str <= '9') {
        int32_t digit = *str - '0';

        if (res > INT32_MAX / 10 || (res == INT32_MAX / 10 && digit > (sign == 1 ? 7 : 8))) {
            return sign == 1 ? INT32_MAX : INT32_MIN;
        }

        res = res * 10 + digit;
        str++;
    }

    if (sign == -1) {
        res = -res;
    }

    return (int32_t)res;
}


void purrgo_config_init(void)
{
    /*
     * Значения по умолчанию.
     */
    app_config.tz_offset_minutes = 180;
    app_config.log_mode = LOGGER_MODE_STANDARD;
    app_config.backlight_on = true;

    /*
     * До появления настройки POI они не отображались.
     *
     * Поэтому PURRGO_POI_MODE_NO сохраняет прежнее поведение после обновления
     * прошивки.
     */
    app_config.poi_mode = PURRGO_POI_MODE_NO;

    /*
     * Подписи по умолчанию выключены.
     *
     * На текущем этапе механизм отрисовки подписей ещё не реализован.
     */
    app_config.map_details = PURRGO_MAP_DETAILS_HIGH;

    /*
     * Отображение текущего трека по умолчанию включено.
     */


    /*
     * Видимость слоев карты.
     */
    app_config.layer_landuse = true;
    app_config.layer_water = false;
    app_config.layer_landuse_labels = true;
    app_config.layer_water_labels = false;
    app_config.layer_roads = true;
    app_config.layer_poi = true;
    app_config.layer_poi_labels = true;
    app_config.layer_route = false;
    app_config.layer_track = true;

    /*
     * Базовый путь к картам для PC-эмулятора.
     */
    purrgo_snprintf(
        app_config.map_dir,
        sizeof(app_config.map_dir),
        "%s",
        purrgo_fs_get_maps_path()
    );

    /*
     * Последние координаты.
     */
    app_config.last_lat_1e7 = 537135000;
    app_config.last_lon_1e7 = 284199000;
}


bool purrgo_config_load(void)
{
    /*
     * Сначала всегда загружаем значения по умолчанию.
     *
     * Поэтому отсутствие нового ключа в старом PURRGO.CFG
     * не приводит к использованию неинициализированного значения.
     */
    PURRGO_LOG("DEFAULT CONFIG VALUES SET\r\n");
    purrgo_config_init();

    PURRGO_LOG("LOADING CONFIG FROM FILE: %s\r\n", purrgo_fs_get_config_path());
    purrgo_file_t* file = purrgo_fs_open(
        purrgo_fs_get_config_path(),
        FS_READ
    );

    if (!file) {
        /*
         * Если файла нет, создаём его с дефолтными значениями.
         */
        purrgo_logger_set_mode(app_config.log_mode);
        PURRGO_LOG("FILE READ ERROR, SAVING DEFAULTS TO FILE: %s\r\n", purrgo_fs_get_config_path());
        purrgo_config_save();
        return false;
    }

    /*
     * Стековый буфер конфигурации.
     */
    char buf[CONFIG_MAX_SIZE];

    memset(buf, 0, sizeof(buf));

    uint32_t bytes_read = purrgo_fs_read(
        file,
        (uint8_t*)buf,
        (uint32_t)(sizeof(buf) - 1)
    );

    purrgo_fs_close(file);

    if (bytes_read == 0) {
        purrgo_config_init();
        purrgo_logger_set_mode(app_config.log_mode);
        return false;
    }

    char* line = buf;

    while (*line) {
        char* end = line;

        /*
         * Поиск конца строки.
         */
        while (
            *end &&
            *end != '\n' &&
            *end != '\r'
        ) {
            end++;
        }

        bool is_eof = (*end == '\0');

        /*
         * Временно превращаем конец строки в '\0'.
         */
        *end = '\0';

        /*
         * Поиск разделителя KEY=VALUE.
         */
        char* delim = strchr(line, '=');

        if (delim) {
            *delim = '\0';

            char* key = line;
            char* val = delim + 1;

            /*
             * Часовой пояс в минутах.
             */
            if (strcmp(key, "TZ_MIN") == 0) {
                int32_t tz_min = parse_int32(val);

                if (
                    tz_min >= -720 &&
                    tz_min <= 840
                ) {
                    app_config.tz_offset_minutes =
                        (int16_t)tz_min;
                }
            }

            /*
             * Каталог карт.
             */
            else if (strcmp(key, "MAP_DIR") == 0) {
                purrgo_snprintf(
                    app_config.map_dir,
                    sizeof(app_config.map_dir),
                    "%.*s",
                    (int)(sizeof(app_config.map_dir) - 1),
                    val
                );
            }

            /*
             * Последняя широта.
             */
            else if (strcmp(key, "LAST_LAT_1E7") == 0) {
                int32_t lat = parse_int32(val);

                if (
                    lat >= -900000000 &&
                    lat <= 900000000
                ) {
                    app_config.last_lat_1e7 = lat;
                }
            }

            /*
             * Последняя долгота.
             */
            else if (strcmp(key, "LAST_LON_1E7") == 0) {
                int32_t lon = parse_int32(val);

                if (
                    lon >= -1800000000 &&
                    lon <= 1800000000
                ) {
                    app_config.last_lon_1e7 = lon;
                }
            }

            /*
             * Глобальное включение POI.
             *
             * Поддержка старого формата.
             */
            else if (strcmp(key, "POI_ENABLED") == 0) {
                int32_t enabled = parse_int32(val);

                if (enabled == 0) {
                    app_config.poi_mode = PURRGO_POI_MODE_NO;
                }
                else if (enabled == 1) {
                    app_config.poi_mode = PURRGO_POI_MODE_CIRCLES;
                }
            }

            /*
             * Режим отображения POI.
             */
            else if (strcmp(key, "POI_MODE") == 0) {
                int32_t mode = parse_int32(val);

                if (
                    mode == PURRGO_POI_MODE_NO ||
                    mode == PURRGO_POI_MODE_CIRCLES ||
                    mode == PURRGO_POI_MODE_ICONS
                ) {
                    app_config.poi_mode = (purrgo_poi_mode_t)mode;
                }
            }

            /*
             * Детализация карты.
             */
            else if (strcmp(key, "MAP_DETAILS") == 0) {
                int32_t mode = parse_int32(val);

                if (
                    mode == PURRGO_MAP_DETAILS_LOW ||
                    mode == PURRGO_MAP_DETAILS_HIGH
                ) {
                    app_config.map_details = (purrgo_map_details_t)mode;
                }
            }

            /*
             * Режим записи трека.
             */
            else if (strcmp(key, "LOG_MODE") == 0) {
                int32_t mode = parse_int32(val);

                if (
                    mode == LOGGER_MODE_OFF ||
                    mode == LOGGER_MODE_STANDARD ||
                    mode == LOGGER_MODE_EXPEDITION
                ) {
                    app_config.log_mode = (track_logger_mode_t)mode;
                }
            }



            /*
             * Видимость слоев карты.
             */
            else if (strcmp(key, "LAYER_LANDUSE") == 0) {
                app_config.layer_landuse = (parse_int32(val) != 0);
            }
            else if (strcmp(key, "LAYER_WATER") == 0) {
                app_config.layer_water = (parse_int32(val) != 0);
            }
            else if (strcmp(key, "LAYER_LANDUSE_LABELS") == 0) {
                app_config.layer_landuse_labels = (parse_int32(val) != 0);
            }
            else if (strcmp(key, "LAYER_WATER_LABELS") == 0) {
                app_config.layer_water_labels = (parse_int32(val) != 0);
            }
            else if (strcmp(key, "LAYER_ROADS") == 0) {
                app_config.layer_roads = (parse_int32(val) != 0);
            }
            else if (strcmp(key, "LAYER_POI") == 0) {
                app_config.layer_poi = (parse_int32(val) != 0);
            }
            else if (strcmp(key, "LAYER_POI_LABELS") == 0) {
                app_config.layer_poi_labels = (parse_int32(val) != 0);
            }
            else if (strcmp(key, "LAYER_ROUTE") == 0) {
                app_config.layer_route = (parse_int32(val) != 0);
            }
            else if (strcmp(key, "LAYER_TRACK") == 0) {
                app_config.layer_track = (parse_int32(val) != 0);
            }
        }

        if (is_eof) {
            break;
        }

        line = end + 1;

        /*
         * Пропуск CR/LF между строками.
         */
        while (
            *line == '\n' ||
            *line == '\r'
        ) {
            line++;
        }
    }

    /*
     * Применяем загруженный режим записи трека.
     */
    purrgo_logger_set_mode(app_config.log_mode);

    return true;
}


bool purrgo_config_save(void)
{
    PURRGO_LOG(
        "Saving configuration to: %s\n\r",
        purrgo_fs_get_config_path()
    );

    purrgo_file_t* file = purrgo_fs_open(
        purrgo_fs_get_config_path(),
        FS_WRITE_CREATE
    );

    if (!file) {
        PURRGO_LOG(
            "purrgo_config_save: failed to open file\n\r"
        );

        return false;
    }

    char buf[CONFIG_MAX_SIZE];

    /*
     * Формируем текстовую конфигурацию.
     */
    int len = purrgo_snprintf(
        buf,
        sizeof(buf),

        "TZ_MIN=%d\n"
        "MAP_DIR=%.*s\n"
        "LAST_LAT_1E7=%d\n"
        "LAST_LON_1E7=%d\n"
        "POI_MODE=%d\n"

        "LOG_MODE=%d\n"
        "MAP_DETAILS=%d\n"
        "LAYER_LANDUSE=%d\n"
        "LAYER_WATER=%d\n"
        "LAYER_LANDUSE_LABELS=%d\n"
        "LAYER_WATER_LABELS=%d\n"
        "LAYER_ROADS=%d\n"
        "LAYER_POI=%d\n"
        "LAYER_POI_LABELS=%d\n"
        "LAYER_ROUTE=%d\n"
        "LAYER_TRACK=%d\n",

        (int)app_config.tz_offset_minutes,

        (int)(sizeof(app_config.map_dir) - 1),
        app_config.map_dir,

        (int)app_config.last_lat_1e7,

        (int)app_config.last_lon_1e7,

        (int)app_config.poi_mode,



        (int)app_config.log_mode,

        (int)app_config.map_details,

        app_config.layer_landuse ? 1 : 0,
        app_config.layer_water ? 1 : 0,
        app_config.layer_landuse_labels ? 1 : 0,
        app_config.layer_water_labels ? 1 : 0,
        app_config.layer_roads ? 1 : 0,
        app_config.layer_poi ? 1 : 0,
        app_config.layer_poi_labels ? 1 : 0,
        app_config.layer_route ? 1 : 0,
        app_config.layer_track ? 1 : 0
    );

    /*
     * purrgo_snprintf() возвращает количество символов,
     * которое потребовалось бы записать без учёта '\0'.
     * Если len >= sizeof(buf), конфигурация была бы обрезана.
     */
    if (
        len < 0 ||
        (size_t)len >= sizeof(buf)
    ) {
        purrgo_fs_close(file);
        return false;
    }

    /*
     * Проверяем фактическое количество записанных байт.
     */
    uint32_t written = purrgo_fs_write(
        file,
        (const uint8_t*)buf,
        (uint32_t)len
    );

    if (written != (uint32_t)len) {
        purrgo_fs_close(file);
        return false;
    }

    purrgo_fs_sync(file);

    purrgo_fs_close(file);

    return true;
}