#ifndef PURRGO_MAP_CONTROLLER_H
#define PURRGO_MAP_CONTROLLER_H

#include "purrgo/gnss_types.h"
#include "purrgo/app_fsm.h"
#include <stdbool.h>
#include <stdint.h>

void purrgo_map_controller_init(void);
void purrgo_map_controller_update(const purrgo_gnss_solution_t* current_fix);
bool purrgo_map_controller_handle_button(purrgo_action_t button);

/*
 * Показать прямоугольник (границы маршрута или трека, 1e-7 градуса) целиком:
 * центр карты - центр прямоугольника, масштаб - наименьший из таблицы, при
 * котором прямоугольник помещается в окно карты с полями. Включает ручной
 * режим (автоследование за GNSS выключено до долгого K3 CENTER). Прямоугольник
 * не должен пересекать меридиан 180 градусов (min_lon <= max_lon). Если
 * прямоугольник больше самого крупного масштаба, ставится самый крупный.
 */
void purrgo_map_controller_fit_bounds(int32_t min_lat_1e7, int32_t min_lon_1e7,
                                      int32_t max_lat_1e7, int32_t max_lon_1e7);

/*
 * Поставить центр карты в точку (1e-7 градуса), масштаб не менять. Включает ручной режим
 * (автоследование за GNSS выключено до долгого K3 CENTER): в центре окна остаётся перекрестие,
 * с которого WAYPOINT > MAP CENTER делает точку целью. Точка вне допустимых координат - без изменений.
 */
void purrgo_map_controller_center_on(int32_t lat_1e7, int32_t lon_1e7);

// COMPATIBILITY API DECLARED IN APP_FSM.H
int32_t map_app_get_map_center_lat(void);
int32_t map_app_get_map_center_lon(void);
purrgo_map_scale_t map_app_get_map_zoom_level(void);
uint32_t map_app_get_map_scale_width_m(void);
const char* map_app_get_map_scale_label(void);
bool map_app_is_manual_pan_active(void);

#endif // PURRGO_MAP_CONTROLLER_H
