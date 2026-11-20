#ifndef PURRGO_APP_DATA_CHANGE_H
#define PURRGO_APP_DATA_CHANGE_H

#include <stdbool.h>
#include <stdint.h>

#include "purrgo/gnss_types.h"
#include "purrgo/waypoint.h"

/*
 * "Заметные изменения данных": что именно из данных GNSS и навигации считается
 * поводом перерисовать экран. На e-ink каждый проход - обновление панели, поэтому
 * экран не перерисовывается из-за шума в последнем знаке: изменение учитывается,
 * только когда значение ушло от показанного на пороговую величину. Здесь же
 * хранятся "показанные" значения (shown_*) - те, что были отправлены на экран в
 * момент постановки перерисовки в очередь. Сама очередь (когда перерисовывать) -
 * purrgo/ui/ui_dirty.h; app_fsm.c только склеивает то и другое.
 *
 * Модуль не обращается к часам, дисплею и глобальным данным: решение зависит
 * только от переданных значений, поэтому его проверяют host-тесты
 * (tests/host/test_app_data_change.c).
 */

/* Смещение позиции, при котором маркер на карте/на экране считается сдвинувшимся. */
#define PURRGO_GNSS_MARKER_UPDATE_DISTANCE_M 5

/* Пороги заметного изменения на экране Trip computer. */
#define PURRGO_UI_SPEED_THRESHOLD_KNOTS_100 100   /* 1 уз ~ 1,85 км/ч */
#define PURRGO_UI_ALT_THRESHOLD_M           5
/* Цель навигации: заметное изменение расстояния и пеленга. */
#define PURRGO_UI_WPT_DIST_THRESHOLD_M      5
#define PURRGO_UI_WPT_BEARING_THRESHOLD_DEG 5

typedef struct {
    /* Значения, отправленные на экран (purrgo_app_data_change_commit). */
    int32_t speed_knots_100;
    int32_t alt_m;
    uint8_t satellites;
    bool pos_valid;
    int32_t lat_1e7;
    int32_t lon_1e7;

    /* Цель навигации, как она была показана на Trip computer. */
    bool wpt_valid;
    bool wpt_arrived;
    uint32_t wpt_dist_m;
    uint16_t wpt_bearing_deg;

    /* Позиция, на которой реально нарисован маркер на карте (purrgo_app_notify_marker_rendered). */
    bool marker_valid;
    int32_t marker_lat_1e7;
    int32_t marker_lon_1e7;
} purrgo_app_data_change_t;

/*
 * Начало работы приложения: показанной позиции нет. Позиция маркера НЕ сбрасывается
 * намеренно: она отражает то, что реально нарисовано на панели, и переживает
 * повторную инициализацию приложения (как и состояние логгера).
 */
void purrgo_app_data_change_reset(purrgo_app_data_change_t *dc);

/* Показанная позиция недействительна (смена режима, экран не показывал позицию). */
void purrgo_app_data_change_forget_pos(purrgo_app_data_change_t *dc);

/* Маркер на карте больше не нарисован (экран перерисован без него). */
void purrgo_app_data_change_forget_marker(purrgo_app_data_change_t *dc);

/*
 * Маркер нарисован в позиции rendered_fix. NULL или невалидное решение означает,
 * что маркера на карте нет.
 */
void purrgo_app_data_change_marker_rendered(purrgo_app_data_change_t *dc,
                                            const purrgo_gnss_solution_t *rendered_fix);

/*
 * Позиция заметно сместилась относительно эталона (>= PURRGO_GNSS_MARKER_UPDATE_DISTANCE_M)
 * либо пропала/появилась. Эталон: на карте (on_map) - позиция, на которой реально
 * нарисован маркер; на остальных экранах маркер не рисуется, поэтому эталоном служит
 * позиция, уже отправленная в очередь на вывод. Иначе после ухода с карты расхождение
 * с последним маркером оставалось бы "значительным" вечно и вызывало перерисовку на
 * каждом интервале.
 */
bool purrgo_app_data_change_pos_moved(const purrgo_app_data_change_t *dc, bool on_map,
                                      const purrgo_gnss_solution_t *fix);

/*
 * Скорость, высота, число спутников или цель навигации заметно изменились (экран
 * Trip computer). Побочный эффект: если изменилась цель навигации, её показанные
 * значения запоминаются здесь же (остальные - только в _commit, когда перерисовка
 * действительно поставлена в очередь). wn - результат purrgo_wpt_nav().
 */
bool purrgo_app_data_change_trip_changed(purrgo_app_data_change_t *dc,
                                         const purrgo_gnss_solution_t *fix,
                                         const purrgo_wpt_nav_t *wn);

/* Перерисовка поставлена в очередь: запомнить отправленные на экран значения. */
void purrgo_app_data_change_commit(purrgo_app_data_change_t *dc,
                                   const purrgo_gnss_solution_t *fix);

#endif /* PURRGO_APP_DATA_CHANGE_H */
