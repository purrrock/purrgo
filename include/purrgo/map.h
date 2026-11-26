// file: include/purrgo/map.h
#ifndef PURRGO_MAP_H
#define PURRGO_MAP_H

#include <stdint.h>
#include <stdbool.h>

#include "purrgo/gfx_renderer.h"
#include "purrgo/fs_hal.h"
#include "purrgo/map_types.h"

typedef struct {
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;
} purrgo_bbox_t;

typedef struct {
    uint16_t width;
    uint16_t height;

    int16_t offset_x;
    int16_t offset_y;
} purrgo_viewport_t;

typedef enum {
    MAP_LAYER_LINES = 0,
    MAP_LAYER_POLYGONS,
    MAP_LAYER_POIS
} purrgo_map_layer_t;

/*
 * Отрисовать один слой. idx - индекс слоя (обязателен), mlp - геометрия
 * (обязательна для линий и полигонов), db - подписи (может быть NULL).
 * Файлы открыты вызывающим на чтение; позиция в них не важна.
 * true - слой отрисован целиком; false - ошибка заголовков, чтения или разбора.
 */
bool purrgo_map_render_layer(
    purrgo_file_t *idx_file,
    purrgo_file_t *mlp_file,
    purrgo_file_t *db_file,
    gfx_context_t *gfx,
    const purrgo_bbox_t *camera,
    const purrgo_viewport_t *viewport,
    const purrgo_map_render_params_t *params,
    purrgo_map_layer_t layer_type
);

/* Отрисовать карту каталога params->map_dir со слоями и масштабом из params. */
bool purrgo_map_render_viewport(
    gfx_context_t *gfx,
    const purrgo_viewport_t *viewport,
    const purrgo_bbox_t *camera,
    const purrgo_map_render_params_t *params
);

/*
 * Файлы слоя POI каталога map_dir (pois.idx и pois.db) для поиска объектов. Это те же открытые
 * файлы, что использует отрисовка слоя POI: отдельных слотов файловой системы поиск не занимает
 * и пользуется общим кэшем секторов. Файлы принадлежат карте - вызывающий их не закрывает и не
 * хранит указатель дольше текущей операции (смена каталога карты, выключение слоя POI в
 * настройках или purrgo_map_shutdown() закрывают их).
 * Возвращает false, если pois.idx нет или каталог недопустим. *db_out может быть NULL
 * (pois.db не открылся) - тогда названий нет, но координаты искать можно.
 */
bool purrgo_map_poi_files(const char *map_dir, purrgo_file_t **idx_out, purrgo_file_t **db_out);

void purrgo_map_shutdown(void);

/*
 * Через сколько мс без отрисовки карты и поиска POI файлы слоёв отпускаются
 * (purrgo_map_release_idle()). Открытые файлы не дают карте SD парковаться по тайм-ауту,
 * а карта на стоянке, режим путевого компьютера или меню файлы не используют.
 */
#ifndef PURRGO_MAP_IDLE_RELEASE_MS
#define PURRGO_MAP_IDLE_RELEASE_MS 20000u
#endif

/*
 * Закрыть файлы слоёв, если карта не использовалась idle_ms. Вызывать из главного цикла
 * (now_ms - purrgo_system_time_ms()). Экран не перерисовывается: следующий кадр карты сам
 * откроет нужные слои заново, это дороже кадра с открытыми файлами (открытие, таблицы CLMT,
 * холодный кэш секторов и, если карта SD припаркована, её включение).
 * @return true, если файлы были закрыты.
 */
bool purrgo_map_release_idle(uint32_t now_ms, uint32_t idle_ms);

#endif /* PURRGO_MAP_H */