#ifndef PURRGO_CONFIG_H
#define PURRGO_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "purrgo/track_logger.h"


/*
 * Режим отображения POI.
 */
typedef enum {
    PURRGO_POI_MODE_NO = 0,
    PURRGO_POI_MODE_CIRCLES,
    PURRGO_POI_MODE_ICONS
} purrgo_poi_mode_t;


/*
 * Режим отображения подписей POI.
 *
 * PURRGO_POI_LABELS_ALL:
 *     показывать подписи всех именованных POI.
 *
 * PURRGO_POI_LABELS_IMPORTANT:
 *     показывать только подписи важных POI.
 *
 * PURRGO_POI_LABELS_OFF:
 *     подписи POI отключены.
 *
 * На текущем этапе сам механизм отрисовки текста POI ещё не
 * реализуется. Значение настройки уже хранится в конфигурации,
 * чтобы позднее добавить рендеринг подписей без изменения
 * интерфейса настроек.
 */
typedef enum {
    PURRGO_POI_LABELS_ALL = 0,
    PURRGO_POI_LABELS_IMPORTANT,
    PURRGO_POI_LABELS_OFF
} purrgo_poi_label_mode_t;


/*
 * Глобальная структура настроек устройства.
 */
typedef struct {
    int16_t tz_offset_minutes;
    track_logger_mode_t log_mode;
    bool backlight_on;

    /*
     * Директория выбранной карты.
     */
    char map_dir[128];

    /*
     * Последняя широта для центрирования при холодном старте.
     */
    int32_t last_lat_1e7;

    /*
     * Последняя долгота для центрирования при холодном старте.
     */
    int32_t last_lon_1e7;

    /*
     * Режим отображения POI.
     */
    purrgo_poi_mode_t poi_mode;

    /*
     * Режим отображения подписей POI.
     *
     * На текущем этапе используется только настройками.
     * Сам текстовый renderer POI будет подключён отдельно.
     */
    purrgo_poi_label_mode_t poi_label_mode;

    /*
     * Включение отображения текущего трека на карте.
     */
    bool track_display_enabled;

} purrgo_config_t;


extern purrgo_config_t app_config;


void purrgo_config_init(void);


/*
 * Функции управления файлом конфигурации.
 */
bool purrgo_config_load(void);
bool purrgo_config_save(void);


#endif /* PURRGO_CONFIG_H */