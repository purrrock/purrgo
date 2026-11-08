#ifndef PURRGO_WAYPOINT_H
#define PURRGO_WAYPOINT_H

/*
 * Навигация к путевой точке (Waypoint).
 *
 * Цель одна. Она хранится в настройках (app_config.wpt_*, PURRGO.CFG) и
 * переживает выключение. Задаётся в меню WAYPOINT одним из способов:
 *   - HERE       - текущая позиция GNSS (отметить место: стоянка, машина);
 *   - MAP CENTER - центр карты на экране (в ручном режиме карты там рисуется
 *                  перекрестие). Точка дополнительно записывается отдельным
 *                  GPX-файлом в каталог WAYPTS (purrgo_wpt_save_to_waypts());
 *   - из файла   - точка <wpt> GPX-файла, выбранного как маршрут (ROUTE).
 *                  Отдельный каталог не нужен: GPX из планировщиков обычно
 *                  содержит и трек, и <wpt>. Файл только с <wpt> тоже подходит:
 *                  выберите его как ROUTE, линия просто не будет нарисована.
 *   - (none)     - снять цель.
 * На SD ядро записывает только цель (PURRGO.CFG) и точки MAP CENTER (WAYPTS).
 *
 * Расстояние, пеленг и признак прибытия пересчитываются раз в секунду
 * (purrgo_wpt_update(), из purrgo_app_update()). Выводятся на экране Trip
 * computer; на карте цель отмечена иконкой PURRGO_WPT_TARGET_ICON (purrgo_wpt_render()).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "purrgo/gfx_renderer.h"
#include "purrgo/gnss_types.h"
#include "purrgo/map.h"
#include "purrgo/navigation.h"

/* Сколько точек читается из GPX-файла (остальные отбрасываются). */
#ifndef PURRGO_WPT_LIST_MAX
#define PURRGO_WPT_LIST_MAX 64
#endif

/* Номер иконки цели навигации в purrgo_poi_icons (src/core/purrgo_poi_icons.c). */
#ifndef PURRGO_WPT_TARGET_ICON
#define PURRGO_WPT_TARGET_ICON 35
#endif

/* Радиус прибытия, м. */
#ifndef PURRGO_WPT_ARRIVAL_RADIUS_M
#define PURRGO_WPT_ARRIVAL_RADIUS_M 15U
#endif

/*
 * Ниже этой скорости (узлы * 100) курс по RMC ненадёжен: стрелка на экране
 * показывает пеленг от севера, а не относительно направления движения.
 */
#ifndef PURRGO_WPT_COURSE_MIN_SPEED_KNOTS_100
#define PURRGO_WPT_COURSE_MIN_SPEED_KNOTS_100 100
#endif

/* ------------------------------------------------------------------ */
/*  Цель                                                                */
/* ------------------------------------------------------------------ */

bool purrgo_wpt_is_set(void);

/* Цель или NULL, если не задана. Указатель действителен до следующего вызова set/clear. */
const purrgo_waypoint_t *purrgo_wpt_target(void);

/*
 * Задать цель (координаты 1e-7 градуса, имя UTF-8; NULL или "" - имя "WPT").
 * Только RAM и app_config: сохранение в PURRGO.CFG выполняет вызывающий
 * (purrgo_config_save()). Возвращает false при координатах вне диапазона.
 */
bool purrgo_wpt_set(int32_t lat_1e7, int32_t lon_1e7, const char *name);

void purrgo_wpt_clear(void);

/*
 * Записывает точку отдельным GPX-файлом MPnnnnnn.GPX в каталог WAYPTS (формат 8.3;
 * nnnnnn - следующий свободный номер после самого большого из имеющихся). Файл
 * содержит один <wpt> с именем "MPn" (n - тот же номер без ведущих нулей); слой
 * Waypoints подхватит его при следующей загрузке (см. purrgo_wpt_layer_clear()).
 * name_out (может быть NULL) получает имя точки. false - координаты вне диапазона,
 * каталог недоступен, нет свободного номера или сбой записи (имя в name_out
 * тогда не определено). Цель навигации не меняет.
 */
bool purrgo_wpt_save_to_waypts(int32_t lat_1e7, int32_t lon_1e7,
                               char *name_out, size_t name_size);

/* ------------------------------------------------------------------ */
/*  Расстояние и направление                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    bool valid;                 /* цель задана и есть фикс: поля nav актуальны */
    purrgo_nav_status_t nav;
} purrgo_wpt_nav_t;

/* Пересчитать по текущему решению GNSS. */
void purrgo_wpt_update(const purrgo_gnss_solution_t *fix);

const purrgo_wpt_nav_t *purrgo_wpt_nav(void);

/*
 * Угол стрелки на цель, градусы по часовой от "вверх" экрана. true - угол
 * отсчитан от направления движения (курс надёжен), false - от севера.
 */
bool purrgo_wpt_arrow_angle(const purrgo_gnss_solution_t *fix, uint16_t bearing_deg,
                            uint16_t *angle_deg_out);

typedef struct {
    int16_t x, y;
} purrgo_wpt_pt_t;

/*
 * Точки стрелки радиуса r с центром (cx, cy), повёрнутой на angle_deg по часовой:
 * pts[0] - хвост, pts[1] - остриё, pts[2], pts[3] - концы наконечника. Рисуются
 * отрезками хвост-остриё, остриё-pts[2], остриё-pts[3].
 */
void purrgo_wpt_arrow_points(int16_t cx, int16_t cy, int16_t r, uint16_t angle_deg,
                             purrgo_wpt_pt_t pts[4]);

/*
 * Точка на расстоянии r от (cx, cy) в направлении angle_deg (градусы по часовой
 * от "вверх" экрана, y вниз). Нужна для засечек и подписей циферблата компаса.
 */
void purrgo_wpt_polar(int16_t cx, int16_t cy, int16_t r, uint16_t angle_deg,
                      purrgo_wpt_pt_t *out);

/* "845 m", "1.23 km", "12.3 km", "123 km". */
void purrgo_wpt_format_distance(char *buf, size_t size, uint32_t meters);

/* Восемь румбов: "N", "NE", "E", ... */
const char *purrgo_wpt_cardinal(uint16_t bearing_deg);

/* ------------------------------------------------------------------ */
/*  Список точек из GPX-файла                                           */
/* ------------------------------------------------------------------ */

/*
 * Читает <wpt> из GPX-файла filename (только имя, без пути) каталога ROUTES.
 * Пустое или неверное имя очищает список. Точки без имени получают "WPT n".
 * Возвращает число точек в списке.
 */
size_t purrgo_wpt_list_load(const char *filename);

size_t purrgo_wpt_list_count(void);
const purrgo_waypoint_t *purrgo_wpt_list_get(size_t index);

/* В файле было больше точек, чем PURRGO_WPT_LIST_MAX. */
bool purrgo_wpt_list_truncated(void);

/* ------------------------------------------------------------------ */
/*  Карта                                                               */
/* ------------------------------------------------------------------ */

/*
 * Рисует иконку цели PURRGO_WPT_TARGET_ICON (булавка) в текущем clip-окне gfx:
 * остриё иконки - в точке цели. Вне видимой области ничего не рисуется. Цель не
 * задана - ничего не делает.
 */
void purrgo_wpt_render(gfx_context_t *gfx, const purrgo_bbox_t *camera,
                       const purrgo_viewport_t *vp);

#endif /* PURRGO_WAYPOINT_H */
