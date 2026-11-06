#ifndef PURRGO_WPT_LAYER_H
#define PURRGO_WPT_LAYER_H

/*
 * Слой карты Waypoints (PURRGO_LAYER_WAYPOINT): путевые точки <wpt> на карте.
 *
 * Источники точек (в таком порядке заполнения):
 *   1. выбранный маршрут (app_config.route_file, каталог ROUTES);
 *   2. все файлы *.gpx каталога WAYPTS, в порядке перечисления каталога.
 * Каждая точка рисуется иконкой PURRGO_WPT_LAYER_ICON из purrgo_poi_icons.
 *
 * Одновременно хранится и отображается не более PURRGO_WPT_LAYER_MAX точек;
 * остальные отбрасываются (первыми остаются точки маршрута). Подписи не
 * рисуются. Это не цель навигации (purrgo_wpt_*): цель рисуется отдельным
 * символом поверх этого слоя.
 *
 * Данные читаются с SD один раз и держатся в RAM; повторная загрузка - при
 * смене маршрута либо если прошлая попытка не удалась (SD ещё не готова).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "purrgo/gfx_renderer.h"
#include "purrgo/map.h"
#include "purrgo/navigation.h"

/* Максимум одновременно отображаемых путевых точек (маршрут + WAYPTS вместе). */
#ifndef PURRGO_WPT_LAYER_MAX
#define PURRGO_WPT_LAYER_MAX 64
#endif

/* Номер иконки в purrgo_poi_icons (src/core/purrgo_poi_icons.c). */
#ifndef PURRGO_WPT_LAYER_ICON
#define PURRGO_WPT_LAYER_ICON 46
#endif

/*
 * Приводит набор точек в соответствие с настройкой: загружает <wpt> из файла
 * route_file (пустое имя - маршрута нет) и из каталога WAYPTS. Если набор уже
 * загружен для этого имени, ничего не делает. Возвращает true, если есть что
 * рисовать.
 */
bool purrgo_wpt_layer_sync(const char *route_file);

/* Забыть загруженные точки (следующий sync прочитает всё заново). */
void purrgo_wpt_layer_clear(void);

size_t purrgo_wpt_layer_count(void);
const purrgo_waypoint_t *purrgo_wpt_layer_get(size_t index);

/* Точек было больше PURRGO_WPT_LAYER_MAX: лишние отброшены. */
bool purrgo_wpt_layer_truncated(void);

/*
 * Рисует иконки в текущем clip-окне gfx. Слой выключается вызывающим кодом
 * (PURRGO_LAYER_WAYPOINT). Точки вне видимой области пропускаются.
 */
void purrgo_wpt_layer_render(gfx_context_t *gfx, const purrgo_bbox_t *camera,
                             const purrgo_viewport_t *vp);

#endif /* PURRGO_WPT_LAYER_H */
