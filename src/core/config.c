#include "purrgo/config.h"
#include "purrgo/fs_hal.h"
#include <stdio.h>
#include <string.h>

#define CONFIG_FILENAME "PURRGO.CFG"
#define CONFIG_MAX_SIZE 512

purrgo_config_t app_config;

// Внутренний парсер целочисленных значений (замена atoi для контроля над типами)
static int32_t parse_int32(const char* str) {
    int32_t res = 0;
    int32_t sign = 1;
    
    // Пропуск пробелов
    while (*str == ' ' || *str == '\t') str++;
    
    // Определение знака
    if (*str == '-') { sign = -1; str++; }
    else if (*str == '+') { str++; }
    
    // Чтение числовой части
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res * sign;
}

void purrgo_config_init(void) {
    app_config.tz_offset_minutes = 180; // UTC+3:00 по умолчанию[cite: 3]
    app_config.log_mode = LOGGER_MODE_STANDARD; //[cite: 3]
    app_config.backlight_on = true; //[cite: 3]
    
    // Базовые значения карт и координат (Минск)
   /*
     * Default map directory for the PC emulator.
     *
     * The emulator is normally launched from:
     *
     *     <repository>/build/apps/emulator/
     *
     * and test map data is located under:
     *
     *     <repository>/tests/data/maps/
     */
    strncpy(
        app_config.map_dir,
        "../../../tests/data/maps",
        sizeof(app_config.map_dir) - 1
    );
     app_config.map_dir[sizeof(app_config.map_dir) - 1] = '\0';

     app_config.last_lat_1e7 = 537135000;
     app_config.last_lon_1e7 = 284199000;
 }

bool purrgo_config_load(void) {
    purrgo_file_t* file = purrgo_fs_open(CONFIG_FILENAME, FS_READ);
    
    if (!file) {
        // Если файла нет, инициализируем дефолтные значения и сразу создаем файл
        purrgo_config_init();
        purrgo_config_save();
        return false;
    }

    // Стековый буфер. 256 байт достаточно для базовой конфигурации.
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
        return false;
    }

    char* line = buf;
    while (*line) {
        char* end = line;
        
        // Поиск символа завершения текущей строки (CR или LF)
        while (*end && *end != '\n' && *end != '\r') end++;
        bool is_eof = (*end == '\0');
        *end = '\0'; // Замена переноса на терминатор для изоляции строки
        
        // Поиск знака равенства (разделителя ключа и значения)
        char* delim = strchr(line, '=');
        if (delim) {
            *delim = '\0'; // Разделение строки на две подстроки
            char* key = line;
            char* val = delim + 1;
            
            // Маппинг строковых ключей на поля структуры
            if (strcmp(key, "TZ") == 0) {
                // Пользователь задает TZ в часах (например, TZ=+3). Конвертируем в минуты.
                app_config.tz_offset_minutes = (int16_t)(parse_int32(val) * 60);
            } else if (strcmp(key, "MAP_DIR") == 0) {
                strncpy(app_config.map_dir, val, sizeof(app_config.map_dir) - 1);
                app_config.map_dir[sizeof(app_config.map_dir) - 1] = '\0';
            } else if (strcmp(key, "LAST_LAT_1E7") == 0) {
                app_config.last_lat_1e7 = parse_int32(val);
            } else if (strcmp(key, "LAST_LON_1E7") == 0) {
                app_config.last_lon_1e7 = parse_int32(val);
            }
        }
        
        if (is_eof) break;
        line = end + 1;
        
        // Пропуск пустых строк и символов CRLF между строками
        while (*line == '\n' || *line == '\r') line++; 
    }
    return true;
}

bool purrgo_config_save(void) {
	fprintf(stderr, "Saving configuration to: %s\n", CONFIG_FILENAME);
    purrgo_file_t* file = purrgo_fs_open(CONFIG_FILENAME, FS_WRITE_CREATE);
	
    if (!file) {
    perror("purrgo_config_save: fopen");
    return false;
}
    
    char buf[CONFIG_MAX_SIZE];
    
    // Трансляция минутного интервала обратно в часы. 
    // Для дробных поясов (UTC+3:30) в будущем потребуется сменить ключ на TZ_MIN.
    int16_t tz_h = app_config.tz_offset_minutes / 60;

    // Формирование текстового блока настроек. 
    // Применение (int) каста гарантирует совместимость %d с 32-битными типами.
    int len = snprintf(buf, sizeof(buf), 
        "TZ=%+d\n"
        "MAP_DIR=%s\n"
        "LAST_LAT_1E7=%d\n"
        "LAST_LON_1E7=%d\n",
        tz_h,
        app_config.map_dir,
        (int)app_config.last_lat_1e7,
        (int)app_config.last_lon_1e7
    );
    
    /*
     * snprintf() returns the number of characters that would have
     * been written, excluding the terminating NUL.
     *
     * Therefore len >= sizeof(buf) means that the configuration
     * was truncated and must not be written.
     */
    if (len < 0 || (size_t)len >= sizeof(buf)) {
        purrgo_fs_close(file);
        return false;
     }

    /*
     * Verify that the complete configuration was actually written.
     *
     * purrgo_fs_write() returns the number of bytes written.
     * Previously this result was ignored, so purrgo_config_save()
     * reported success even when the filesystem write failed.
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