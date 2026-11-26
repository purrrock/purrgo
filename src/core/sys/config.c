#include "purrgo/config.h"
#include "purrgo/fs_hal.h"
#include "purrgo/logger.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/purrgo_time.h"
#include "purrgo/supply.h"
#include "purrgo/utc_hint.h"
#include <string.h>


/*
 * Размер буфера чтения PURRGO.CFG: с запасом под комментарии и пустые строки
 * при ручной правке (строки за пределами буфера не читаются).
 */
#define CONFIG_MAX_SIZE 1024

/*
 * Буфер записи: наихудший случай - около 250 байт полей и слоёв плюс MAP_DIR
 * (до PURRGO_FS_MAX_PATH + 31 символов), ROUTE_FILE (до PURRGO_ROUTE_NAME_MAX)
 * и позиция чтения (BOOK_FILE до PURRGO_BOOK_NAME_MAX, BOOK_CRC, BOOK_PAGE).
 */
#define CONFIG_SAVE_MAX_SIZE 1024


purrgo_config_t app_config;

/* Хук перед сохранением (см. purrgo_config_set_presave_hook()). */
static purrgo_config_presave_fn s_presave_hook;

void purrgo_config_set_presave_hook(purrgo_config_presave_fn fn)
{
    s_presave_hook = fn;
}


/*
 * Слои карты: единственная таблица для файла PURRGO.CFG, меню MAP LAYERS и
 * значений по умолчанию. Индекс в таблице = purrgo_layer_t.
 */
static const struct {
    const char* key;      /* LAYER_<key>=0|1 в PURRGO.CFG */
    const char* title;    /* пункт меню */
    bool        on;       /* по умолчанию */
} s_layers[PURRGO_LAYER_COUNT] = {
    [PURRGO_LAYER_LANDUSE]        = { "LANDUSE",        "Landuse",        true  },
    [PURRGO_LAYER_WATER]          = { "WATER",          "Water",          true  },
    [PURRGO_LAYER_LANDUSE_LABELS] = { "LANDUSE_LABELS", "Landuse labels", true  },
    [PURRGO_LAYER_WATER_LABELS]   = { "WATER_LABELS",   "Water labels",   true  },
    [PURRGO_LAYER_ROADS]          = { "ROADS",          "Roads",          true  },
    [PURRGO_LAYER_POI]            = { "POI",            "POI",            true  },
    [PURRGO_LAYER_POI_LABELS]     = { "POI_LABELS",     "POI labels",     true  },
    [PURRGO_LAYER_ROUTE]          = { "ROUTE",          "Route",          false },
    [PURRGO_LAYER_WAYPOINT]       = { "WAYPOINT",       "Waypoints",      false },
    [PURRGO_LAYER_TRACK]          = { "TRACK",          "Track",          true  },
};

const char* purrgo_layer_title(purrgo_layer_t id)
{
    return ((unsigned)id < PURRGO_LAYER_COUNT) ? s_layers[id].title : "";
}

uint16_t purrgo_layers_default(void)
{
    uint16_t mask = 0;
    for (unsigned i = 0; i < PURRGO_LAYER_COUNT; i++) {
        if (s_layers[i].on) {
            mask = (uint16_t)(mask | (1u << i));
        }
    }
    return mask;
}

/* Слой по ключу из PURRGO.CFG (без префикса LAYER_); -1 - неизвестный ключ. */
static int layer_find(const char* key)
{
    for (int i = 0; i < PURRGO_LAYER_COUNT; i++) {
        if (strcmp(key, s_layers[i].key) == 0) {
            return i;
        }
    }
    return -1;
}


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


/* Шестнадцатеричное 32-битное значение (CRC); 0 при пустой строке или ошибке. */
static bool parse_hex32(const char* str, uint32_t* out)
{
    uint64_t res = 0;
    int digits = 0;

    while (*str == ' ' || *str == '\t') {
        str++;
    }
    for (; digits < 8; digits++, str++) {
        char c = *str;
        uint32_t d;
        if (c >= '0' && c <= '9') {
            d = (uint32_t)(c - '0');
        } else if (c >= 'A' && c <= 'F') {
            d = (uint32_t)(c - 'A') + 10u;
        } else if (c >= 'a' && c <= 'f') {
            d = (uint32_t)(c - 'a') + 10u;
        } else {
            break;
        }
        res = (res << 4) | d;
    }
    if (digits == 0 || (*str != '\0' && *str != ' ' && *str != '\t')) {
        return false;
    }
    *out = (uint32_t)res;
    return true;
}



/*
 * LAST_UTC: последнее известное время UTC в виде YYYY-MM-DDTHH:MM:SSZ (удобно править руками). В RAM -
 * секунды от 01.01.2000 (app_config.last_utc_epoch). Пустое значение или другой формат - время неизвестно.
 */
#define UTC_TEXT_LEN 20U   /* "2026-11-25T10:20:30Z" */

static bool utc_digits(const char *p, unsigned n, uint32_t *out)
{
    uint32_t v = 0U;

    for (unsigned i = 0U; i < n; i++) {
        if (p[i] < '0' || p[i] > '9') {
            return false;
        }
        v = v * 10U + (uint32_t)(p[i] - '0');
    }
    *out = v;
    return true;
}

static bool parse_utc_text(const char *val, uint32_t *epoch)
{
    uint32_t y, mo, d, h, mi, s;

    if (strlen(val) != UTC_TEXT_LEN ||
        val[4] != '-' || val[7] != '-' || val[10] != 'T' || val[13] != ':' || val[16] != ':' || val[19] != 'Z') {
        return false;
    }
    if (!utc_digits(val, 4U, &y) || !utc_digits(val + 5, 2U, &mo) || !utc_digits(val + 8, 2U, &d) ||
        !utc_digits(val + 11, 2U, &h) || !utc_digits(val + 14, 2U, &mi) || !utc_digits(val + 17, 2U, &s)) {
        return false;
    }
    if (y < 2000U || y > 2099U) {
        return false;
    }
    return purrgo_time_datetime_to_epoch((uint8_t)(y - 2000U), (uint8_t)mo, (uint8_t)d,
                                         (uint8_t)h, (uint8_t)mi, (uint8_t)s, epoch);
}

/* Пустая строка, если время неизвестно (0) или вне 2000-2099. */
static void format_utc_text(uint32_t epoch, char *buf, size_t size)
{
    uint8_t y, mo, d, h, mi, s;

    buf[0] = '\0';
    if (epoch == 0U || !purrgo_time_epoch_to_datetime(epoch, &y, &mo, &d, &h, &mi, &s)) {
        return;
    }
    (void)purrgo_snprintf(buf, size, "%04u-%02u-%02uT%02u:%02u:%02uZ", 2000U + (unsigned)y,
                          (unsigned)mo, (unsigned)d, (unsigned)h, (unsigned)mi, (unsigned)s);
}

void purrgo_config_init(void)
{
    /* Полный сброс: поля, которых нет в списке ниже, тоже обнуляются. */
    memset(&app_config, 0, sizeof app_config);

    /* Хук привязан к состоянию контроллера карты, а оно сбрасывается вместе с настройками. */
    s_presave_hook = NULL;

    /* Последнее известное время хранится в конфигурации: сброс настроек сбрасывает и его. */
    purrgo_utc_hint_set_floor(0U);

    /*
     * Значения по умолчанию.
     */
    app_config.tz_offset_minutes = 180;
    app_config.log_mode = LOGGER_MODE_STANDARD;
    app_config.backlight_on = true;

    /*
     * POI по умолчанию отображаются иконками. Сохранённое в config значение
     * POI_MODE (в том числе NO) перекрывает умолчание.
     */
    app_config.poi_mode = PURRGO_POI_MODE_ICONS;

    /*
     * Подписи по умолчанию выключены.
     *
     * На текущем этапе механизм отрисовки подписей ещё не реализован.
     */
    app_config.map_details = PURRGO_MAP_DETAILS_HIGH;

    /*
     * Источник питания: основной тип элементов - Ni-MH.
     */
    app_config.power_source = PURRGO_POWER_SRC_DEFAULT;

    /* Запись лога на SD по умолчанию выключена. */
    app_config.sd_log = false;

    /* Звуковые сигналы по умолчанию выключены. */
    app_config.sound_level = PURRGO_SOUND_LEVEL_OFF;

    /*
     * Отображение текущего трека по умолчанию включено.
     */


    /*
     * Видимость слоев карты.
     */
    app_config.layers = purrgo_layers_default();

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
     * Маршрут по умолчанию не выбран.
     */
    app_config.route_file[0] = '\0';
    app_config.route_from_tracks = false;

    /*
     * Цель навигации не задана.
     */
    app_config.wpt_valid = false;
    app_config.wpt_lat_1e7 = 0;
    app_config.wpt_lon_1e7 = 0;
    app_config.wpt_name[0] = '\0';

    /*
     * Книга не открывалась.
     */
    app_config.book_file[0] = '\0';
    app_config.book_crc32 = 0;
    app_config.book_page = 0;

    /*
     * Центр карты на холодном старте, пока в PURRGO.CFG нет сохранённого.
     */
    app_config.last_lat_1e7 = 537135000;
    app_config.last_lon_1e7 = 284199000;
}


#define CONFIG_PATH_MAX 96

/* Путь временного файла для атомарного сохранения: <config>.TMP */
static bool config_make_tmp_path(char* out, size_t out_size)
{
    int n = purrgo_snprintf(out, out_size, "%s.TMP", purrgo_fs_get_config_path());
    return n > 0 && (size_t)n < out_size;
}

/*
 * Восстановление после прерванного сохранения.
 *
 * purrgo_fs_replace() на FatFs выполняется как "удалить старый, переименовать
 * временный", поэтому при потере питания между этими шагами PURRGO.CFG
 * отсутствует, а полный и синхронизированный PURRGO.CFG.TMP есть.
 * Если рабочего конфига нет, а временный файл заканчивается переводом
 * строки (то есть не оборван посреди записи), завершаем замену.
 */
static void config_recover_interrupted_save(void)
{
    const char* cfg_path = purrgo_fs_get_config_path();

    purrgo_file_t* file = purrgo_fs_open(cfg_path, FS_READ);
    if (file) {
        (void)purrgo_fs_close(file);
        return;
    }

    char tmp_path[CONFIG_PATH_MAX];
    if (!config_make_tmp_path(tmp_path, sizeof(tmp_path))) {
        return;
    }

    file = purrgo_fs_open(tmp_path, FS_READ);
    if (!file) {
        return;
    }

    char buf[CONFIG_MAX_SIZE];
    uint32_t n = purrgo_fs_read(file, (uint8_t*)buf, (uint32_t)sizeof(buf));
    (void)purrgo_fs_close(file);

    if (n > 0 && n < sizeof(buf) && buf[n - 1] == '\n') {
        PURRGO_LOGW("Recovering config from interrupted save\r\n");
        (void)purrgo_fs_replace(tmp_path, cfg_path);
    }
}

/*
 * Проверка MAP_DIR из PURRGO.CFG. Каталог карты - полный путь (например
 * "0:/PURRGO/MAPS/name"), поэтому '/' и ':' допустимы, в отличие от имён
 * ROUTE_FILE и BOOK_FILE. Отбрасываются обратная косая черта, управляющие
 * символы и компоненты "..". Сам путь к каталогу карт платформы ("../../.."
 * в PC-эмуляторе) проверке не подлежит, проверяется только остаток после него.
 */
static bool map_dir_is_valid(const char* path)
{
    if (path[0] == '\0') {
        return false;
    }

    const char* maps = purrgo_fs_get_maps_path();
    const size_t maps_len = strlen(maps);
    if (maps_len > 0 && strncmp(path, maps, maps_len) == 0 &&
        (path[maps_len] == '\0' || path[maps_len] == '/')) {
        path += maps_len;
    }

    for (const char* p = path; *p != '\0'; p++) {
        if (*p == '\\' || (unsigned char)*p < 0x20u) {
            return false;
        }
    }

    for (const char* p = path; *p != '\0'; ) {
        const char* end = strchr(p, '/');
        const size_t len = (end != NULL) ? (size_t)(end - p) : strlen(p);

        if (len == 2 && p[0] == '.' && p[1] == '.') {
            return false;
        }
        if (end == NULL) {
            break;
        }
        p = end + 1;
    }
    return true;
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

    config_recover_interrupted_save();

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
        PURRGO_LOGW("CONFIG FILE NOT FOUND, SAVING DEFAULTS TO FILE: %s\r\n", purrgo_fs_get_config_path());
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

    if (bytes_read == sizeof(buf) - 1) {
        PURRGO_LOGW("Config file is larger than %u bytes: the tail is ignored\r\n", (unsigned)(sizeof(buf) - 1));
    }

    if (bytes_read == 0) {
        purrgo_config_init();
        purrgo_logger_set_mode(app_config.log_mode);
        return false;
    }

    char* line = buf;

    /* Цель навигации собирается из нескольких ключей: задана, если есть SET, LAT и LON. */
    bool wpt_set = false;
    bool wpt_have_lat = false;
    bool wpt_have_lon = false;

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
                if (map_dir_is_valid(val)) {
                    purrgo_snprintf(
                        app_config.map_dir,
                        sizeof(app_config.map_dir),
                        "%.*s",
                        (int)(sizeof(app_config.map_dir) - 1),
                        val
                    );
                }
            }

            /*
             * Файл маршрута (только имя, без пути).
             */
            else if (strcmp(key, "ROUTE_FILE") == 0) {
                if (strchr(val, '/') == NULL && strchr(val, '\\') == NULL &&
                    val[0] != '.' &&
                    strlen(val) < sizeof(app_config.route_file)) {
                    purrgo_snprintf(
                        app_config.route_file,
                        sizeof(app_config.route_file),
                        "%s",
                        val
                    );
                }
            }

            else if (strcmp(key, "ROUTE_SRC") == 0) {
                app_config.route_from_tracks = (parse_int32(val) == 1);
            }

            /*
             * Цель навигации (Waypoint).
             */
            else if (strcmp(key, "WPT_SET") == 0) {
                wpt_set = (parse_int32(val) != 0);
            }
            else if (strcmp(key, "WPT_LAT") == 0) {
                int32_t lat = parse_int32(val);

                if (lat >= -900000000 && lat <= 900000000) {
                    app_config.wpt_lat_1e7 = lat;
                    wpt_have_lat = true;
                }
            }
            else if (strcmp(key, "WPT_LON") == 0) {
                int32_t lon = parse_int32(val);

                if (lon >= -1800000000 && lon <= 1800000000) {
                    app_config.wpt_lon_1e7 = lon;
                    wpt_have_lon = true;
                }
            }
            else if (strcmp(key, "WPT_NAME") == 0) {
                if (strlen(val) < sizeof(app_config.wpt_name)) {
                    purrgo_snprintf(
                        app_config.wpt_name,
                        sizeof(app_config.wpt_name),
                        "%s",
                        val
                    );
                }
            }

            /*
             * Позиция чтения: файл книги (только имя), CRC файла и страница.
             */
            else if (strcmp(key, "BOOK_FILE") == 0) {
                if (strchr(val, '/') == NULL && strchr(val, '\\') == NULL &&
                    val[0] != '.' &&
                    strlen(val) < sizeof(app_config.book_file)) {
                    purrgo_snprintf(
                        app_config.book_file,
                        sizeof(app_config.book_file),
                        "%s",
                        val
                    );
                }
            }
            else if (strcmp(key, "BOOK_CRC") == 0) {
                uint32_t crc;
                if (parse_hex32(val, &crc)) {
                    app_config.book_crc32 = crc;
                }
            }
            else if (strcmp(key, "BOOK_PAGE") == 0) {
                int32_t page = parse_int32(val);
                if (page >= 0) {
                    app_config.book_page = (uint32_t)page;
                }
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
             * Последнее известное время UTC (нижняя граница для времени приёмника и меток файлов).
             */
            else if (strcmp(key, "LAST_UTC") == 0) {
                uint32_t epoch;

                if (parse_utc_text(val, &epoch)) {
                    app_config.last_utc_epoch = epoch;
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
             * Источник питания.
             */
            else if (strcmp(key, "POWER_SRC") == 0) {
                int32_t src = parse_int32(val);

                if (src >= 0 && src < PURRGO_POWER_SRC_COUNT) {
                    app_config.power_source = (purrgo_power_source_t)src;
                }
            }

            /*
             * Запись диагностического лога на SD.
             */
            else if (strcmp(key, "SD_LOG") == 0) {
                const int32_t on = parse_int32(val);

                if (on == 0 || on == 1) {
                    app_config.sd_log = (on == 1);
                }
            }

            /*
             * Звуковые сигналы.
             */
            else if (strcmp(key, "SOUND") == 0) {
                /* 0 - OFF, 1 - LOW, 2 - HIGH. Старый файл с SOUND=1 (было ON) читается как LOW. */
                const int32_t level = parse_int32(val);

                if (level >= (int32_t)PURRGO_SOUND_LEVEL_OFF && level < (int32_t)PURRGO_SOUND_LEVEL_COUNT) {
                    app_config.sound_level = (purrgo_sound_level_t)level;
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
            else if (strncmp(key, "LAYER_", 6) == 0) {
                const int id = layer_find(key + 6);

                if (id >= 0) {
                    const uint16_t bit = (uint16_t)(1u << id);

                    if (parse_int32(val) != 0) {
                        app_config.layers = (uint16_t)(app_config.layers | bit);
                    }
                    else {
                        app_config.layers = (uint16_t)(app_config.layers & ~bit);
                    }
                }
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

    app_config.wpt_valid = wpt_set && wpt_have_lat && wpt_have_lon;

    purrgo_utc_hint_set_floor(app_config.last_utc_epoch);

    /*
     * Применяем загруженный режим записи трека.
     */
    purrgo_logger_set_mode(app_config.log_mode);

    return true;
}


bool purrgo_config_save(void)
{
    /* Низкое питание: запись на карту остановлена (purrgo/supply.h), настройки остаются в RAM. */
    if (purrgo_supply_storage_blocked()) {
        PURRGO_LOGW("purrgo_config_save: skipped, SD writes stopped (low supply)\n\r");
        return false;
    }

    /* Актуальный центр карты попадает в LAST_LAT_1E7 / LAST_LON_1E7 при каждой записи файла. */
    if (s_presave_hook != NULL) {
        s_presave_hook();
    }

    /*
     * Последнее известное время: оценка по данным приёмника в этом сеансе (не меньше уже сохранённого).
     * Нет времени в сеансе - остаётся прежнее значение из файла, оно не затирается.
     */
    uint32_t utc_now;
    if (purrgo_utc_hint_estimate(&utc_now) && utc_now > app_config.last_utc_epoch) {
        app_config.last_utc_epoch = utc_now;
    }
    char utc_text[UTC_TEXT_LEN + 1U];
    format_utc_text(app_config.last_utc_epoch, utc_text, sizeof utc_text);

    PURRGO_LOG(
        "Saving configuration to: %s\n\r",
        purrgo_fs_get_config_path()
    );

    /*
     * Атомарное сохранение: текст формируется заранее и пишется во
     * временный файл. Рабочий PURRGO.CFG заменяется только после того,
     * как временный файл полностью записан и синхронизирован.
     * Сбой питания или ошибка SD в середине записи оставляет старый
     * конфиг нетронутым.
     */
    char tmp_path[CONFIG_PATH_MAX];
    if (!config_make_tmp_path(tmp_path, sizeof(tmp_path))) {
        return false;
    }

    char buf[CONFIG_SAVE_MAX_SIZE];

    /*
     * Формируем текстовую конфигурацию.
     */
    int len = purrgo_snprintf(
        buf,
        sizeof(buf),

        "TZ_MIN=%d\n"
        "MAP_DIR=%.*s\n"
        "ROUTE_FILE=%.*s\n"
        "ROUTE_SRC=%d\n"
        "LAST_LAT_1E7=%d\n"
        "LAST_LON_1E7=%d\n"
        "LAST_UTC=%s\n"
        "POI_MODE=%d\n"

        "LOG_MODE=%d\n"
        "MAP_DETAILS=%d\n"
        "POWER_SRC=%d\n"
        "SD_LOG=%d\n"
        "SOUND=%d\n"
        "BOOK_FILE=%.*s\n"
        "BOOK_CRC=%08X\n"
        "BOOK_PAGE=%u\n"
        "WPT_SET=%d\n"
        "WPT_LAT=%d\n"
        "WPT_LON=%d\n"
        "WPT_NAME=%.*s\n",

        (int)app_config.tz_offset_minutes,

        (int)(sizeof(app_config.map_dir) - 1),
        app_config.map_dir,

        (int)(sizeof(app_config.route_file) - 1),
        app_config.route_file,

        app_config.route_from_tracks ? 1 : 0,

        (int)app_config.last_lat_1e7,

        (int)app_config.last_lon_1e7,

        utc_text,

        (int)app_config.poi_mode,



        (int)app_config.log_mode,

        (int)app_config.map_details,

        (int)app_config.power_source,

        app_config.sd_log ? 1 : 0,

        (int)app_config.sound_level,

        (int)(sizeof(app_config.book_file) - 1),
        app_config.book_file,

        (unsigned)app_config.book_crc32,

        (unsigned)app_config.book_page,

        app_config.wpt_valid ? 1 : 0,

        (int)app_config.wpt_lat_1e7,

        (int)app_config.wpt_lon_1e7,

        (int)(sizeof(app_config.wpt_name) - 1),
        app_config.wpt_name
    );

    /*
     * Слои карты: по строке LAYER_<ИМЯ>=0|1 на слой (текст для ручной правки).
     */
    for (int i = 0; len >= 0 && i < PURRGO_LAYER_COUNT; i++) {
        if ((size_t)len >= sizeof(buf)) {
            break;
        }
        const int n = purrgo_snprintf(
            buf + len,
            sizeof(buf) - (size_t)len,
            "LAYER_%s=%d\n",
            s_layers[i].key,
            purrgo_layer_on(app_config.layers, (purrgo_layer_t)i) ? 1 : 0
        );
        len = (n < 0) ? -1 : len + n;
    }

    /*
     * purrgo_snprintf() возвращает количество символов,
     * которое потребовалось бы записать без учёта '\0'.
     * Если len >= sizeof(buf), конфигурация была бы обрезана.
     */
    if (
        len < 0 ||
        (size_t)len >= sizeof(buf)
    ) {
        PURRGO_LOGE("purrgo_config_save: config does not fit buffer\n\r");
        return false;
    }

    purrgo_file_t* file = purrgo_fs_open(
        tmp_path,
        FS_WRITE_CREATE
    );

    if (!file) {
        PURRGO_LOGE(
            "purrgo_config_save: failed to open temporary file\n\r"
        );

        return false;
    }

    uint32_t written = purrgo_fs_write(
        file,
        (const uint8_t*)buf,
        (uint32_t)len
    );

    bool ok = (written == (uint32_t)len);

    if (ok) {
        ok = purrgo_fs_sync(file);
    }

    if (!purrgo_fs_close(file)) {
        ok = false;
    }

    if (!ok) {
        PURRGO_LOGE("purrgo_config_save: write error\n\r");
        return false;
    }

    if (!purrgo_fs_replace(tmp_path, purrgo_fs_get_config_path())) {
        PURRGO_LOGE("purrgo_config_save: replace error\n\r");
        return false;
    }

    return true;
}
