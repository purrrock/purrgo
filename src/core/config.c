#include "purrgo/config.h"
#include "purrgo/fs_hal.h"
#include "purrgo/logger.h"
#include "purrgo/purrgo_format.h"
#include <string.h>


#define CONFIG_MAX_SIZE 512


purrgo_config_t app_config;


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
    [PURRGO_LAYER_WATER]          = { "WATER",          "Water",          false },
    [PURRGO_LAYER_LANDUSE_LABELS] = { "LANDUSE_LABELS", "Landuse labels", true  },
    [PURRGO_LAYER_WATER_LABELS]   = { "WATER_LABELS",   "Water labels",   false },
    [PURRGO_LAYER_ROADS]          = { "ROADS",          "Roads",          true  },
    [PURRGO_LAYER_POI]            = { "POI",            "POI",            true  },
    [PURRGO_LAYER_POI_LABELS]     = { "POI_LABELS",     "POI labels",     true  },
    [PURRGO_LAYER_ROUTE]          = { "ROUTE",          "Route",          false },
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


void purrgo_config_init(void)
{
    /* Полный сброс: поля, которых нет в списке ниже, тоже обнуляются. */
    memset(&app_config, 0, sizeof app_config);

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
     * Последние координаты.
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
        PURRGO_LOG("Recovering config from interrupted save\r\n");
        (void)purrgo_fs_replace(tmp_path, cfg_path);
    }
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
        "MAP_DETAILS=%d\n",

        (int)app_config.tz_offset_minutes,

        (int)(sizeof(app_config.map_dir) - 1),
        app_config.map_dir,

        (int)app_config.last_lat_1e7,

        (int)app_config.last_lon_1e7,

        (int)app_config.poi_mode,



        (int)app_config.log_mode,

        (int)app_config.map_details
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
        PURRGO_LOG("purrgo_config_save: config does not fit buffer\n\r");
        return false;
    }

    purrgo_file_t* file = purrgo_fs_open(
        tmp_path,
        FS_WRITE_CREATE
    );

    if (!file) {
        PURRGO_LOG(
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
        PURRGO_LOG("purrgo_config_save: write error\n\r");
        return false;
    }

    if (!purrgo_fs_replace(tmp_path, purrgo_fs_get_config_path())) {
        PURRGO_LOG("purrgo_config_save: replace error\n\r");
        return false;
    }

    return true;
}
